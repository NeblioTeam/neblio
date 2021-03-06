#include "ntp1wallet.h"

// the following is a necessary include for pwalletMain and CWalletTx objects
#include "init.h"
#include "main.h"
#include "txmempool.h"

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread.hpp>

const std::string NTP1Wallet::ICON_ERROR_CONTENT = "<DownloadError>";

NTP1Wallet::NTP1Wallet()
{
    lastTxCount                  = 0;
    lastOutputsCount             = 0;
    updateBalance                = false;
    everSucceededInLoadingTokens = false;
}

void NTP1Wallet::update()
{
    __getOutputs();
    if (updateBalance) {
        __RecalculateTokensBalances();
        everSucceededInLoadingTokens = true;
    }
}

bool NTP1Wallet::getRetrieveFullMetadata() const { return retrieveFullMetadata; }

void NTP1Wallet::setRetrieveFullMetadata(bool value) { retrieveFullMetadata = value; }

std::map<std::string, NTP1Int> NTP1Wallet::getBalances() const { return balances; }

const std::unordered_map<std::string, NTP1TokenMetaData>& NTP1Wallet::getTokenMetadataMap() const
{
    return tokenInformation;
}

void NTP1Wallet::__getOutputs()
{
    // this helps in persisting to get the wallet data when the application is launched for the first
    // time and nebl wallet is null still the 100 number is just a protection against infinite waiting

    std::shared_ptr<CWallet> localWallet = std::atomic_load(&pwalletMain);

    if (!appInitiated) {
        return;
    }

#ifdef QT_GUI
    for (int i = 0;
         i < 100 && ((!everSucceededInLoadingTokens && std::atomic_load(&localWallet) == nullptr));
         i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif

    if (std::atomic_load(&localWallet) == nullptr) {
        return;
    }

    const CTxDB txdb;

    const uint256 bestBlockHash = txdb.GetBestBlockHash();

    std::vector<COutput> vecOutputs;
    std::atomic_load(&localWallet)->AvailableCoins(txdb, vecOutputs);

    // remove outputs that are outside confirmation bounds
    auto outputToRemoveIt = std::remove_if(
        vecOutputs.begin(), vecOutputs.end(), [this, &txdb, &bestBlockHash](const COutput& output) {
            if (maxConfirmations >= 0 &&
                output.tx->GetDepthInMainChain(txdb, bestBlockHash) > maxConfirmations) {
                return true;
            }
            if (minConfirmations >= 0 &&
                output.tx->GetDepthInMainChain(txdb, bestBlockHash) < minConfirmations) {
                return true;
            }
            return false;
        });
    vecOutputs.erase(outputToRemoveIt, vecOutputs.end());

    int64_t currTxCount      = 0;
    int64_t currOutputsCount = 0;
    {
        LOCK2(cs_main, std::atomic_load(&localWallet)->cs_wallet);

        currTxCount      = static_cast<int64_t>(std::atomic_load(&localWallet)->mapWallet.size());
        currOutputsCount = static_cast<int64_t>(vecOutputs.size());

        // if no new outputs are available
        if (lastTxCount == currTxCount && lastOutputsCount == currOutputsCount) {
            return;
        }
    }

    updateBalance = true;

    int failedRetrievals = 0;

    for (unsigned long i = 0; i < vecOutputs.size(); i++) {
        NTP1OutPoint output = ConvertNeblOutputToNTP1(vecOutputs[i]);
        uint256      txHash = output.getHash();

        // get the transaction from the wallet
        CWalletTx neblTx;
        if (!std::atomic_load(&localWallet)->GetTransaction(txHash, neblTx)) {
            NLog.write(
                b_sev::err,
                "Error: Although the output number {} of transaction {} belongs to you, it couldn't "
                "be found in your wallet.",
                vecOutputs[i].i, txHash.ToString());
            continue;
        }

        // NTP1 transactions strictly contain OP_RETURN in one of their vouts
        std::string opReturnArg;
        if (!NTP1Transaction::IsTxNTP1(&neblTx, &opReturnArg)) {
            continue;
        }

        // if output already exists, check if it's spent, if it's remove it
        if (removeOutputIfSpent(output, neblTx, bestBlockHash, txdb))
            continue;

        NTP1Transaction ntp1tx;
        try {
            std::vector<std::pair<CTransaction, NTP1Transaction>> prevTxs =
                NTP1Transaction::GetAllNTP1InputsOfTx(neblTx, txdb, true);
            ntp1tx.readNTP1DataFromTx(txdb, neblTx, prevTxs);
        } catch (std::exception& ex) {
            NLog.write(b_sev::err, "Unable to download transaction information. Error says: {}",
                       ex.what());
            failedRetrievals++;
            continue;
        }

        // include only NTP1 transactions
        if (ntp1tx.getTxOut(output.getIndex()).tokenCount() > 0) {
            try {
                // transaction with output index
                walletOutputsWithTokens[output] = ntp1tx;
                for (long j = 0; j < static_cast<long>(ntp1tx.getTxOut(output.getIndex()).tokenCount());
                     j++) {

                    NTP1TokenTxData tokenTx = ntp1tx.getTxOut(output.getIndex()).getToken(j);

                    // find issue transaction to get meta data from
                    uint256      issueTxid = tokenTx.getIssueTxId();
                    CTransaction issueTx;
                    try {
                        issueTx = CTransaction::FetchTxFromDisk(issueTxid);
                    } catch (const std::exception& ex) {
                        if (!mempool.lookup(issueTxid, issueTx)) {
                            throw std::runtime_error(
                                "Transaction not found on disk or mempool. Disk search error: " +
                                std::string(ex.what()));
                        }
                    }

                    std::vector<std::pair<CTransaction, NTP1Transaction>> issueTxInputs =
                        NTP1Transaction::GetAllNTP1InputsOfTx(issueTx, txdb, true);
                    NTP1Transaction issueNTP1Tx;
                    issueNTP1Tx.readNTP1DataFromTx(txdb, issueTx, issueTxInputs);

                    // find the correct output in the issuance transaction that has the token in question
                    // issued
                    int  relevantIssueOutputIndex = -1;
                    bool stop                     = false;
                    for (int k = 0; k < (int)issueNTP1Tx.getTxOutCount(); k++) {
                        for (int l = 0; l < (int)issueNTP1Tx.getTxOut(k).tokenCount(); l++) {
                            if (issueNTP1Tx.getTxOut(k).getToken(l).getTokenId() ==
                                tokenTx.getTokenId()) {
                                relevantIssueOutputIndex = k;
                                stop                     = true;
                                break;
                            }
                        }
                        if (stop) {
                            break;
                        }
                    }

                    if (relevantIssueOutputIndex < 0) {
                        throw std::runtime_error("Could not find the correct output index for token: " +
                                                 tokenTx.getTokenId());
                    }

                    if (retrieveFullMetadata) {
                        try {
                            tokenInformation[tokenTx.getTokenId()] =
                                NTP1Transaction::GetFullNTP1IssuanceMetadata(txdb, issueTxid);
                        } catch (std::exception& ex) {
                            NLog.write(b_sev::err, "Failed to retrieve NTP1 token metadata. Error: {}",
                                       ex.what());
                            tokenInformation[tokenTx.getTokenId()] =
                                GetMinimalMetadataInfoFromTxData(tokenTx);
                        } catch (...) {
                            NLog.write(b_sev::err,
                                       "Failed to retrieve NTP1 token metadata. Unknown exception.");
                            tokenInformation[tokenTx.getTokenId()] =
                                GetMinimalMetadataInfoFromTxData(tokenTx);
                        }
                    } else {
                        // no metadata available, set the name manually
                        tokenInformation[tokenTx.getTokenId()] =
                            GetMinimalMetadataInfoFromTxData(tokenTx);
                    }
                }
            } catch (std::exception& ex) {
                NLog.write(b_sev::err, "Unable to download token metadata. Error says: {}", ex.what());
                continue;
            }
        }
    }

    scanSpentTransactions(txdb, bestBlockHash);

    lastTxCount      = currTxCount - failedRetrievals;
    lastOutputsCount = currOutputsCount - failedRetrievals;
}

void NTP1Wallet::__RecalculateTokensBalances()
{
    balances.clear();
    for (std::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator it =
             walletOutputsWithTokens.begin();
         it != walletOutputsWithTokens.end(); it++) {
        const NTP1Transaction& tx          = it->second;
        int                    outputIndex = it->first.getIndex();
        AddOutputToWalletBalance(tx, outputIndex, this->balances);
    }
    updateBalance = false;
}

void NTP1Wallet::AddOutputToWalletBalance(const NTP1Transaction& tx, int outputIndex,
                                          std::map<std::string, NTP1Int>& balancesTable)
{
    for (long j = 0; j < static_cast<long>(tx.getTxOut(outputIndex).tokenCount()); j++) {
        NTP1TokenTxData    tokenTx = tx.getTxOut(outputIndex).getToken(j);
        const std::string& tokenID = tokenTx.getTokenId();
        if (balancesTable.find(tokenID) == balancesTable.end()) {
            balancesTable[tokenID] = tokenTx.getAmount();
        } else {
            balancesTable[tokenID] += tokenTx.getAmount();
        }
    }
}

bool NTP1Wallet::removeOutputIfSpent(const NTP1OutPoint& output, const CWalletTx& neblTx,
                                     const uint256& bestBlockHash, const ITxDB& txdb)
{
    std::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator outputIt =
        walletOutputsWithTokens.find(output);
    if (outputIt != walletOutputsWithTokens.end()) {
        if (pwalletMain->IsSpent(neblTx.GetHash(), output.getIndex(), txdb, bestBlockHash)) {
            walletOutputsWithTokens.erase(outputIt);
        }
        return true;
    }
    return false;
}

void NTP1Wallet::scanSpentTransactions(const ITxDB& txdb, const uint256& bestBlockHash)
{
    std::shared_ptr<CWallet> localWallet = std::atomic_load(&pwalletMain);
    if (localWallet == nullptr)
        return;
    std::deque<NTP1OutPoint> toRemove;
    for (std::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator it =
             walletOutputsWithTokens.begin();
         it != walletOutputsWithTokens.end(); it++) {
        CWalletTx      neblTx;
        int            outputIndex = it->first.getIndex();
        const uint256& txHash      = it->first.getHash();
        if (!localWallet->GetTransaction(txHash, neblTx))
            continue;
        if (pwalletMain->IsSpent(neblTx.GetHash(), outputIndex, txdb, bestBlockHash)) {
            // this, although the right way to do things, causes a crash. A safer plan is chosen
            // it = walletOutputsWithTokens.erase(it);
            toRemove.push_back(it->first);
        }
    }
    for (unsigned long i = 0; i < toRemove.size(); i++) {
        if (walletOutputsWithTokens.find(toRemove[i]) != walletOutputsWithTokens.end()) {
            walletOutputsWithTokens.erase(toRemove[i]);
        } else {
            NLog.write(b_sev::err,
                       "Unable to find output {}:{}, although it was found before and marked for "
                       "removal.",
                       toRemove[i].getHash().ToString(), ToString(toRemove[i].getIndex()));
            //            std::cerr<<"Unable to find output " << toRemove[i].getHash().ToString() <<
            //            ":"
            //            << ToString(toRemove[i].getIndex()) << ", although it was found before and
            //            marked for removal." << std::endl;
        }
    }
}

NTP1OutPoint NTP1Wallet::ConvertNeblOutputToNTP1(const COutput& output)
{
    return NTP1OutPoint(output.tx->GetHash(), output.i);
}

NTP1TokenMetaData NTP1Wallet::GetMinimalMetadataInfoFromTxData(const NTP1TokenTxData& tokenTx)
{
    NTP1TokenMetaData res;
    res.setTokenName(tokenTx.getTokenSymbol());
    res.setTokenId(tokenTx.getTokenId());
    res.setIssuanceTxIdHex(tokenTx.getIssueTxId().ToString());
    return res;
}

std::string NTP1Wallet::getTokenName(const std::string& tokenID) const
{
    std::unordered_map<std::string, NTP1TokenMetaData>::const_iterator it =
        tokenInformation.find(tokenID);
    if (it == tokenInformation.end()) {
        return std::string("<NameError>");
    } else {
        return it->second.getTokenName();
    }
}

NTP1Int NTP1Wallet::getTokenBalance(const std::string& tokenID) const
{
    std::map<std::string, NTP1Int>::const_iterator it = balances.find(tokenID);
    if (it == balances.end()) {
        return 0;
    } else {
        return static_cast<int64_t>(it->second);
    }
}

std::string NTP1Wallet::getTokenNameFromIndex(int index) const
{
    std::map<std::string, NTP1Int>::const_iterator it = balances.begin();
    std::advance(it, index);
    std::unordered_map<std::string, NTP1TokenMetaData>::const_iterator itToken =
        tokenInformation.find(it->first);
    if (itToken == tokenInformation.end()) {
        return std::string("<NameError>");
    } else {
        return itToken->second.getTokenName();
    }
}

std::string NTP1Wallet::getTokenID(int index) const
{
    std::map<std::string, NTP1Int>::const_iterator it = balances.begin();
    std::advance(it, index);
    std::unordered_map<std::string, NTP1TokenMetaData>::const_iterator itToken =
        tokenInformation.find(it->first);
    if (itToken == tokenInformation.end()) {
        return std::string("<TokenIdError>");
    } else {
        return itToken->second.getTokenId();
    }
}

std::string NTP1Wallet::getTokenIssuanceTxid(int index) const
{
    std::map<std::string, NTP1Int>::const_iterator it = balances.begin();
    std::advance(it, index);
    std::unordered_map<std::string, NTP1TokenMetaData>::const_iterator itToken =
        tokenInformation.find(it->first);
    if (itToken == tokenInformation.end()) {
        return ""; // empty to detect error automatically (not viewable by user)
    } else {
        return itToken->second.getIssuanceTxId().ToString();
    }
}

std::string NTP1Wallet::getTokenDescription(int index) const
{
    std::map<std::string, NTP1Int>::const_iterator it = balances.begin();
    std::advance(it, index);
    std::unordered_map<std::string, NTP1TokenMetaData>::const_iterator itToken =
        tokenInformation.find(it->first);
    if (itToken == tokenInformation.end()) {
        return std::string("<DescError>");
    } else {
        return itToken->second.getTokenDescription();
    }
}

NTP1Int NTP1Wallet::getTokenBalance(int index) const
{
    if (index > getNumberOfTokens())
        return 0;
    std::map<std::string, NTP1Int>::const_iterator it = balances.begin();
    std::advance(it, index);
    if (it == balances.end()) {
        return 0;
    } else {
        return static_cast<int64_t>(it->second);
    }
}

std::string NTP1Wallet::__downloadIcon(const std::string& IconURL)
{
    try {
        return cURLTools::GetFileFromHTTPS(IconURL, 30, false);
    } catch (std::exception& ex) {
        NLog.write(b_sev::err, "Error: Failed at downloading icon from {}. Error says: {}", IconURL,
                   ex.what());
        return ICON_ERROR_CONTENT;
    }
}

void NTP1Wallet::__asyncDownloadAndSetIcon(std::string IconURL, std::string tokenId,
                                           NTP1WalletPtr wallet)
{
    wallet->setTokenIcon(tokenId, __downloadIcon(IconURL));
}

std::string NTP1Wallet::getAndCacheTokenIcon(int index)
{
    std::map<std::string, NTP1Int>::const_iterator it = balances.begin();
    std::advance(it, index);
    std::string                                                        tokenId = it->first;
    std::unordered_map<std::string, NTP1TokenMetaData>::const_iterator itToken =
        tokenInformation.find(tokenId);
    if (itToken == tokenInformation.end()) {
        return "";
    }
    if (!tokenIcons.exists(tokenId)) {
        const std::string& IconURL = itToken->second.getIconURL();
        if (IconURL.empty()) {
            // no icon URL provided, set empty icon and return
            tokenIcons.set(tokenId, "");
            return "";
        }
        boost::thread IconDownloadThread(
            boost::bind(__asyncDownloadAndSetIcon, IconURL, tokenId, shared_from_this()));
        IconDownloadThread.detach();
        return "";
    } else {
        std::string icon = tokenIcons.get(tokenId).value_or("");
        // if there was an error getting the icon OR the icon is empty, and a download URL now
        // exists, download again
        if (icon == ICON_ERROR_CONTENT || (icon == "" && !itToken->second.getIconURL().empty())) {
            const std::string& IconURL = itToken->second.getIconURL();
            boost::thread      IconDownloadThread(
                boost::bind(__asyncDownloadAndSetIcon, IconURL, tokenId, shared_from_this()));
            IconDownloadThread.detach();
        }

        icon = tokenIcons.get(tokenId).value_or("");
        return icon;
    }
}

int64_t NTP1Wallet::getNumberOfTokens() const { return balances.size(); }

const std::map<std::string, NTP1Int>& NTP1Wallet::getBalancesMap() const { return balances; }

const std::unordered_map<NTP1OutPoint, NTP1Transaction>& NTP1Wallet::getWalletOutputsWithTokens() const
{
    return walletOutputsWithTokens;
}

bool NTP1Wallet::IconHasErrorContent(const std::string& icon) { return icon == ICON_ERROR_CONTENT; }

void NTP1Wallet::clear()
{
    tokenInformation.clear();
    walletOutputsWithTokens.clear();
    tokenIcons.clear();
    balances.clear();
    lastTxCount      = 0;
    lastOutputsCount = 0;
}

void NTP1Wallet::setMinMaxConfirmations(int minConfs, int maxConfs)
{
    minConfirmations = minConfs;
    maxConfirmations = maxConfs;
}

void NTP1Wallet::setTokenIcon(const std::string& tokenId, const std::string& iconData)
{
    tokenIcons.set(tokenId, iconData);
}

std::string NTP1Wallet::Serialize(const NTP1Wallet& wallet)
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("token_info", SerializeMap(wallet.tokenInformation, false, false)));
    root.push_back(
        json_spirit::Pair("outputs", SerializeMap(wallet.walletOutputsWithTokens, false, false)));
    root.push_back(
        json_spirit::Pair("icons", SerializeMap(wallet.tokenIcons.getInternalMap(), false, true)));
    root.push_back(json_spirit::Pair("balances", SerializeMap(wallet.balances, false, false)));

    return json_spirit::write_formatted(root);
}

NTP1Wallet NTP1Wallet::Deserialize(const std::string& data)
{
    NTP1Wallet result;

    json_spirit::Value parsedData;
    json_spirit::read_or_throw(data, parsedData);

    json_spirit::Value tokenInfoData(NTP1Tools::GetObjectField(parsedData.get_obj(), "token_info"));
    result.tokenInformation =
        DeserializeMap<std::unordered_map<std::string, NTP1TokenMetaData>>(tokenInfoData, false, false);
    json_spirit::Value outputsData(NTP1Tools::GetObjectField(parsedData.get_obj(), "outputs"));
    result.walletOutputsWithTokens =
        DeserializeMap<std::unordered_map<NTP1OutPoint, NTP1Transaction>>(outputsData, false, false);
    json_spirit::Value iconsData(NTP1Tools::GetObjectField(parsedData.get_obj(), "icons"));
    result.tokenIcons.setInternalMap(
        DeserializeMap<std::unordered_map<std::string, std::string>>(iconsData, false, true));
    json_spirit::Value balancesData(NTP1Tools::GetObjectField(parsedData.get_obj(), "balances"));
    result.balances = DeserializeMap<std::map<std::string, NTP1Int>>(balancesData, false, false);

    return result;
}

void NTP1Wallet::exportToFile(const boost::filesystem::path& filePath) const
{
    std::string                output = Serialize(*this);
    boost::filesystem::fstream fileObj(filePath, std::ios::out);
    fileObj.write(output.c_str(), output.size());
    fileObj.close();
}

void NTP1Wallet::importFromFile(const boost::filesystem::path& filePath)
{
    boost::filesystem::fstream fileObj(filePath, std::ios::in);
    std::string data((std::istreambuf_iterator<char>(fileObj)), std::istreambuf_iterator<char>());
    fileObj.close();
    this->clear();
    *this = Deserialize(data);
}

std::string NTP1Wallet::__KeyToString(const std::string& str, bool serialize)
{
    if (serialize) {
        std::string res;
        boost::algorithm::hex(str.begin(), str.end(), std::back_inserter(res));
        return res;
    } else {
        return str;
    }
}

void NTP1Wallet::__KeyFromString(const std::string& str, bool deserialize, std::string& result)
{
    if (deserialize) {
        result.clear();
        boost::algorithm::unhex(str.begin(), str.end(), std::back_inserter(result));
    } else {
        result = str;
    }
}

std::string NTP1Wallet::__KeyToString(const NTP1OutPoint& op, bool)
{
    return op.getHash().ToString() + ":" + ToString(op.getIndex());
}

void NTP1Wallet::__KeyFromString(const std::string& outputString, bool /*deserialize*/,
                                 NTP1OutPoint&      result)
{
    std::vector<std::string> strs;
    boost::split(strs, outputString, boost::is_any_of(":"));
    if (strs.size() != 2) {
        throw std::runtime_error("The output string is of invalid format. The string given is: \"" +
                                 outputString + "\". The correct format is hash:index");
    }
    uint256 hash;
    hash.SetHex(strs.at(0));
    result = NTP1OutPoint(hash, FromString<unsigned int>(strs.at(1)));
}

json_spirit::Value NTP1Wallet::__ValToJson(const NTP1TokenMetaData& input, bool)
{
    return input.exportDatabaseJsonData();
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value& input, bool /*deserialize*/,
                               NTP1TokenMetaData&        result)
{
    result.setNull();
    result.importDatabaseJsonData(input);
}

json_spirit::Value NTP1Wallet::__ValToJson(const NTP1Transaction& input, bool)
{
    return input.exportDatabaseJsonData();
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value& input, bool /*deserialize*/,
                               NTP1Transaction&          result)
{
    result.setNull();
    result.importDatabaseJsonData(input);
}

json_spirit::Value NTP1Wallet::__ValToJson(const std::string& input, bool serialize)
{
    if (serialize) {
        std::string res;
        boost::algorithm::hex(input.begin(), input.end(), std::back_inserter(res));
        return res;
    } else {
        return input;
    }
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value& input, bool deserialize, std::string& result)
{
    if (deserialize) {
        result.clear();
        std::string inputStr = input.get_str();
        boost::algorithm::unhex(inputStr.begin(), inputStr.end(), std::back_inserter(result));
    } else {
        result = input.get_str();
    }
}

json_spirit::Value NTP1Wallet::__ValToJson(const int64_t& input, bool)
{
    return json_spirit::Value(input);
}

json_spirit::Value NTP1Wallet::__ValToJson(const NTP1Int& input, bool)
{
    return json_spirit::Value(ToString(input));
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value& input, bool /*deserialize*/, int64_t& result)
{
    result = input.get_int64();
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value& input, bool /*deserialize*/, NTP1Int& result)
{
    result = FromString<NTP1Int>(input.get_str());
}

template <typename Container>
json_spirit::Value NTP1Wallet::SerializeMap(const Container& TheMap, bool serializeKey,
                                            bool serializeValue)
{
    json_spirit::Object json_obj;
    for (typename Container::const_iterator it = TheMap.begin(); it != TheMap.end(); it++) {
        std::string        first  = __KeyToString(it->first, serializeKey);
        json_spirit::Value second = __ValToJson(it->second, serializeValue);
        json_obj.push_back(json_spirit::Pair(first, second));
    }
    json_spirit::Value json_value(json_obj);
    return json_value;
}

template <typename Container>
Container NTP1Wallet::DeserializeMap(const json_spirit::Value& json_val, bool deserializeKey,
                                     bool deserializeValue)
{
    Container           result;
    json_spirit::Object json_obj = json_val.get_obj();
    for (typename json_spirit::Object::const_iterator it = json_obj.begin(); it != json_obj.end();
         ++it) {
        typename Container::key_type first;
        __KeyFromString(it->name_, deserializeKey, first);
        typename Container::mapped_type second;
        __ValFromJson(it->value_, deserializeValue, second);
        result[first] = second;
    }
    return result;
}

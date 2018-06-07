#include "ntp1wallet.h"

// the following is a necessary include for pwalletMain and CWalletTx objects
#include "init.h"

#include <boost/thread.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/fstream.hpp>

const std::string NTP1Wallet::ICON_ERROR_CONTENT = "<DownloadError>";

NTP1Wallet::NTP1Wallet()
{
    lastTxCount = 0;
    lastOutputsCount = 0;
    updateBalance = false;
    everSucceededInLoadingTokens = false;
}

void NTP1Wallet::update()
{
    __getOutputs();
    if(updateBalance) {
        __RecalculateTokensBalances();
        everSucceededInLoadingTokens = true;
    }
}

void NTP1Wallet::__getOutputs()
{
    // this helps in persisting to get the wallet data when the application is launched for the first time and nebl wallet is null still
    // the 100 number is just a protection against infinite waiting
    for(int i = 0; i < 100 && ((!everSucceededInLoadingTokens && pwalletMain == NULL) || !appInitiated); i++) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }

    if(pwalletMain == NULL) {
        return;
    }

    std::vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs);

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        // if no new outputs are available
        if(lastTxCount == static_cast<int64_t>(pwalletMain->mapWallet.size()) &&
           lastOutputsCount == static_cast<int64_t>(vecOutputs.size())) {
            return;
        }
    }

    updateBalance = true;

    int failedRetrievals = 0;

    for(unsigned long i = 0; i < vecOutputs.size(); i++) {
        NTP1OutPoint output = ConvertNeblOutputToNTP1(vecOutputs[i]);
        uint256 txHash = output.getHash();
        CWalletTx neblTx;

        // get the transaction from the wallet
        if(!pwalletMain->GetTransaction(txHash, neblTx)) {
            printf("Error: Although the output number %i of transaction %s belongs to you, it couldn't be found in your wallet.\n", vecOutputs[i].i, txHash.ToString().c_str());
            continue;
        }

        // if output already exists, check if it's spent, if it's remove it
        if(removeOutputIfSpent(output, neblTx)) continue;

        NTP1Transaction ntp1tx;
        try {
            ntp1tx = NTP1APICalls::RetrieveData_TransactionInfo(txHash.ToString(), fTestNet);
        } catch (std::exception& ex) {
            printf("Unable to download transaction information. Error says: %s\n", ex.what());
            failedRetrievals++;
            continue;
        }

        // include only NTP1 transactions
        if(ntp1tx.getTxOut(output.getIndex()).getNumOfTokens() > 0) {
            // transaction with output index
            walletOutputsWithTokens[output] = ntp1tx;
            for(long j = 0; j < static_cast<long>(ntp1tx.getTxOut(output.getIndex()).getNumOfTokens()); j++) {
                NTP1TokenTxData tokenTx = ntp1tx.getTxOut(output.getIndex()).getToken(j);
                tokenInformation[tokenTx.getTokenIdBase58()] =
                        NTP1APICalls::RetrieveData_NTP1TokensMetaData(tokenTx.getTokenIdBase58(),
                                                                      txHash.ToString(),
                                                                      output.getIndex(),
                                                                      fTestNet);
            }
        }
    }

    scanSpentTransactions();

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        lastTxCount = static_cast<int64_t>(pwalletMain->mapWallet.size()) - failedRetrievals;
        lastOutputsCount = static_cast<int64_t>(vecOutputs.size()) - failedRetrievals;
    }
}

void NTP1Wallet::__RecalculateTokensBalances()
{
    balances.clear();
    for(boost::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator it = walletOutputsWithTokens.begin();
        it != walletOutputsWithTokens.end();
        it++) {
        const NTP1Transaction& tx = it->second;
        int outputIndex = it->first.getIndex();
        AddOutputToWalletBalance(tx, outputIndex, this->balances);
    }
    updateBalance = false;
}

void NTP1Wallet::AddOutputToWalletBalance(const NTP1Transaction &tx, int outputIndex, std::map<string, int64_t> &balancesTable)
{
    for(long j = 0; j < static_cast<long>(tx.getTxOut(outputIndex).getNumOfTokens()); j++) {
        NTP1TokenTxData tokenTx = tx.getTxOut(outputIndex).getToken(j);
        const std::string& tokenID = tokenTx.getTokenIdBase58();
        if(balancesTable.find(tokenID) == balancesTable.end()) {
            balancesTable[tokenID] = tokenTx.getAmount();
        } else {
            balancesTable[tokenID] += tokenTx.getAmount();
        }
    }
}

bool NTP1Wallet::removeOutputIfSpent(const NTP1OutPoint &output, const CWalletTx& neblTx)
{
    boost::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator outputIt = walletOutputsWithTokens.find(output);
    if(outputIt != walletOutputsWithTokens.end()) {
        if(neblTx.IsSpent(output.getIndex())) {
            walletOutputsWithTokens.erase(outputIt);
        }
        return true;
    }
    return false;
}

void NTP1Wallet::scanSpentTransactions()
{
    if(pwalletMain == NULL) return;
    std::deque<NTP1OutPoint> toRemove;
    for(boost::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator it = walletOutputsWithTokens.begin();
        it != walletOutputsWithTokens.end();
        it++)
    {
        CWalletTx neblTx;
        int outputIndex = it->first.getIndex();
        const uint256& txHash = it->first.getHash();
        if(!pwalletMain->GetTransaction(txHash, neblTx)) continue;
        if(neblTx.IsSpent(outputIndex)) {
            // this, although the right way to do things, causes a crash. A safer plan is chosen
            // it = walletOutputsWithTokens.erase(it);
            toRemove.push_back(it->first);
        }
    }
    for(unsigned long i = 0; i < toRemove.size(); i++) {
        if(walletOutputsWithTokens.find(toRemove[i]) != walletOutputsWithTokens.end()) {
            walletOutputsWithTokens.erase(toRemove[i]);
        } else {
            printf("Unable to find output %s:%s, although it was found before and marked for removal.\n", toRemove[i].getHash().ToString().c_str(), ToString(toRemove[i].getIndex()).c_str());
//            std::cerr<<"Unable to find output " << toRemove[i].getHash().ToString() << ":" << ToString(toRemove[i].getIndex()) << ", although it was found before and marked for removal." << std::endl;
        }
    }
}

NTP1OutPoint NTP1Wallet::ConvertNeblOutputToNTP1(const COutput &output)
{
    return NTP1OutPoint(output.tx->GetHash(), output.i);
}

std::string NTP1Wallet::getTokenName(const std::string &tokenID) const
{
    boost::unordered_map<std::string,  NTP1TokenMetaData>::const_iterator it = tokenInformation.find(tokenID);
    if(it == tokenInformation.end()) {
        return std::string("<NameError>");
    } else {
        return it->second.getTokenName();
    }
}

int64_t NTP1Wallet::getTokenBalance(const string &tokenID) const
{
    std::map<std::string, int64_t>::const_iterator it = balances.find(tokenID);
    if(it == balances.end()) {
        return 0;
    } else {
        return static_cast<int64_t>(it->second);
    }
}

string NTP1Wallet::getTokenName(int index) const
{
    std::map<std::string, int64_t>::const_iterator it = balances.begin();
    std::advance(it, index);
    boost::unordered_map<std::string,  NTP1TokenMetaData>::const_iterator itToken = tokenInformation.find(it->first);
    if(itToken == tokenInformation.end()) {
        return std::string("<NameError>");
    } else {
        return itToken->second.getTokenName();
    }
}

string NTP1Wallet::getTokenDescription(int index) const
{
    std::map<std::string, int64_t>::const_iterator it = balances.begin();
    std::advance(it, index);
    boost::unordered_map<std::string,  NTP1TokenMetaData>::const_iterator itToken = tokenInformation.find(it->first);
    if(itToken == tokenInformation.end()) {
        return std::string("<DescError>");
    } else {
        return itToken->second.getTokenDescription();
    }
}

int64_t NTP1Wallet::getTokenBalance(int index) const
{
    if(index > getNumberOfTokens()) return 0;
    std::map<std::string, int64_t>::const_iterator it = balances.begin();
    std::advance(it, index);
    if(it == balances.end()) {
        return 0;
    } else {
        return static_cast<int64_t>(it->second);
    }
}

std::string NTP1Wallet::__downloadIcon(const std::string& IconURL) {
    try {
        return cURLTools::GetFileFromHTTPS(IconURL, 30, false);
    } catch (std::exception& ex) {
        printf("Error: Failed at downloading icon from %s. Error says: %s\n", IconURL.c_str(), ex.what());
        return ICON_ERROR_CONTENT;
    }
}

void
NTP1Wallet::__asyncDownloadAndSetIcon(std::string IconURL, std::string tokenId,
                                      boost::shared_ptr<NTP1Wallet> wallet)
{
    wallet->tokenIcons.set(tokenId, __downloadIcon(IconURL));
}


string NTP1Wallet::getTokenIcon(int index)
{
    std::map<std::string, int64_t>::const_iterator it = balances.begin();
    std::advance(it, index);
    std::string tokenId = it->first;
    boost::unordered_map<std::string,  NTP1TokenMetaData>::const_iterator itToken = tokenInformation.find(tokenId);
    if(!tokenIcons.exists(tokenId)) {
        const std::string& IconURL = itToken->second.getIconURL();
        if(IconURL.empty()) {
            // no icon URL provided, set empty icon and return
            tokenIcons.set(tokenId, "");
            return "";
        }
        boost::thread IconDownloadThread(boost::bind(__asyncDownloadAndSetIcon, IconURL, tokenId, shared_from_this()));
        IconDownloadThread.detach();
        return "";
    } else {
        std::string icon;
        tokenIcons.get(tokenId, icon);
        // if there was an error getting the icon OR the icon is empty, and a download URL now exists, download again
        if(icon == ICON_ERROR_CONTENT || (icon == "" && !itToken->second.getIconURL().empty())) {
            const std::string& IconURL = itToken->second.getIconURL();
            boost::thread IconDownloadThread(boost::bind(__asyncDownloadAndSetIcon, IconURL, tokenId, shared_from_this()));
            IconDownloadThread.detach();
        }

        tokenIcons.get(tokenId, icon);
        return icon;
    }
}

int64_t NTP1Wallet::getNumberOfTokens() const
{
    return balances.size();
}

bool NTP1Wallet::hasEverSucceeded() const
{
    return everSucceededInLoadingTokens;
}

bool NTP1Wallet::IconHasErrorContent(const string &icon)
{
    return icon == ICON_ERROR_CONTENT;
}

void NTP1Wallet::clear()
{
    tokenInformation.clear();
    walletOutputsWithTokens.clear();
    tokenIcons.clear();
    balances.clear();
    lastTxCount = 0;
    lastOutputsCount = 0;
}

string NTP1Wallet::Serialize(const NTP1Wallet &wallet)
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("token_info", SerializeMap(wallet.tokenInformation, false, false)));
    root.push_back(json_spirit::Pair("outputs", SerializeMap(wallet.walletOutputsWithTokens, false, false)));
    root.push_back(json_spirit::Pair("icons", SerializeMap(wallet.tokenIcons.getInternalMap(), false, true)));
    root.push_back(json_spirit::Pair("balances", SerializeMap(wallet.balances, false, false)));

    return json_spirit::write_formatted( root );
}

NTP1Wallet NTP1Wallet::Deserialize(const string &data)
{
    NTP1Wallet result;

    json_spirit::Value parsedData;
    json_spirit::read_or_throw(data, parsedData);

    json_spirit::Value tokenInfoData(NTP1Tools::GetObjectField(parsedData.get_obj(), "token_info"));
    result.tokenInformation =
            DeserializeMap<boost::unordered_map<std::string,  NTP1TokenMetaData> >(tokenInfoData, false, false);
    json_spirit::Value outputsData(NTP1Tools::GetObjectField(parsedData.get_obj(), "outputs"));
    result.walletOutputsWithTokens =
            DeserializeMap<boost::unordered_map<NTP1OutPoint,  NTP1Transaction> >(outputsData, false, false);
    json_spirit::Value iconsData(NTP1Tools::GetObjectField(parsedData.get_obj(), "icons"));
    result.tokenIcons.setInternalMap(
            DeserializeMap<boost::unordered_map<std::string,  std::string> >(iconsData, false, true));
    json_spirit::Value balancesData(NTP1Tools::GetObjectField(parsedData.get_obj(), "balances"));
    result.balances =
            DeserializeMap<std::map<std::string, int64_t> >(balancesData, false, false);

    return result;
}

void NTP1Wallet::exportToFile(const boost::filesystem::path &filePath) const
{
    std::string output = Serialize(*this);
    boost::filesystem::fstream fileObj(filePath, std::ios::out);
    fileObj.write(output.c_str(), output.size());
    fileObj.close();
}

void NTP1Wallet::importFromFile(const boost::filesystem::path &filePath)
{
    boost::filesystem::fstream fileObj(filePath, std::ios::in);
    std::string data((std::istreambuf_iterator<char>(fileObj)),
                      std::istreambuf_iterator<char>());
    fileObj.close();
    this->clear();
    *this = Deserialize(data);
}

string NTP1Wallet::__KeyToString(const string &str, bool serialize)
{
    if(serialize) {
        std::string res;
        boost::algorithm::hex(str.begin(), str.end(), std::back_inserter(res));
        return res;
    } else {
        return str;
    }
}

void NTP1Wallet::__KeyFromString(const string &str, bool deserialize, std::string &result)
{
    if(deserialize) {
        result.clear();
        boost::algorithm::unhex(str.begin(), str.end(), std::back_inserter(result));
    } else {
        result = str;
    }
}

string NTP1Wallet::__KeyToString(const NTP1OutPoint &op, bool)
{
    return op.getHash().ToString() + ":" + ToString(op.getIndex());
}

void NTP1Wallet::__KeyFromString(const string &outputString, bool deserialize, NTP1OutPoint &result)
{
    vector<string> strs;
    boost::split(strs, outputString, boost::is_any_of(":"));
    if(strs.size() != 2) {
        throw std::runtime_error("The output string is of invalid format. The string given is: \"" + outputString + "\". The correct format is hash:index");
    }
    uint256 hash;
    hash.SetHex(strs.at(0));
    result = NTP1OutPoint(hash, FromString<unsigned int>(strs.at(1)));
}

json_spirit::Value NTP1Wallet::__ValToJson(const NTP1TokenMetaData &input, bool)
{
    return input.exportDatabaseJsonData();
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value &input, bool deserialize, NTP1TokenMetaData& result)
{
    result.setNull();
    result.importDatabaseJsonData(input);
}

json_spirit::Value NTP1Wallet::__ValToJson(const NTP1Transaction &input, bool)
{
    return input.exportDatabaseJsonData();
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value &input, bool deserialize, NTP1Transaction &result)
{
    result.setNull();
    result.importDatabaseJsonData(input);
}

json_spirit::Value NTP1Wallet::__ValToJson(const string &input, bool serialize)
{
    if(serialize) {
        std::string res;
        boost::algorithm::hex(input.begin(), input.end(), std::back_inserter(res));
        return res;
    } else {
        return input;
    }
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value &input, bool deserialize, string &result)
{
    if(deserialize) {
        result.clear();
        std::string inputStr = input.get_str();
        boost::algorithm::unhex(inputStr.begin(), inputStr.end(), std::back_inserter(result));
    } else {
        result = input.get_str();
    }
}

json_spirit::Value NTP1Wallet::__ValToJson(const int64_t &input, bool)
{
    return json_spirit::Value(input);
}

void NTP1Wallet::__ValFromJson(const json_spirit::Value &input, bool deserialize, int64_t &result)
{
    result = input.get_int64();
}

template<typename Container>
json_spirit::Value NTP1Wallet::SerializeMap(const Container &TheMap, bool serializeKey, bool serializeValue)
{
    json_spirit::Object json_obj;
    for(typename Container::const_iterator it = TheMap.begin();
        it != TheMap.end();
        it++) {
        std::string first = __KeyToString(it->first, serializeKey);
        json_spirit::Value second = __ValToJson(it->second, serializeValue);
        json_obj.push_back( json_spirit::Pair(first, second) );
    }
    json_spirit::Value json_value(json_obj);
    return json_value;
}

template<typename Container>
Container NTP1Wallet::DeserializeMap(const json_spirit::Value &json_val, bool deserializeKey, bool deserializeValue)
{
    Container result;
    json_spirit::Object json_obj = json_val.get_obj();
    for(typename json_spirit::Object::const_iterator it = json_obj.begin();
        it != json_obj.end();
        ++it) {
        typename Container::key_type first;
        __KeyFromString(it->name_, deserializeKey, first);
        typename Container::mapped_type second;
        __ValFromJson(it->value_, deserializeValue, second);
        result[first] = second;
    }
    return result;
}

#include "ntp1wallet.h"

// the following is a necessary include for pwalletMain and CWalletTx objects
#include "init.h"

#include <boost/thread.hpp>

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
            printf("Error: Although the output number %i of transaction %s belongs to you, it couldn't be found in your wallet.", vecOutputs[i].i, txHash.ToString().c_str());
            continue;
        }

        // if output already exists, check if it's spent, if it's remove it
        if(removeOutputIfSpent(output, neblTx)) continue;

        NTP1Transaction ntp1tx;
        try {
            ntp1tx = NTP1APICalls::RetrieveData_TransactionInfo(txHash.ToString(), fTestNet);
        } catch (std::exception& ex) {
            printf("Unable to download transaction information. Error says: %s", ex.what());
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

void NTP1Wallet::SubtractOutputFromWalletBalance(const NTP1Transaction &tx, int outputIndex, std::map<string, int64_t> &balancesTable)
{
    for(long j = 0; j < static_cast<long>(tx.getTxOut(outputIndex).getNumOfTokens()); j++) {
        NTP1TokenTxData tokenTx = tx.getTxOut(outputIndex).getToken(j);
        const std::string& tokenID = tokenTx.getTokenIdBase58();
        if(balancesTable.find(tokenID) != balancesTable.end()) {
            balancesTable[tokenID] -= tokenTx.getAmount();
        } else {
            // TODO: raise exception
        }
    }
}

bool NTP1Wallet::removeOutputIfSpent(const NTP1OutPoint &output, const CWalletTx& neblTx)
{
    boost::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator outputIt = walletOutputsWithTokens.find(output);
    if(outputIt != walletOutputsWithTokens.end()) {
        if(neblTx.IsSpent(output.getIndex())) {
//            SubtractOutputFromWalletBalance(outputIt->second, output.getIndex(), balances);
            walletOutputsWithTokens.erase(outputIt);
        }
        return true;
    }
    return false;
}

void NTP1Wallet::scanSpentTransactions()
{
    if(pwalletMain == NULL) return;
    for(boost::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator it = walletOutputsWithTokens.begin();
        it != walletOutputsWithTokens.end();
        it++)
    {
        CWalletTx neblTx;
        int outputIndex = it->first.getIndex();
        const uint256& txHash = it->first.getHash();
        if(!pwalletMain->GetTransaction(txHash, neblTx)) continue;
        if(neblTx.IsSpent(outputIndex)) {
            it = walletOutputsWithTokens.erase(it);
        }
    }
}

void NTP1Wallet::considerNTP1OutputSpent(const NTP1OutPoint& output)
{
    boost::unordered_map<NTP1OutPoint, NTP1Transaction>::iterator outputIt = walletOutputsWithTokens.find(output);
    if(outputIt == walletOutputsWithTokens.end()) {
        // TODO: this should throw an exception, and should be inside a class that has all token stuff
        return;
    }
    SubtractOutputFromWalletBalance(walletOutputsWithTokens[output], output.getIndex(), balances);
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
        return cURLTools::GetFileFromHTTPS(IconURL, false);
    } catch (std::exception& ex) {
        printf("Error: Failed at downloading icon from %s. Error says: %s", IconURL.c_str(), ex.what());
        return ICON_ERROR_CONTENT;
    }
}

void NTP1Wallet::__asyncDownloadAndSetIcon(std::string IconURL, std::string tokenId, ThreadSafeHashMap<std::string, std::string>& IconsMap)
{
    IconsMap.set(tokenId, __downloadIcon(IconURL));
}


string NTP1Wallet::getTokenIcon(int index) const
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
        boost::thread IconDownloadThread(boost::bind(__asyncDownloadAndSetIcon, IconURL, tokenId, boost::ref(tokenIcons)));
        IconDownloadThread.detach();
        return "";
    } else {
        std::string icon;
        tokenIcons.get(tokenId, icon);
        // if there was an error getting the icon OR the icon is empty, and a download URL now exists, download again
        if(icon == ICON_ERROR_CONTENT || (icon == "" && !itToken->second.getIconURL().empty())) {
            const std::string& IconURL = itToken->second.getIconURL();
            boost::thread IconDownloadThread(boost::bind(__asyncDownloadAndSetIcon, IconURL, tokenId, boost::ref(tokenIcons)));
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

    root.push_back(json_spirit::Pair("token_info", SerializeMap(wallet.tokenInformation)));

    root.push_back( json_spirit::Pair( "identifier", "id" ) );
    root.push_back( json_spirit::Pair( "label", "name" ) );
    root.push_back( json_spirit::Pair( "items", json_spirit::Array() ) );

    json_spirit::Array& items_array = root.back().value_.get_array();
    items_array.push_back( json_spirit::Object() );

    json_spirit::Object& item_1 = items_array.back().get_obj();
    item_1.push_back( json_spirit::Pair( "id", "Vehicle_n" ) );
    item_1.push_back( json_spirit::Pair( "name", "Vehicle" ) );
    item_1.push_back( json_spirit::Pair( "type", "root" ) );
    item_1.push_back( json_spirit::Pair( "children", json_spirit::Array() ) );

    json_spirit::Array& children_array = item_1.back().value_.get_array();

    children_array.push_back( json_spirit::Object() );
    json_spirit::Object& child_1 = children_array.back().get_obj();
    child_1.push_back( json_spirit::Pair( "_reference", "Passenger_n" ) );

    children_array.push_back( json_spirit::Object() );
    json_spirit::Object& child_2 = children_array.back().get_obj();
    child_2.push_back( json_spirit::Pair( "_reference", "Commercial_n" ) );

    return json_spirit::write_formatted( root );
}

NTP1Wallet NTP1Wallet::Deserialize(const string &data)
{
    NTP1Wallet result;

    return result;
}

template<typename Container>
json_spirit::Value NTP1Wallet::SerializeMap(const Container &TheMap)
{
    json_spirit::Object json_obj;
    for(typename Container::const_iterator it = TheMap.begin();
        it != TheMap.end();
        it++) {
        std::string first = it->first;
        json_spirit::Value second = it->second.exportDatabaseJsonData();
        json_obj.push_back( json_spirit::Pair(first, second) );
    }
    json_spirit::Value json_value(json_obj);
    return json_value;
}

template<typename Container>
Container NTP1Wallet::DeserializeMap(const json_spirit::Object &json_obj)
{
    Container result;
    for(typename json_spirit::Object::const_iterator it = json_obj.begin();
        it != json_obj.end();
        ++it) {
        result.insert(*it);
    }
    return result;
}

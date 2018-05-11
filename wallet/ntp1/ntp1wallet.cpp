#include "ntp1wallet.h"

// the following is a necessary include for pwalletMain and CWalletTx objects
#include "init.h"

const std::string NTP1Wallet::ICON_ERROR_CONTENT = "<DownloadError>";

NTP1Wallet::NTP1Wallet()
{
    lastSizeFound = 0;
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
    std::vector<COutput> vecOutputs;

    // this helps in persisting to get the wallet data when the application is launched for the first time and nebl wallet is null still
    // the 100 number is just a protection against infinite waiting
    for(int i = 0; i < 100 && ((!everSucceededInLoadingTokens && pwalletMain == NULL) || !appInitiated); i++) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }

    if(pwalletMain == NULL) {
        return;
    }
    pwalletMain->AvailableCoins(vecOutputs);

    // if no new outpust are available
    if(lastSizeFound == static_cast<int64_t>(vecOutputs.size())) {
        return;
    }

    updateBalance = true;

    for(unsigned long i = 0; i < vecOutputs.size(); i++) {
        NTP1OutPoint output = ConvertNeblOutputToNTP1(vecOutputs[i]);
        uint256 txHash = output.getHash();
        CWalletTx neblTx;
        if(!pwalletMain->GetTransaction(txHash, neblTx)) {
            // TODO: Check the severity of this error
            printf("Error: Although the output number %i of transaction %s belongs to you, it couldn't be found in your wallet.", vecOutputs[i].i, txHash.ToString().c_str());
            continue;
        }

        // if output already exists, check if it's spent, if it's remove it
        if(removeOutputIfSpent(output, neblTx)) continue;

        NTP1Transaction ntp1tx = NTP1APICalls::RetrieveData_TransactionInfo(txHash.ToString(), fTestNet);
        if(walletOutputsWithTokens.find(output) != walletOutputsWithTokens.end()) {
            throw std::logic_error("Error while loading NTP1 tokens. Transaction hash found twice.");
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
    lastSizeFound = vecOutputs.size();
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
            SubtractOutputFromWalletBalance(outputIt->second, output.getIndex(), balances);
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
            considerNTP1OutputSpent(it->first);
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

std::string NTP1Wallet::__downloadIcon(const std::string& IconURL) const {
    try {
        return cURLTools::GetFileFromHTTPS(IconURL, false);
    } catch (std::exception& ex) {
        printf("Error: Failed at downloading icon from %s. Error says: %s", IconURL.c_str(), ex.what());
        return ICON_ERROR_CONTENT;
    }
}

string NTP1Wallet::getTokenIcon(int index) const
{
    std::map<std::string, int64_t>::const_iterator it = balances.begin();
    std::advance(it, index);
    std::string tokenId = it->first;
    boost::unordered_map<std::string,  std::string>::const_iterator itIcon = tokenIcons.find(tokenId);
    if(itIcon == tokenIcons.end()) {
        boost::unordered_map<std::string,  NTP1TokenMetaData>::const_iterator itToken = tokenInformation.find(tokenId);
        const std::string& IconURL = itToken->second.getIconURL();
        if(IconURL.empty()) {
            return "";
        }
        tokenIcons[tokenId] = __downloadIcon(IconURL);
        return tokenIcons[tokenId];
    } else {
        if(itIcon->second == ICON_ERROR_CONTENT) {
            boost::unordered_map<std::string,  NTP1TokenMetaData>::const_iterator itToken = tokenInformation.find(tokenId);
            const std::string& IconURL = itToken->second.getIconURL();
            tokenIcons[tokenId] = __downloadIcon(IconURL);
        }
        return itIcon->second;
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

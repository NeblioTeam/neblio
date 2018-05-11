#ifndef NTP1WALLET_H
#define NTP1WALLET_H

#include "ntp1/ntp1apicalls.h"
#include "ntp1/ntp1transaction.h"
#include "ntp1/ntp1tokenmetadata.h"
#include "ntp1/ntp1outpoint.h"
#include "curltools.h"
#include "ThreadSafeHashMap.h"

#include <boost/unordered_map.hpp>

class COutput;
class CWalletTx;

class NTP1Wallet
{
    // base58 token id vs NTP1 token meta data object
    boost::unordered_map<std::string,  NTP1TokenMetaData> tokenInformation;
    // transaction with output index
    boost::unordered_map<NTP1OutPoint, NTP1Transaction> walletOutputsWithTokens;
    // wallet balances
    std::map<std::string, int64_t> balances;
    // map from token id vs icon image data
    mutable ThreadSafeHashMap<std::string, std::string> tokenIcons;

    bool updateBalance;

    // remains false until a successful attempt to retrieve tokens is over (for display purposes)
    bool everSucceededInLoadingTokens;

    void __getOutputs();
    void __RecalculateTokensBalances();
    static void __asyncDownloadAndSetIcon(std::string IconURL, std::string tokenId, ThreadSafeHashMap<std::string, std::string> &IconsMap);
    static std::string __downloadIcon(const std::string &IconURL);
    static void AddOutputToWalletBalance(const NTP1Transaction& tx, int outputIndex, std::map<std::string, int64_t>& balancesTable);
    static void SubtractOutputFromWalletBalance(const NTP1Transaction& tx, int outputIndex, std::map<std::string, int64_t>& balancesTable);
    // returns true if removed
    bool removeOutputIfSpent(const NTP1OutPoint& output, const CWalletTx& neblTx);
    void scanSpentTransactions();
    void considerNTP1OutputSpent(const NTP1OutPoint &output);
    static NTP1OutPoint ConvertNeblOutputToNTP1(const COutput& output);

    // when scanning the neblio wallet, this is the number of relevant transactions found
    int64_t lastSizeFound;

    static const std::string ICON_ERROR_CONTENT;

public:
    NTP1Wallet();
    void update();
    std::string getTokenName(const std::string& tokenID) const;
    int64_t getTokenBalance(const std::string& tokenID) const;
    std::string getTokenName(int index) const;
    std::string getTokenDescription(int index) const;
    int64_t getTokenBalance(int index) const;
    std::string getTokenIcon(int index) const;
    int64_t getNumberOfTokens() const;
    bool hasEverSucceeded() const;
    friend inline bool operator==(const NTP1Wallet& lhs, const NTP1Wallet& rhs);
    static bool IconHasErrorContent(const std::string& icon);
};

bool operator==(const NTP1Wallet &lhs, const NTP1Wallet &rhs)
{
    return (lhs.getNumberOfTokens()     == rhs.getNumberOfTokens()     &&
            lhs.tokenInformation        == rhs.tokenInformation        &&
            lhs.walletOutputsWithTokens == rhs.walletOutputsWithTokens &&
            lhs.tokenIcons              == rhs.tokenIcons              &&
            lhs.balances                == rhs.balances);
}

#endif // NTP1WALLET_H

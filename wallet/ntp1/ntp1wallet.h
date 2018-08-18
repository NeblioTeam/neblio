#ifndef NTP1WALLET_H
#define NTP1WALLET_H

#include "ThreadSafeHashMap.h"
#include "curltools.h"
#include "ntp1/ntp1apicalls.h"
#include "ntp1/ntp1outpoint.h"
#include "ntp1/ntp1tokenmetadata.h"
#include "ntp1/ntp1transaction.h"
#include "json/json_spirit.h"

#include <unordered_map>

class COutput;
class CWalletTx;

class NTP1Wallet : public boost::enable_shared_from_this<NTP1Wallet>
{
    bool retrieveMetadataFromAPI = true;

    int minConfirmations = -1; // default value -1 will not make any constraints
    int maxConfirmations = -1; // default value -1 will not make any constraints

    // base58 token id vs NTP1 token meta data object
    std::unordered_map<std::string, NTP1TokenMetaData> tokenInformation;
    // transaction with output index
    std::unordered_map<NTP1OutPoint, NTP1Transaction> walletOutputsWithTokens;
    // wallet balances
    std::map<std::string, int64_t> balances;
    // map from token id vs icon image data
    ThreadSafeHashMap<std::string, std::string> tokenIcons;

    bool updateBalance;

    // remains false until a successful attempt to retrieve tokens is over (for display purposes)
    bool everSucceededInLoadingTokens;

    void __getOutputs();
    void __RecalculateTokensBalances();

    // it's very important to use shared_from_this() here to guarantee thread-safety
    // if the shared_ptr's content gets deleted before the thread gets executed, it will lead to a
    // segfault passing a shared_ptr guarantees that the object will survive until the end
    static void __asyncDownloadAndSetIcon(std::string IconURL, std::string tokenId,
                                          boost::shared_ptr<NTP1Wallet> wallet);

    static std::string __downloadIcon(const std::string& IconURL);
    static void        AddOutputToWalletBalance(const NTP1Transaction& tx, int outputIndex,
                                                std::map<std::string, int64_t>& balancesTable);
    // returns true if removed
    bool                removeOutputIfSpent(const NTP1OutPoint& output, const CWalletTx& neblTx);
    void                scanSpentTransactions();
    static NTP1OutPoint ConvertNeblOutputToNTP1(const COutput& output);

    // when scanning the neblio wallet, this is the number of relevant transactions found
    int64_t lastTxCount;
    int64_t lastOutputsCount;

    static const std::string ICON_ERROR_CONTENT;

public:
    NTP1Wallet();
    void                                  update();
    std::string                           getTokenName(const std::string& tokenID) const;
    int64_t                               getTokenBalance(const std::string& tokenID) const;
    std::string                           getTokenName(int index) const;
    std::string                           getTokenId(int index) const;
    std::string                           getTokenDescription(int index) const;
    int64_t                               getTokenBalance(int index) const;
    std::string                           getTokenIcon(int index);
    int64_t                               getNumberOfTokens() const;
    const std::map<std::string, int64_t>& getBalancesMap() const;
    const std::unordered_map<NTP1OutPoint, NTP1Transaction>& getWalletOutputsWithTokens();
    bool                                                     hasEverSucceeded() const;
    friend inline bool operator==(const NTP1Wallet& lhs, const NTP1Wallet& rhs);
    static bool        IconHasErrorContent(const std::string& icon);
    void               clear();
    void               setMinMaxConfirmations(int minConfs, int maxConfs = -1);
    static std::string Serialize(const NTP1Wallet& wallet);
    static NTP1Wallet  Deserialize(const std::string& data);

    // Serialize and deserialize maps basically is a generic serializer for any map
    // Notice that the type considerations are taking in
    // __KeyToString/__KeyFromString/__ValToJson/__ValFromJson
    template <typename Container>
    static json_spirit::Value SerializeMap(const Container& TheMap, bool serializeKey,
                                           bool serializeValue);
    template <typename Container>
    static Container DeserializeMap(const json_spirit::Value& json_val, bool deserializeKey,
                                    bool deserializeValue);

    void exportToFile(const boost::filesystem::path& filePath) const;
    void importFromFile(const boost::filesystem::path& filePath);

    //    static void CreateNTP1SendTransaction(uint64_t fee);

    bool getRetrieveMetadataFromAPI() const;
    void setRetrieveMetadataFromAPI(bool value);

    std::map<std::string, int64_t> getBalances() const;

    const std::unordered_map<std::string, NTP1TokenMetaData>& getTokenMetadataMap() const;

private:
    static std::string __KeyToString(const std::string& str, bool serialize);
    static void        __KeyFromString(const std::string& str, bool deserialize, std::string& result);
    static std::string __KeyToString(const NTP1OutPoint& op, bool);
    static void __KeyFromString(const std::string& outputString, bool deserialize, NTP1OutPoint& result);
    static json_spirit::Value __ValToJson(const NTP1TokenMetaData& input, bool serialize);
    static void               __ValFromJson(const json_spirit::Value& input, bool deserialize,
                                            NTP1TokenMetaData& result);
    static json_spirit::Value __ValToJson(const NTP1Transaction& input, bool serialize);
    static void               __ValFromJson(const json_spirit::Value& input, bool deserialize,
                                            NTP1Transaction& result);
    static json_spirit::Value __ValToJson(const std::string& input, bool serialize);
    static void __ValFromJson(const json_spirit::Value& input, bool deserialize, std::string& result);
    static json_spirit::Value __ValToJson(const int64_t& input, bool serialize);
    static void __ValFromJson(const json_spirit::Value& input, bool deserialize, int64_t& result);
};

bool operator==(const NTP1Wallet& lhs, const NTP1Wallet& rhs)
{
    return (lhs.getNumberOfTokens() == rhs.getNumberOfTokens() &&
            lhs.tokenInformation == rhs.tokenInformation &&
            lhs.walletOutputsWithTokens == rhs.walletOutputsWithTokens &&
            lhs.tokenIcons == rhs.tokenIcons && lhs.balances == rhs.balances &&
            lhs.lastTxCount == rhs.lastTxCount && lhs.lastOutputsCount == rhs.lastOutputsCount);
}

#endif // NTP1WALLET_H

#ifndef NTP1TOKENTXDATA_H
#define NTP1TOKENTXDATA_H

#include "json_spirit.h"
#include "ntp1script.h"
#include "serialize.h"
#include "uint256.h"
#include <string>

class NTP1TokenTxData
{
    std::string tokenId;
    NTP1Int     amount;
    uint256     issueTxId;
    uint64_t    divisibility;
    // should be bool, but bool doesn't play well with the serializer
    int         lockStatus;
    std::string aggregationPolicy;
    std::string tokenSymbol;

public:
    NTP1TokenTxData();
    void               setNull();
    void               setTokenId(const std::string& Str);
    void               setIssueTxIdHex(const std::string& hex);
    void               importJsonData(const std::string& data);
    void               importJsonData(const json_spirit::Value& data);
    json_spirit::Value exportDatabaseJsonData() const;
    void               importDatabaseJsonData(const json_spirit::Value& data);
    std::string        getTokenId() const;
    NTP1Int            getAmount() const;
    void               setAmount(const NTP1Int& value);
    uint64_t           getDivisibility() const;
    uint256            getIssueTxId() const;
    bool               getLockStatus() const;
    const std::string& getAggregationPolicy() const;
    void               setDivisibility(const uint64_t& value);
    void               setLockStatus(bool value);
    void               setAggregationPolicy(const std::string& value);
    std::string        getTokenSymbol() const;
    void               setTokenSymbol(const std::string& value);
    friend inline bool operator==(const NTP1TokenTxData& lhs, const NTP1TokenTxData& rhs);

    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(tokenId);
                        READWRITE(amount);
                        READWRITE(issueTxId);
                        READWRITE(divisibility);
                        READWRITE(lockStatus);
                        READWRITE(aggregationPolicy);
                        READWRITE(tokenSymbol);
                       )
    // clang-format on
};

bool operator==(const NTP1TokenTxData& lhs, const NTP1TokenTxData& rhs)
{
    return (lhs.tokenId == rhs.tokenId && lhs.amount == rhs.amount && lhs.issueTxId == rhs.issueTxId &&
            lhs.divisibility == rhs.divisibility && lhs.lockStatus == rhs.lockStatus &&
            lhs.aggregationPolicy == rhs.aggregationPolicy);
}

#endif // NTP1TOKENTXDATA_H

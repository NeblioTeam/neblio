#ifndef NTP1TOKENMINIMALMETADATA_H
#define NTP1TOKENMINIMALMETADATA_H

#include "uint256.h"
#include <string>
#include <vector>

class NTP1TokenMinimalMetaData
{
protected:
    std::string tokenId;
    uint256     issuanceTxId;
    uint64_t    divisibility;
    bool        lockStatus;
    std::string aggregationPolicy;

public:
    NTP1TokenMinimalMetaData();
    void               setNull();
    void               setTokenId(const std::string& Str);
    void               setIssuanceTxIdHex(const std::string& hex);
    const std::string& getTokenId() const;
    std::string        getIssuanceTxIdHex() const;
    uint64_t           getDivisibility() const;
    bool               getLockStatus() const;
    const std::string& getAggregationPolicy() const;
    uint256            getIssuanceTxId() const;
    void               setIssuanceTxId(const uint256& value);

    friend inline bool operator==(const NTP1TokenMinimalMetaData& lhs,
                                  const NTP1TokenMinimalMetaData& rhs);
};

bool operator==(const NTP1TokenMinimalMetaData& lhs, const NTP1TokenMinimalMetaData& rhs)
{
    return (lhs.getTokenId() == rhs.getTokenId() && lhs.getIssuanceTxId() == rhs.getIssuanceTxId() &&
            lhs.getDivisibility() == rhs.getDivisibility() &&
            lhs.getLockStatus() == rhs.getLockStatus() &&
            lhs.getAggregationPolicy() == rhs.getAggregationPolicy());
}

#endif // NTP1TOKENMINIMALMETADATA_H

#ifndef NTP1TOKENTXDATA_H
#define NTP1TOKENTXDATA_H

#include <string>
#include "uint256.h"
#include "ntp1tools.h"

class NTP1TokenTxData
{
    std::vector<unsigned char> tokenId;
    uint64_t amount;
    uint256 issueTxId;
    uint64_t divisibility;
    bool lockStatus;
    std::string aggregationPolicy;

public:
    NTP1TokenTxData();
    void setNull();
    void setTokenIdBase58(const std::string& Str);
    void setIssueTxIdHex(const std::string& hex);
    void importJsonData(const std::string& data);
    void importJsonData(const json_spirit::Value& data);
    std::string getTokenIdBase58() const;
    uint64_t getAmount() const;
    uint64_t getDivisibility() const;
    uint256 getIssueTxId() const;
    bool getLockStatus() const;
    const std::string& getAggregationPolicy() const;
};

#endif // NTP1TOKENTXDATA_H

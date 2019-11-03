#ifndef NTP1TOKENMETADATA_H
#define NTP1TOKENMETADATA_H

#include "base58.h"
#include "json_spirit.h"
#include "ntp1tokenminimalmetadata.h"
#include "uint256.h"
#include <vector>

class NTP1Script_Issuance;

class NTP1TokenMetaData : public NTP1TokenMinimalMetaData
{
    NTP1Int            totalSupply;
    std::string        tokenName;
    std::string        tokenDescription;
    std::string        tokenIssuer;
    std::string        iconURL;
    std::string        iconImageType;
    json_spirit::Value userData;
    json_spirit::Value urls;

public:
    void setNull();
    bool isNull() const;

    void               importRestfulAPIJsonData(const std::string& data);
    void               importRestfulAPIJsonData(const json_spirit::Value& data);
    json_spirit::Value exportDatabaseJsonData(bool for_rpc = false) const;
    void               importDatabaseJsonData(const json_spirit::Value& data);

    void readSomeDataFromStandardJsonFormat(const json_spirit::Value& data);
    void readSomeDataFromNTP1IssuanceScript(NTP1Script_Issuance* sd);

    NTP1TokenMetaData();
    NTP1Int            getTotalSupply() const;
    const std::string& getTokenName() const;
    const std::string& getTokenDescription() const;
    const std::string& getTokenIssuer() const;
    const std::string& getIconURL() const;
    const std::string& getIconImageType() const;
    friend inline bool operator==(const NTP1TokenMetaData& lhs, const NTP1TokenMetaData& rhs);
    void               setTokenName(const std::string& value);
};

bool operator==(const NTP1TokenMetaData& lhs, const NTP1TokenMetaData& rhs)
{
    return (lhs.getTokenId() == rhs.getTokenId() && lhs.getIssuanceTxId() == rhs.getIssuanceTxId() &&
            lhs.getDivisibility() == rhs.getDivisibility() &&
            lhs.getLockStatus() == rhs.getLockStatus() &&
            lhs.getAggregationPolicy() == rhs.getAggregationPolicy() &&
            lhs.totalSupply == rhs.totalSupply && lhs.tokenName == rhs.tokenName &&
            lhs.tokenDescription == rhs.tokenDescription && lhs.tokenIssuer == rhs.tokenIssuer &&
            lhs.iconURL == rhs.iconURL && lhs.iconImageType == rhs.iconImageType);
}

#endif // NTP1TOKENMETADATA_H

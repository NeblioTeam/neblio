#ifndef NTP1TOKENMETADATA_H
#define NTP1TOKENMETADATA_H

#include "base58.h"
#include "json_spirit.h"
#include "ntp1tokenminimalmetadata.h"
#include "uint256.h"
#include <vector>

class NTP1TokenMetaData : public NTP1TokenMinimalMetaData
{
    uint64_t           numOfHolders;
    uint64_t           totalSupply;
    uint64_t           numOfTransfers;
    uint64_t           numOfIssuance;
    uint64_t           numOfBurns;
    uint64_t           firstBlock;
    CBitcoinAddress    issueAddress;
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

    NTP1TokenMetaData();
    uint64_t               getNumOfHolders() const;
    uint64_t               getTotalSupply() const;
    uint64_t               getNumOfTransfers() const;
    uint64_t               getNumOfIssuance() const;
    uint64_t               getNumOfBurns() const;
    uint64_t               getFirstBlock() const;
    const CBitcoinAddress& getIssueAddress() const;
    const std::string&     getTokenName() const;
    const std::string&     getTokenDescription() const;
    const std::string&     getTokenIssuer() const;
    const std::string&     getIconURL() const;
    const std::string&     getIconImageType() const;
    friend inline bool     operator==(const NTP1TokenMetaData& lhs, const NTP1TokenMetaData& rhs);
    void                   setTokenName(const std::string& value);
};

bool operator==(const NTP1TokenMetaData& lhs, const NTP1TokenMetaData& rhs)
{
    return (lhs.getTokenId() == rhs.getTokenId() && lhs.getIssuanceTxId() == rhs.getIssuanceTxId() &&
            lhs.getDivisibility() == rhs.getDivisibility() &&
            lhs.getLockStatus() == rhs.getLockStatus() &&
            lhs.getAggregationPolicy() == rhs.getAggregationPolicy() &&
            lhs.numOfHolders == rhs.numOfHolders && lhs.totalSupply == rhs.totalSupply &&
            lhs.numOfTransfers == rhs.numOfTransfers && lhs.numOfIssuance == rhs.numOfIssuance &&
            lhs.numOfBurns == rhs.numOfBurns && lhs.firstBlock == rhs.firstBlock &&
            lhs.issueAddress == rhs.issueAddress && lhs.tokenName == rhs.tokenName &&
            lhs.tokenDescription == rhs.tokenDescription && lhs.tokenIssuer == rhs.tokenIssuer &&
            lhs.iconURL == rhs.iconURL && lhs.iconImageType == rhs.iconImageType);
}

#endif // NTP1TOKENMETADATA_H

#ifndef NTP1TOKENMETADATA_H
#define NTP1TOKENMETADATA_H

#include <vector>
#include "base58.h"
#include "uint256.h"
#include "json_spirit.h"

class NTP1TokenMetaData
{
    std::vector<unsigned char> tokenId;
    uint256 issuanceTxId;
    uint64_t divisibility;
    bool lockStatus;
    std::string aggregationPolicy;
    uint64_t numOfHolders;
    uint64_t totalSupply;
    uint64_t numOfTransfers;
    uint64_t numOfIssuance;
    uint64_t numOfBurns;
    uint64_t firstBlock;
    CBitcoinAddress issueAddress;
    std::string tokenName;
    std::string tokenDescription;
    std::string tokenIssuer;
    std::string iconURL;
    std::string iconImageType;
    std::string sha2Issue;

    void setTokenIdBase58(const std::string &Str);
    void setIssuanceTxIdHex(const std::string &hex);

public:
    void setNull();
    bool isNull() const;

    void importRestfulAPIJsonData(const std::string& data);
    void importRestfulAPIJsonData(const json_spirit::Value& data);
    json_spirit::Value exportDatabaseJsonData() const;
    void importDatabaseJsonData(const json_spirit::Value& data);

    NTP1TokenMetaData();
    std::string getTokenIdBase58() const;
    std::string getIssuanceTxIdHex() const;
    uint64_t getDivisibility() const;
    bool getLockStatus() const;
    const std::string& getAggregationPolicy() const;
    uint64_t getNumOfHolders() const;
    uint64_t getTotalSupply() const;
    uint64_t getNumOfTransfers() const;
    uint64_t getNumOfIssuance() const;
    uint64_t getNumOfBurns() const;
    uint64_t getFirstBlock() const;
    uint256 getIssuanceTxId() const;
    const CBitcoinAddress& getIssueAddress() const;
    const std::string& getTokenName() const;
    const std::string& getTokenDescription() const;
    const std::string& getTokenIssuer() const;
    const std::string& getIconURL() const;
    const std::string& getIconImageType() const;
    const std::string& getSha2Issue() const;
    friend inline bool operator==(const NTP1TokenMetaData& lhs, const NTP1TokenMetaData& rhs);
};

bool operator==(const NTP1TokenMetaData &lhs, const NTP1TokenMetaData &rhs)
{
    return (lhs.tokenId == rhs.tokenId &&
            lhs.issuanceTxId == rhs.issuanceTxId &&
            lhs.divisibility == rhs.divisibility &&
            lhs.lockStatus == rhs.lockStatus &&
            lhs.aggregationPolicy == rhs.aggregationPolicy &&
            lhs.numOfHolders == rhs.numOfHolders &&
            lhs.totalSupply == rhs.totalSupply &&
            lhs.numOfTransfers == rhs.numOfTransfers &&
            lhs.numOfIssuance == rhs.numOfIssuance &&
            lhs.numOfBurns == rhs.numOfBurns &&
            lhs.firstBlock == rhs.firstBlock &&
            lhs.issueAddress == rhs.issueAddress &&
            lhs.tokenName == rhs.tokenName &&
            lhs.tokenDescription == rhs.tokenDescription &&
            lhs.tokenIssuer == rhs.tokenIssuer &&
            lhs.iconURL == rhs.iconURL &&
            lhs.iconImageType == rhs.iconImageType &&
            lhs.sha2Issue == rhs.sha2Issue);
}

#endif // NTP1TOKENMETADATA_H

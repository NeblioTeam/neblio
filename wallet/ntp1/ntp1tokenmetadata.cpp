#include "ntp1tokenmetadata.h"

#include "ntp1tools.h"

NTP1TokenMetaData::NTP1TokenMetaData()
{

}

void NTP1TokenMetaData::setNull()
{
    tokenId.clear();
    issuanceTxId = 0;
    divisibility = -1;
    lockStatus = false;
    aggregationPolicy.clear();
    numOfHolders = 0;
    totalSupply = 0;
    numOfTransfers = 0;
    numOfIssuance = 0;
    numOfBurns = 0;
    firstBlock = 0;
    issueAddress = CBitcoinAddress();
    tokenName.clear();
    tokenDescription.clear();
    tokenIssuer.clear();
    iconURL.clear();
    iconImageType.clear();
    sha2Issue.clear();
}

bool NTP1TokenMetaData::isNull() const
{
    return tokenId.size() == 0;
}

void NTP1TokenMetaData::importJsonData(const std::string &data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);
        importJsonData(parsedData);
    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

void NTP1TokenMetaData::importJsonData(const json_spirit::Value &data)
{
    try {
        setTokenIdBase58(NTP1Tools::GetStrField(data.get_obj(), "tokenId"));
        setIssuanceTxIdHex(NTP1Tools::GetStrField(data.get_obj(), "issuanceTxid"));
        divisibility = NTP1Tools::GetUint64Field(data.get_obj(), "divisibility");
        lockStatus = NTP1Tools::GetBoolField(data.get_obj(), "lockStatus");
        aggregationPolicy = NTP1Tools::GetStrField(data.get_obj(), "aggregationPolicy");
        numOfHolders = NTP1Tools::GetUint64Field(data.get_obj(), "numOfHolders");
        totalSupply = NTP1Tools::GetUint64Field(data.get_obj(), "totalSupply");
        numOfTransfers = NTP1Tools::GetUint64Field(data.get_obj(), "numOfTransfers");
        numOfIssuance = NTP1Tools::GetUint64Field(data.get_obj(), "numOfIssuance");
        numOfBurns = NTP1Tools::GetUint64Field(data.get_obj(), "numOfBurns");
        firstBlock = NTP1Tools::GetUint64Field(data.get_obj(), "firstBlock");
        issueAddress = CBitcoinAddress(NTP1Tools::GetStrField(data.get_obj(), "issueAddress"));
        if(!issueAddress.IsValid()) {
            throw std::runtime_error("Address of token " + getTokenIdBase58() + " was issued to an invalid address.");
        }
        // fields inside metadata
        json_spirit::Object metadata = NTP1Tools::GetObjectField(data.get_obj(), "metadataOfIssuence");
        // data inside metadata
        json_spirit::Object innerdata = NTP1Tools::GetObjectField(metadata, "data");
        tokenName = NTP1Tools::GetStrField(innerdata, "tokenName");
        tokenDescription = NTP1Tools::GetStrField(innerdata, "description");
        tokenIssuer = NTP1Tools::GetStrField(innerdata, "issuer");
        try {
            json_spirit::Array urls = NTP1Tools::GetArrayField(innerdata, "urls");
            for(long i = 0; i < static_cast<long>(urls.size()); i++) {
                std::string urlName = NTP1Tools::GetStrField(urls[i].get_obj(), "name");
                if(urlName == "icon") {
                    iconImageType = NTP1Tools::GetStrField(urls[i].get_obj(), "mimeType");
                    iconURL = NTP1Tools::GetStrField(urls[i].get_obj(), "url");
                }
            }
        } catch (...) {}
        sha2Issue = NTP1Tools::GetStrField(data.get_obj(), "sha2Issue");

//        tokenName.clear();
//        tokenDescription.clear();
//        tokenIssuer.clear();
//        iconURL.clear();
//        iconType.clear();
//        sha2Issue.clear();

    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

void NTP1TokenMetaData::setTokenIdBase58(const std::string &Str)
{
    if(!DecodeBase58(Str, tokenId)) {
        std::string errorMsg = "Failed to decode token ID base58: " + Str;
        printf("%s", errorMsg.c_str());
        throw std::runtime_error(errorMsg.c_str());
    }
}

std::string NTP1TokenMetaData::getTokenIdBase58() const
{
    return EncodeBase58(tokenId);
}

std::string NTP1TokenMetaData::getIssuanceTxIdHex() const
{
    return issuanceTxId.ToString();
}

uint64_t NTP1TokenMetaData::getDivisibility() const
{
    return divisibility;
}

bool NTP1TokenMetaData::getLockStatus() const
{
    return lockStatus;
}

const std::string &NTP1TokenMetaData::getAggregationPolicy() const
{
    return aggregationPolicy;
}

uint64_t NTP1TokenMetaData::getNumOfHolders() const
{
    return numOfHolders;
}

uint64_t NTP1TokenMetaData::getTotalSupply() const
{
    return totalSupply;
}

uint64_t NTP1TokenMetaData::getNumOfTransfers() const
{
    return numOfTransfers;
}

uint64_t NTP1TokenMetaData::getNumOfIssuance() const
{
    return numOfIssuance;
}

uint64_t NTP1TokenMetaData::getNumOfBurns() const
{
    return numOfBurns;
}

uint64_t NTP1TokenMetaData::getFirstBlock() const
{
    return firstBlock;
}

uint256 NTP1TokenMetaData::getIssuanceTxId() const
{
    return issuanceTxId;
}

const CBitcoinAddress &NTP1TokenMetaData::getIssueAddress() const
{
    return issueAddress;
}

const std::string &NTP1TokenMetaData::getTokenName() const
{
    return tokenName;
}

const std::string &NTP1TokenMetaData::getTokenDescription() const
{
    return tokenDescription;
}

const std::string &NTP1TokenMetaData::getTokenIssuer() const
{
    return tokenIssuer;
}

const std::string &NTP1TokenMetaData::getIconURL() const
{
    return iconURL;
}

const std::string &NTP1TokenMetaData::getIconImageType() const
{
    return iconImageType;
}

const std::string &NTP1TokenMetaData::getSha2Issue() const
{
    return sha2Issue;
}

void NTP1TokenMetaData::setIssuanceTxIdHex(const std::string &hex)
{
    issuanceTxId.SetHex(hex);
}

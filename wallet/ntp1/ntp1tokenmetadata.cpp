#include "ntp1tokenmetadata.h"

#include "ntp1tools.h"

NTP1TokenMetaData::NTP1TokenMetaData() { setNull(); }

void NTP1TokenMetaData::setTokenName(const std::string& value) { tokenName = value; }

void NTP1TokenMetaData::setNull()
{
    numOfHolders   = 0;
    totalSupply    = 0;
    numOfTransfers = 0;
    numOfIssuance  = 0;
    numOfBurns     = 0;
    firstBlock     = 0;
    issueAddress   = CBitcoinAddress();
    tokenName.clear();
    tokenDescription.clear();
    tokenIssuer.clear();
    iconURL.clear();
    iconImageType.clear();
}

bool NTP1TokenMetaData::isNull() const { return getTokenId().size() == 0; }

void NTP1TokenMetaData::importRestfulAPIJsonData(const std::string& data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);
        importRestfulAPIJsonData(parsedData);
    } catch (std::exception& ex) {
        printf("%s\n", ex.what());
        throw;
    }
}

void NTP1TokenMetaData::importRestfulAPIJsonData(const json_spirit::Value& data)
{
    try {
        setTokenId(NTP1Tools::GetStrField(data.get_obj(), "tokenId"));
        setIssuanceTxIdHex(NTP1Tools::GetStrField(data.get_obj(), "issuanceTxid"));
        divisibility      = NTP1Tools::GetUint64Field(data.get_obj(), "divisibility");
        lockStatus        = NTP1Tools::GetBoolField(data.get_obj(), "lockStatus");
        aggregationPolicy = NTP1Tools::GetStrField(data.get_obj(), "aggregationPolicy");
        numOfHolders      = NTP1Tools::GetUint64Field(data.get_obj(), "numOfHolders");
        totalSupply       = NTP1Tools::GetUint64Field(data.get_obj(), "totalSupply");
        numOfTransfers    = NTP1Tools::GetUint64Field(data.get_obj(), "numOfTransfers");
        numOfIssuance     = NTP1Tools::GetUint64Field(data.get_obj(), "numOfIssuance");
        numOfBurns        = NTP1Tools::GetUint64Field(data.get_obj(), "numOfBurns");
        firstBlock        = NTP1Tools::GetUint64Field(data.get_obj(), "firstBlock");
        issueAddress      = CBitcoinAddress(NTP1Tools::GetStrField(data.get_obj(), "issueAddress"));
        if (!issueAddress.IsValid()) {
            throw std::runtime_error("Address of token " + getTokenId() +
                                     " was issued to an invalid address.");
        }
        // fields inside metadata
        json_spirit::Object metadata = NTP1Tools::GetObjectField(data.get_obj(), "metadataOfIssuence");
        // data inside metadata
        json_spirit::Object innerdata = NTP1Tools::GetObjectField(metadata, "data");
        tokenName                     = NTP1Tools::GetStrField(innerdata, "tokenName");
        tokenDescription              = NTP1Tools::GetStrField(innerdata, "description");
        tokenIssuer                   = NTP1Tools::GetStrField(innerdata, "issuer");
        try {
            json_spirit::Array urlsArray = NTP1Tools::GetArrayField(innerdata, "urls");
            urls = json_spirit::Value(NTP1Tools::GetArrayField(innerdata, "urls"));
            for (long i = 0; i < static_cast<long>(urlsArray.size()); i++) {
                std::string urlName = NTP1Tools::GetStrField(urlsArray[i].get_obj(), "name");
                if (urlName == "icon") {
                    iconImageType = NTP1Tools::GetStrField(urlsArray[i].get_obj(), "mimeType");
                    iconURL       = NTP1Tools::GetStrField(urlsArray[i].get_obj(), "url");
                }
            }
            userData = NTP1Tools::GetObjectField(innerdata, "userData");
        } catch (...) {
        }
    } catch (std::exception& ex) {
        printf("%s\n", ex.what());
        throw;
    }
}

json_spirit::Value NTP1TokenMetaData::exportDatabaseJsonData(bool for_rpc) const
{
    json_spirit::Object root;

    if (!for_rpc) {
        root.push_back(json_spirit::Pair("userData", userData));
        root.push_back(json_spirit::Pair("urls", urls));
        root.push_back(json_spirit::Pair("tokenId", getTokenId()));
        root.push_back(json_spirit::Pair("issuanceTxid", getIssuanceTxIdHex()));
        root.push_back(json_spirit::Pair("divisibility", divisibility));
        root.push_back(json_spirit::Pair("lockStatus", static_cast<bool>(lockStatus)));
        root.push_back(json_spirit::Pair("aggregationPolicy", aggregationPolicy));
        root.push_back(json_spirit::Pair("numOfHolders", numOfHolders));
        root.push_back(json_spirit::Pair("totalSupply", totalSupply));
        root.push_back(json_spirit::Pair("numOfTransfers", numOfTransfers));
        root.push_back(json_spirit::Pair("numOfIssuance", numOfIssuance));
        root.push_back(json_spirit::Pair("numOfBurns", numOfBurns));
        root.push_back(json_spirit::Pair("firstBlock", firstBlock));
        root.push_back(json_spirit::Pair("issueAddress", issueAddress.ToString()));
        root.push_back(json_spirit::Pair("tokenName", tokenName));
        root.push_back(json_spirit::Pair("tokenDescription", tokenDescription));
        root.push_back(json_spirit::Pair("tokenIssuer", tokenIssuer));
        root.push_back(json_spirit::Pair("iconURL", iconURL));
        root.push_back(json_spirit::Pair("iconImageType", iconImageType));

        return json_spirit::Value(root);
    } else {
        root.push_back(json_spirit::Pair("tokenName", tokenName));
        root.push_back(json_spirit::Pair("description", tokenDescription));
        root.push_back(json_spirit::Pair("urls", urls));
        root.push_back(json_spirit::Pair("issuer", tokenIssuer));
        root.push_back(json_spirit::Pair("userData", userData));
        json_spirit::Value  rV(root);
        json_spirit::Object data;
        data.push_back(json_spirit::Pair("data", rV));

        return data;
    }
}

void NTP1TokenMetaData::importDatabaseJsonData(const json_spirit::Value& data)
{
    setNull();

    setTokenId(NTP1Tools::GetStrField(data.get_obj(), "tokenId"));
    setIssuanceTxIdHex(NTP1Tools::GetStrField(data.get_obj(), "issuanceTxid"));
    divisibility      = NTP1Tools::GetUint64Field(data.get_obj(), "divisibility");
    lockStatus        = (int)NTP1Tools::GetBoolField(data.get_obj(), "lockStatus");
    aggregationPolicy = NTP1Tools::GetStrField(data.get_obj(), "aggregationPolicy");
    numOfHolders      = NTP1Tools::GetUint64Field(data.get_obj(), "numOfHolders");
    totalSupply       = NTP1Tools::GetUint64Field(data.get_obj(), "totalSupply");
    numOfTransfers    = NTP1Tools::GetUint64Field(data.get_obj(), "numOfTransfers");
    numOfIssuance     = NTP1Tools::GetUint64Field(data.get_obj(), "numOfIssuance");
    numOfBurns        = NTP1Tools::GetUint64Field(data.get_obj(), "numOfBurns");
    firstBlock        = NTP1Tools::GetUint64Field(data.get_obj(), "firstBlock");
    issueAddress      = CBitcoinAddress(NTP1Tools::GetStrField(data.get_obj(), "issueAddress"));
    tokenName         = NTP1Tools::GetStrField(data.get_obj(), "tokenName");
    tokenDescription  = NTP1Tools::GetStrField(data.get_obj(), "tokenDescription");
    tokenIssuer       = NTP1Tools::GetStrField(data.get_obj(), "tokenIssuer");
    iconURL           = NTP1Tools::GetStrField(data.get_obj(), "iconURL");
    iconImageType     = NTP1Tools::GetStrField(data.get_obj(), "iconImageType");
}

uint64_t NTP1TokenMetaData::getNumOfHolders() const { return numOfHolders; }

uint64_t NTP1TokenMetaData::getTotalSupply() const { return totalSupply; }

uint64_t NTP1TokenMetaData::getNumOfTransfers() const { return numOfTransfers; }

uint64_t NTP1TokenMetaData::getNumOfIssuance() const { return numOfIssuance; }

uint64_t NTP1TokenMetaData::getNumOfBurns() const { return numOfBurns; }

uint64_t NTP1TokenMetaData::getFirstBlock() const { return firstBlock; }

const CBitcoinAddress& NTP1TokenMetaData::getIssueAddress() const { return issueAddress; }

const std::string& NTP1TokenMetaData::getTokenName() const { return tokenName; }

const std::string& NTP1TokenMetaData::getTokenDescription() const { return tokenDescription; }

const std::string& NTP1TokenMetaData::getTokenIssuer() const { return tokenIssuer; }

const std::string& NTP1TokenMetaData::getIconURL() const { return iconURL; }

const std::string& NTP1TokenMetaData::getIconImageType() const { return iconImageType; }

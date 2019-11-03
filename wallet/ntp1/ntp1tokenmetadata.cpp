#include "ntp1tokenmetadata.h"

#include "ntp1tools.h"

#include "ntp1script_issuance.h"

NTP1TokenMetaData::NTP1TokenMetaData() { setNull(); }

void NTP1TokenMetaData::setTokenName(const std::string& value) { tokenName = value; }

void NTP1TokenMetaData::setNull()
{
    totalSupply = 0;
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
        totalSupply       = NTP1Tools::GetUint64Field(data.get_obj(), "totalSupply");
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
        root.push_back(json_spirit::Pair("totalSupply", totalSupply.str()));
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
    totalSupply       = FromString<NTP1Int>(NTP1Tools::GetStrField(data.get_obj(), "totalSupply"));
    tokenName         = NTP1Tools::GetStrField(data.get_obj(), "tokenName");
    tokenDescription  = NTP1Tools::GetStrField(data.get_obj(), "tokenDescription");
    tokenIssuer       = NTP1Tools::GetStrField(data.get_obj(), "tokenIssuer");
    iconURL           = NTP1Tools::GetStrField(data.get_obj(), "iconURL");
    iconImageType     = NTP1Tools::GetStrField(data.get_obj(), "iconImageType");
}

void NTP1TokenMetaData::readSomeDataFromStandardJsonFormat(const json_spirit::Value& data)
{
    json_spirit::Object dataObj = NTP1Tools::GetObjectField(data.get_obj(), "data");

    this->tokenName        = NTP1Tools::GetStrField(dataObj, "tokenName");
    this->tokenDescription = NTP1Tools::GetStrField(dataObj, "description");
    this->tokenIssuer      = NTP1Tools::GetStrField(dataObj, "issuer");
    try {
        json_spirit::Array urlsArray = NTP1Tools::GetArrayField(dataObj, "urls");
        this->urls                   = json_spirit::Value(NTP1Tools::GetArrayField(dataObj, "urls"));
        for (long i = 0; i < static_cast<long>(urlsArray.size()); i++) {
            std::string urlName = NTP1Tools::GetStrField(urlsArray[i].get_obj(), "name");
            if (urlName == "icon") {
                this->iconImageType = NTP1Tools::GetStrField(urlsArray[i].get_obj(), "mimeType");
                this->iconURL       = NTP1Tools::GetStrField(urlsArray[i].get_obj(), "url");
                break;
            }
        }
        this->userData = NTP1Tools::GetObjectField(dataObj, "userData");
    } catch (...) {
    }
}

void NTP1TokenMetaData::readSomeDataFromNTP1IssuanceScript(NTP1Script_Issuance* sd)
{
    this->totalSupply       = sd->getAmount();
    this->tokenName         = sd->getTokenSymbol();
    this->divisibility      = sd->getDivisibility();
    this->aggregationPolicy = sd->getAggregationPolicyStr();
    this->lockStatus        = sd->isLocked();
}

NTP1Int NTP1TokenMetaData::getTotalSupply() const { return totalSupply; }

const std::string& NTP1TokenMetaData::getTokenName() const { return tokenName; }

const std::string& NTP1TokenMetaData::getTokenDescription() const { return tokenDescription; }

const std::string& NTP1TokenMetaData::getTokenIssuer() const { return tokenIssuer; }

const std::string& NTP1TokenMetaData::getIconURL() const { return iconURL; }

const std::string& NTP1TokenMetaData::getIconImageType() const { return iconImageType; }

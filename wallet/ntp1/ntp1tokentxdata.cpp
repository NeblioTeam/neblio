#include "ntp1tokentxdata.h"
#include "ntp1tools.h"

#include "base58.h"

void NTP1TokenTxData::setDivisibility(const uint64_t& value) { divisibility = value; }

void NTP1TokenTxData::setLockStatus(bool value) { lockStatus = static_cast<int>(value); }

void NTP1TokenTxData::setAggregationPolicy(const std::string& value) { aggregationPolicy = value; }

std::string NTP1TokenTxData::getTokenSymbol() const { return tokenSymbol; }

void NTP1TokenTxData::setTokenSymbol(const std::string& value) { tokenSymbol = value; }

NTP1TokenTxData::NTP1TokenTxData() { setNull(); }

void NTP1TokenTxData::setNull()
{
    tokenId.clear();
    amount       = 0;
    issueTxId    = 0;
    divisibility = 0;
    lockStatus   = 0;
    aggregationPolicy.clear();
}

void NTP1TokenTxData::setTokenId(const std::string& Str) { tokenId = Str; }

void NTP1TokenTxData::setIssueTxIdHex(const std::string& hex) { issueTxId.SetHex(hex); }

void NTP1TokenTxData::importJsonData(const std::string& data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);
        importJsonData(parsedData);
    } catch (std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

void NTP1TokenTxData::importJsonData(const json_spirit::Value& data)
{
    try {
        setTokenId(NTP1Tools::GetStrField(data.get_obj(), "tokenId"));
        setIssueTxIdHex(NTP1Tools::GetStrField(data.get_obj(), "issueTxid"));
        amount            = NTP1Tools::GetUint64Field(data.get_obj(), "amount");
        divisibility      = NTP1Tools::GetUint64Field(data.get_obj(), "divisibility");
        lockStatus        = (int)NTP1Tools::GetBoolField(data.get_obj(), "lockStatus");
        aggregationPolicy = NTP1Tools::GetStrField(data.get_obj(), "aggregationPolicy");
    } catch (std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

json_spirit::Value NTP1TokenTxData::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("tokenId", getTokenId()));
    root.push_back(json_spirit::Pair("issueTxid", getIssueTxId().ToString()));
    root.push_back(json_spirit::Pair("amount", ToString(amount)));
    root.push_back(json_spirit::Pair("divisibility", divisibility));
    root.push_back(json_spirit::Pair("lockStatus", static_cast<bool>(lockStatus)));
    root.push_back(json_spirit::Pair("aggregationPolicy", aggregationPolicy));

    return json_spirit::Value(root);
}

void NTP1TokenTxData::importDatabaseJsonData(const json_spirit::Value& data)
{
    setNull();

    setTokenId(NTP1Tools::GetStrField(data.get_obj(), "tokenId"));
    setIssueTxIdHex(NTP1Tools::GetStrField(data.get_obj(), "issueTxid"));
    amount            = NTP1Tools::GetNTP1IntField(data.get_obj(), "amount");
    divisibility      = NTP1Tools::GetUint64Field(data.get_obj(), "divisibility");
    lockStatus        = (int)NTP1Tools::GetBoolField(data.get_obj(), "lockStatus");
    aggregationPolicy = NTP1Tools::GetStrField(data.get_obj(), "aggregationPolicy");
}

std::string NTP1TokenTxData::getTokenId() const { return tokenId; }

NTP1Int NTP1TokenTxData::getAmount() const { return FromString<NTP1Int>(amount); }

void NTP1TokenTxData::setAmount(const NTP1Int& value) { amount = value; }

uint64_t NTP1TokenTxData::getDivisibility() const { return divisibility; }

uint256 NTP1TokenTxData::getIssueTxId() const { return issueTxId; }

bool NTP1TokenTxData::getLockStatus() const { return static_cast<bool>(lockStatus); }

const std::string& NTP1TokenTxData::getAggregationPolicy() const { return aggregationPolicy; }

#include "ntp1tokentxdata.h"

#include "base58.h"

NTP1TokenTxData::NTP1TokenTxData()
{

}

void NTP1TokenTxData::setNull()
{
    tokenId.clear();
    amount = 0;
    issueTxId = 0;
    divisibility = 0;
    lockStatus = false;
    aggregationPolicy.clear();
}

void NTP1TokenTxData::setTokenIdBase58(const std::string &Str)
{
    if(!DecodeBase58(Str, tokenId)) {
        std::string errorMsg = "Failed to decode token ID base58: " + Str;
        printf("%s", errorMsg.c_str());
        throw std::runtime_error(errorMsg.c_str());
    }
}

void NTP1TokenTxData::setIssueTxIdHex(const std::string &hex)
{
    issueTxId.SetHex(hex);
}

void NTP1TokenTxData::importJsonData(const std::string &data)
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

void NTP1TokenTxData::importJsonData(const json_spirit::Value &data)
{
    try {
        setTokenIdBase58(NTP1Tools::GetStrField(data.get_obj(), "tokenId"));
        setIssueTxIdHex(NTP1Tools::GetStrField(data.get_obj(), "issueTxid"));
        amount = NTP1Tools::GetUint64Field(data.get_obj(), "amount");
        divisibility = NTP1Tools::GetUint64Field(data.get_obj(), "divisibility");
        lockStatus = NTP1Tools::GetBoolField(data.get_obj(), "lockStatus");
        aggregationPolicy = NTP1Tools::GetStrField(data.get_obj(), "aggregationPolicy");
    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

std::string NTP1TokenTxData::getTokenIdBase58() const
{
    return EncodeBase58(tokenId);
}

uint64_t NTP1TokenTxData::getAmount() const
{
    return amount;
}

uint64_t NTP1TokenTxData::getDivisibility() const
{
    return divisibility;
}

uint256 NTP1TokenTxData::getIssueTxId() const
{
    return issueTxId;
}

bool NTP1TokenTxData::getLockStatus() const
{
    return lockStatus;
}

const std::string &NTP1TokenTxData::getAggregationPolicy() const
{
    return aggregationPolicy;
}


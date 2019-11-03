#include "ntp1tokenminimalmetadata.h"

void NTP1TokenMinimalMetaData::setIssuanceTxId(const uint256& value) { issuanceTxId = value; }

NTP1TokenMinimalMetaData::NTP1TokenMinimalMetaData() { setNull(); }

void NTP1TokenMinimalMetaData::setNull()
{
    tokenId.clear();
    issuanceTxId = 0;
    divisibility = -1;
    lockStatus   = false;
    aggregationPolicy.clear();
}

void NTP1TokenMinimalMetaData::setTokenId(const std::string& Str) { tokenId = Str; }

const std::string& NTP1TokenMinimalMetaData::getTokenId() const { return tokenId; }

std::string NTP1TokenMinimalMetaData::getIssuanceTxIdHex() const { return issuanceTxId.ToString(); }

uint64_t NTP1TokenMinimalMetaData::getDivisibility() const { return divisibility; }

bool NTP1TokenMinimalMetaData::getLockStatus() const { return lockStatus; }

const std::string& NTP1TokenMinimalMetaData::getAggregationPolicy() const { return aggregationPolicy; }

void NTP1TokenMinimalMetaData::setIssuanceTxIdHex(const std::string& hex) { issuanceTxId.SetHex(hex); }

uint256 NTP1TokenMinimalMetaData::getIssuanceTxId() const { return issuanceTxId; }

#include "ntp1transaction.h"

#include "util.h"

#include <boost/algorithm/hex.hpp>

NTP1Transaction::NTP1Transaction()
{
    setNull();
}

void NTP1Transaction::setNull()
{
    nVersion = NTP1Transaction::CURRENT_VERSION;
    nTime = GetAdjustedTime();
    vin.clear();
    vout.clear();
    nLockTime = 0;
}

bool NTP1Transaction::isNull() const
{
    return (vin.empty() && vout.empty());
}

void NTP1Transaction::importJsonData(const std::string &data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);
        std::string hex = NTP1Tools::GetStrField(parsedData.get_obj(), "hex");
        boost::algorithm::unhex(hex.begin(), hex.end(), std::back_inserter(txSerialized));
        assert(hex.size()/2 == txSerialized.size());
        std::string hash = NTP1Tools::GetStrField(parsedData.get_obj(), "txid");
        txHash.SetHex(hash);
        nLockTime = NTP1Tools::GetUint64Field(parsedData.get_obj(), "locktime");
        nTime = NTP1Tools::GetUint64Field(parsedData.get_obj(), "time");
        nVersion = NTP1Tools::GetUint64Field(parsedData.get_obj(), "version");
        json_spirit::Array vin_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "vin");
        vin.clear();
        vin.resize(vin_list.size());
        for(unsigned long i = 0; i < vin_list.size(); i++) {
            vin[i].importJsonData(vin_list[i]);
        }
        json_spirit::Array vout_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "vout");
        vout.clear();
        vout.resize(vout_list.size());
        for(unsigned long i = 0; i < vout_list.size(); i++) {
            vout[i].importJsonData(vout_list[i]);
        }
    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

std::string NTP1Transaction::getHex() const
{
    std::string out;
    boost::algorithm::hex(txSerialized.begin(), txSerialized.end(), std::back_inserter(out));
    return out;
}

uint256 NTP1Transaction::getTxHash() const
{
    return txHash;
}

uint64_t NTP1Transaction::getLockTime() const
{
    return nLockTime;
}

uint64_t NTP1Transaction::getTime() const
{
    return nTime;
}

unsigned long NTP1Transaction::getTxInCount() const
{
    return vin.size();
}

const NTP1TxIn &NTP1Transaction::getTxIn(unsigned long index) const
{
    return vin[index];
}

unsigned long NTP1Transaction::getTxOutCount() const
{
    return vout.size();
}

const NTP1TxOut &NTP1Transaction::getTxOut(unsigned long index) const
{
    return vout[index];
}

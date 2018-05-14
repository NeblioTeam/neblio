#include "ntp1transaction.h"
#include "ntp1tools.h"
#include "ntp1txin.h"
#include "ntp1txout.h"

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

        setHex(NTP1Tools::GetStrField(parsedData.get_obj(), "hex"));
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

json_spirit::Value NTP1Transaction::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("version", nVersion));
    root.push_back(json_spirit::Pair("txid", txHash.GetHex()));
    root.push_back(json_spirit::Pair("locktime", nLockTime));
    root.push_back(json_spirit::Pair("time", nTime));
    root.push_back(json_spirit::Pair("hex", getHex()));

    json_spirit::Array vinArray;
    for(long i = 0; i < static_cast<long>(vin.size()); i++) {
        vinArray.push_back(vin[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("vin", json_spirit::Value(vinArray)));

    json_spirit::Array voutArray;
    for(long i = 0; i < static_cast<long>(vout.size()); i++) {
        voutArray.push_back(vout[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("vout", json_spirit::Value(voutArray)));

    return json_spirit::Value(root);
}

void NTP1Transaction::importDatabaseJsonData(const json_spirit::Value &data)
{
    setNull();

    nVersion = NTP1Tools::GetUint64Field(data.get_obj(), "version");
    txHash.SetHex(NTP1Tools::GetStrField(data.get_obj(), "txid"));
    nLockTime = NTP1Tools::GetUint64Field(data.get_obj(), "locktime");
    nTime = NTP1Tools::GetUint64Field(data.get_obj(), "time");
    setHex(NTP1Tools::GetStrField(data.get_obj(), "hex"));

    json_spirit::Array vin_list = NTP1Tools::GetArrayField(data.get_obj(), "vin");
    vin.clear();
    vin.resize(vin_list.size());
    for(unsigned long i = 0; i < vin_list.size(); i++) {
        vin[i].importDatabaseJsonData(vin_list[i]);
    }

    json_spirit::Array vout_list = NTP1Tools::GetArrayField(data.get_obj(), "vout");
    vout.clear();
    vout.resize(vout_list.size());
    for(unsigned long i = 0; i < vout_list.size(); i++) {
        vout[i].importDatabaseJsonData(vout_list[i]);
    }
}

std::string NTP1Transaction::getHex() const
{
    std::string out;
    boost::algorithm::hex(txSerialized.begin(), txSerialized.end(), std::back_inserter(out));
    return out;
}

void NTP1Transaction::setHex(const std::string& Hex)
{
    txSerialized.clear();
    boost::algorithm::unhex(Hex.begin(), Hex.end(), std::back_inserter(txSerialized));
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

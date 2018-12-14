#include "ntp1outpoint.h"
#include "ntp1tools.h"

NTP1OutPoint::NTP1OutPoint() { setNull(); }

NTP1OutPoint::NTP1OutPoint(const uint256& hashIn, unsigned int indexIn)
{
    hash    = hashIn;
    index   = indexIn;
    hashStr = hashIn.ToString();
}

void NTP1OutPoint::setNull()
{
    hash    = 0;
    index   = (unsigned int)-1;
    hashStr = "";
}

bool NTP1OutPoint::isNull() const { return (hash == 0 && index == (unsigned int)-1); }

uint256 NTP1OutPoint::getHash() const { return hash; }

unsigned int NTP1OutPoint::getIndex() const { return index; }

json_spirit::Value NTP1OutPoint::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("hash", hash.ToString()));
    root.push_back(json_spirit::Pair("index", uint64_t(index)));

    return json_spirit::Value(root);
}

void NTP1OutPoint::importDatabaseJsonData(const json_spirit::Value& data)
{
    setNull();

    index = NTP1Tools::GetUint64Field(data.get_obj(), "index");
    hash.SetHex(NTP1Tools::GetStrField(data.get_obj(), "hash"));
}

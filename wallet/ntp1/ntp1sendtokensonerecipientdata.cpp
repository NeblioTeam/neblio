#include "ntp1sendtokensonerecipientdata.h"
#include "util.h"

json_spirit::Object NTP1SendTokensOneRecipientData::exportJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("address", json_spirit::Value(destination)));
    root.push_back(json_spirit::Pair("amount", json_spirit::Value(ToString(amount))));
    root.push_back(json_spirit::Pair("tokenId", json_spirit::Value(tokenId)));

    return root;
}

std::string NTP1SendTokensOneRecipientData::exportJsonDataAsString() const
{
    std::stringstream s;
    json_spirit::write(exportJsonData(), s);
    return s.str();
}

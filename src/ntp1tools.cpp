#include "ntp1tools.h"

const std::string NTP1Tools::NTPAPIURL_prefix_local    = "https://ntp1node.nebl.io:8080/v3/addressinfo/";
const std::string NTP1Tools::NTPAPIURL_prefix_external = "https://ntp1node.nebl.io:8080/v3/addressinfo/";

NTP1Tools::NTP1Tools()
{

}

std::string NTP1Tools::GetStrField(const json_spirit::Object &data, const std::string &fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_str();
}

bool NTP1Tools::GetBoolField(const json_spirit::Object &data, const std::string &fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_bool();
}

json_spirit::Array NTP1Tools::GetArrayField(const json_spirit::Object &data, const std::string &fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_array();
}

bool NTP1Tools::AddressContainsNTP1Tokens(const std::string& address)
{
    try {
        std::string addressNTPInfoURL = GetRestAPIAddressURL(address);
        std::string ntpData = cURLTools::GetFileFromHTTPS(addressNTPInfoURL, false);
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(ntpData, parsedData);
        json_spirit::Array utxosArray = GetArrayField(parsedData.get_obj(), "utxos");
        for(long i = 0; i < static_cast<long>(utxosArray.size()); i++) {
            json_spirit::Array tokensArray = GetArrayField(utxosArray[i].get_obj(), "tokens");
            if(tokensArray.size() > 0) {
                return true;
            }
        }
        return false;
    } catch(std::exception& ex) {
        printf("%s",ex.what());
        throw;
    }
}

std::string NTP1Tools::GetRestAPIAddressURL(const std::string &address)
{
#ifdef NEBLIO_REST
    return NTPAPIURL_prefix_local + address;
#else
    return NTPAPIURL_prefix_external + address;
#endif
}

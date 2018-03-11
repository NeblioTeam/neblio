#ifndef NTP1TOOLS_H
#define NTP1TOOLS_H

#include "curltools.h"
#include "json_spirit.h"
#include "json/json_spirit.h"

class NTP1Tools
{
public:
    NTP1Tools();

    static const std::string NTPAPIURL_prefix_local;
    static const std::string NTPAPIURL_prefix_external;

    static std::string GetStrField(const json_spirit::Object &data, const std::string &fieldName);
    static bool GetBoolField(const json_spirit::Object &data, const std::string &fieldName);
    static bool AddressContainsNTP1Tokens(const std::string &address);
    static long GetNumOfNTP1UTXOs(const std::string &address);
    static std::string GetRestAPIAddressURL(const std::string& address);
    static json_spirit::Array GetArrayField(const json_spirit::Object &data, const std::string &fieldName);
};

#endif // NTP1TOOLS_H

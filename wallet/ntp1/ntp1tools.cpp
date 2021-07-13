#include "ntp1tools.h"

#include "util.h"

const std::string NTP1Tools::NTPAPI_base_url_mainnet_local = "https://ntp1node.nebl.io/ntp1/";
const std::string NTP1Tools::NTPAPI_base_url_testnet_local = "https://ntp1node.nebl.io/testnet/ntp1/";

const std::string NTP1Tools::NTPAPI_base_url_mainnet_remote = "https://ntp1node.nebl.io/ntp1/";
const std::string NTP1Tools::NTPAPI_base_url_testnet_remote = "https://ntp1node.nebl.io/testnet/ntp1/";

const std::string NTP1Tools::NTPAPI_addressInfo     = "addressinfo/";
const std::string NTP1Tools::NTPAPI_transactionInfo = "transactioninfo/";
const std::string NTP1Tools::NTPAPI_tokenId         = "tokenid/";
const std::string NTP1Tools::NTPAPI_tokenMetaData   = "tokenmetadata/";
const std::string NTP1Tools::NTPAPI_stakeHolders    = "stakeholders/";
const std::string NTP1Tools::NTPAPI_sendTokens      = "sendtoken/";

const std::string NTP1Tools::EXPLORER_base_url_testnet = "https://testnet-explorer.nebl.io/";
const std::string NTP1Tools::EXPLORER_base_url_mainnet = "https://explorer.nebl.io/";

const std::string NTP1Tools::EXPLORER_tokenInfo       = "token/";
const std::string NTP1Tools::EXPLORER_transactionInfo = "tx/";

NTP1Tools::NTP1Tools() {}

std::string NTP1Tools::GetStrField(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_str();
}

bool NTP1Tools::GetBoolField(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_bool();
}

uint64_t NTP1Tools::GetUint64Field(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_uint64();
}

NTP1Int NTP1Tools::GetNTP1IntField(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return FromString<NTP1Int>(val.get_str());
}

int NTP1Tools::GetIntField(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_int();
}

bool NTP1Tools::GetFieldExists(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return !(val == json_spirit::Value::null);
}

int64_t NTP1Tools::GetInt64Field(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_int64();
}

std::string NTP1Tools::GetURL_APIBase(NetworkType netType)
{
    switch (netType) {
    case NetworkType::Mainnet:
        return NTPAPI_base_url_mainnet_remote;
    case NetworkType::Testnet:
        return NTPAPI_base_url_testnet_remote;
    default:
        return "";
    }
}

json_spirit::Array NTP1Tools::GetArrayField(const json_spirit::Object& data,
                                            const std::string&         fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_array();
}

json_spirit::Object NTP1Tools::GetObjectField(const json_spirit::Object& data,
                                              const std::string&         fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_obj();
}

std::string NTP1Tools::GetURL_TransactionInfo(const std::string& txHash, NetworkType netType)
{
    return GetURL_APIBase(netType) + NTPAPI_transactionInfo + txHash;
}

std::string NTP1Tools::GetURL_TokenID(const std::string& tokenSymbol, NetworkType netType)
{
    return GetURL_APIBase(netType) + NTPAPI_tokenId + tokenSymbol;
}

std::string NTP1Tools::GetURL_TokenMetaData(const std::string& tokenID, NetworkType netType)
{
    return GetURL_APIBase(netType) + NTPAPI_tokenMetaData + tokenID;
}

std::string NTP1Tools::GetURL_TokenUTXOMetaData(const std::string& tokenID, const std::string& txHash,
                                                unsigned long outputIndex, NetworkType netType)
{
    return GetURL_APIBase(netType) + NTPAPI_tokenMetaData + tokenID + "/" + txHash + ":" +
           ToString(outputIndex);
}

std::string NTP1Tools::GetURL_StakeHolders(const std::string& tokenID, NetworkType netType)
{
    return GetURL_APIBase(netType) + NTPAPI_stakeHolders + tokenID;
}

std::string NTP1Tools::GetURL_AddressInfo(const std::string& address, NetworkType netType)
{
    return GetURL_APIBase(netType) + NTPAPI_addressInfo + address;
}

std::string NTP1Tools::GetURL_SendTokens(NetworkType netType)
{
    return GetURL_APIBase(netType) + NTPAPI_sendTokens;
}

std::string NTP1Tools::GetURL_ExplorerBase(NetworkType netType)
{
    return (netType ? EXPLORER_base_url_testnet : EXPLORER_base_url_mainnet);
}

std::string NTP1Tools::GetURL_ExplorerTokenInfo(const std::string& tokenId, NetworkType netType)
{
    return GetURL_ExplorerBase(netType) + EXPLORER_tokenInfo + tokenId;
}

std::string NTP1Tools::GetURL_ExplorerTransactionInfo(const std::string& txId, NetworkType netType)
{
    return GetURL_ExplorerBase(netType) + EXPLORER_transactionInfo + txId;
}

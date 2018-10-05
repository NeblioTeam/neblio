#include "ntp1tools.h"

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

const std::string NTP1Tools::EXPLORER_base_url_testnet = "https://testnet.explorer.nebl.io/";
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

int64_t NTP1Tools::GetInt64Field(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_int64();
}

std::string NTP1Tools::GetURL_APIBase(bool testnet)
{
#ifdef NEBLIO_REST
    return (testnet ? NTPAPI_base_url_testnet_local : NTPAPI_base_url_mainnet_local);
#else
    return (testnet ? NTPAPI_base_url_testnet_remote : NTPAPI_base_url_mainnet_remote);
#endif
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

std::string NTP1Tools::GetURL_TransactionInfo(const std::string& txHash, bool testnet)
{
    return GetURL_APIBase(testnet) + NTPAPI_transactionInfo + txHash;
}

std::string NTP1Tools::GetURL_TokenID(const std::string& tokenSymbol, bool testnet)
{
    return GetURL_APIBase(testnet) + NTPAPI_tokenId + tokenSymbol;
}

std::string NTP1Tools::GetURL_TokenMetaData(const std::string& tokenID, bool testnet)
{
    return GetURL_APIBase(testnet) + NTPAPI_tokenMetaData + tokenID;
}

std::string NTP1Tools::GetURL_TokenUTXOMetaData(const std::string& tokenID, const std::string& txHash,
                                                unsigned long outputIndex, bool testnet)
{
    return GetURL_APIBase(testnet) + NTPAPI_tokenMetaData + tokenID + "/" + txHash + ":" +
           ToString(outputIndex);
}

std::string NTP1Tools::GetURL_StakeHolders(const std::string& tokenID, bool testnet)
{
    return GetURL_APIBase(testnet) + NTPAPI_stakeHolders + tokenID;
}

std::string NTP1Tools::GetURL_AddressInfo(const std::string& address, bool testnet)
{
    return GetURL_APIBase(testnet) + NTPAPI_addressInfo + address;
}

std::string NTP1Tools::GetURL_SendTokens(bool testnet)
{
    return GetURL_APIBase(testnet) + NTPAPI_sendTokens;
}

std::string NTP1Tools::GetURL_ExplorerBase(bool testnet)
{
    return (testnet ? EXPLORER_base_url_testnet : EXPLORER_base_url_mainnet);
}

std::string NTP1Tools::GetURL_ExplorerTokenInfo(const std::string& tokenId, bool testnet)
{
    return GetURL_ExplorerBase(testnet) + EXPLORER_tokenInfo + tokenId;
}

std::string NTP1Tools::GetURL_ExplorerTransactionInfo(const std::string& txId, bool testnet)
{
    return GetURL_ExplorerBase(testnet) + EXPLORER_transactionInfo + txId;
}

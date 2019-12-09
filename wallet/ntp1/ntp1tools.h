#ifndef NTP1TOOLS_H
#define NTP1TOOLS_H

#include "curltools.h"
#include "ntp1script.h"
#include "json/json_spirit.h"

class NTP1Tools
{
public:
    NTP1Tools();

    static const std::string NTPAPI_base_url_mainnet_local;
    static const std::string NTPAPI_base_url_testnet_local;

    static const std::string NTPAPI_base_url_mainnet_remote;
    static const std::string NTPAPI_base_url_testnet_remote;

    static const std::string NTPAPI_addressInfo;
    static const std::string NTPAPI_transactionInfo;
    static const std::string NTPAPI_tokenId;
    static const std::string NTPAPI_tokenMetaData;
    static const std::string NTPAPI_stakeHolders;
    static const std::string NTPAPI_sendTokens;

    static const std::string EXPLORER_base_url_mainnet;
    static const std::string EXPLORER_base_url_testnet;

    static const std::string EXPLORER_tokenInfo;
    static const std::string EXPLORER_transactionInfo;

    // json parsing methods
    static std::string GetStrField(const json_spirit::Object& data, const std::string& fieldName);
    static bool        GetBoolField(const json_spirit::Object& data, const std::string& fieldName);
    static uint64_t    GetUint64Field(const json_spirit::Object& data, const std::string& fieldName);
    static int64_t     GetInt64Field(const json_spirit::Object& data, const std::string& fieldName);
    static NTP1Int     GetNTP1IntField(const json_spirit::Object& data, const std::string& fieldName);
    static bool        GetFieldExists(const json_spirit::Object& data, const std::string& fieldName);
    static json_spirit::Array  GetArrayField(const json_spirit::Object& data,
                                             const std::string&         fieldName);
    static json_spirit::Object GetObjectField(const json_spirit::Object& data,
                                              const std::string&         fieldName);

    // local string manipulation methods
    static std::string GetURL_APIBase(bool testnet);
    static std::string GetURL_AddressInfo(const std::string& address, bool testnet);
    static std::string GetURL_TransactionInfo(const std::string& txHash, bool testnet);
    static std::string GetURL_TokenID(const std::string& tokenSymbol, bool testnet);
    static std::string GetURL_TokenMetaData(const std::string& tokenID, bool testnet);
    static std::string GetURL_TokenUTXOMetaData(const std::string& tokenID, const std::string& txHash,
                                                unsigned long outputIndex, bool testnet);
    static std::string GetURL_StakeHolders(const std::string& tokenID, bool testnet);
    static std::string GetURL_SendTokens(bool testnet);

    static std::string GetURL_ExplorerBase(bool testnet);
    static std::string GetURL_ExplorerTokenInfo(const std::string& tokenId, bool testnet);
    static std::string GetURL_ExplorerTransactionInfo(const std::string& txId, bool testnet);
};

#endif // NTP1TOOLS_H

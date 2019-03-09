#include "ntp1apicalls.h"
#include "ntp1transaction.h"

NTP1APICalls::NTP1APICalls() {}

bool NTP1APICalls::RetrieveData_AddressContainsNTP1Tokens(const std::string& address, bool testnet)
{
    try {
        std::string addressNTPInfoURL = NTP1Tools::GetURL_AddressInfo(address, testnet);
        std::string ntpData =
            cURLTools::GetFileFromHTTPS(addressNTPInfoURL, NTP1_CONNECTION_TIMEOUT, false);
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(ntpData, parsedData);
        json_spirit::Array utxosArray = NTP1Tools::GetArrayField(parsedData.get_obj(), "utxos");
        for (const auto& ob : utxosArray) {
            json_spirit::Array tokensArray = NTP1Tools::GetArrayField(ob.get_obj(), "tokens");
            if (tokensArray.size() > 0) {
                return true;
            }
        }
        return false;
    } catch (std::exception& ex) {
        printf("%s\n", ex.what());
        throw;
    }
}

uint64_t NTP1APICalls::RetrieveData_TotalNeblsExcludingNTP1(const std::string& address, bool testnet)
{
    try {
        std::string addressNTPInfoURL = NTP1Tools::GetURL_AddressInfo(address, testnet);
        std::string ntpData =
            cURLTools::GetFileFromHTTPS(addressNTPInfoURL, NTP1_CONNECTION_TIMEOUT, false);
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(ntpData, parsedData);
        json_spirit::Array utxosArray = NTP1Tools::GetArrayField(parsedData.get_obj(), "utxos");
        uint64_t           totalSats  = 0;
        for (const auto& ob : utxosArray) {
            json_spirit::Array tokensArray = NTP1Tools::GetArrayField(ob.get_obj(), "tokens");
            if (tokensArray.size() == 0) {
                totalSats += NTP1Tools::GetUint64Field(ob.get_obj(), "value");
            }
        }
        return totalSats;
    } catch (std::exception& ex) {
        printf("%s\n", ex.what());
        throw;
    }
}

NTP1TokenMetaData NTP1APICalls::RetrieveData_NTP1TokensMetaData(const std::string& tokenId,
                                                                const std::string& tx, int outputIndex,
                                                                bool testnet)
{
    try {
        std::string ntp1MetaDataURL =
            NTP1Tools::GetURL_TokenUTXOMetaData(tokenId, tx, outputIndex, testnet);
        std::string ntpData =
            cURLTools::GetFileFromHTTPS(ntp1MetaDataURL, NTP1_CONNECTION_TIMEOUT, false);
        NTP1TokenMetaData metadata;
        metadata.importRestfulAPIJsonData(ntpData);
        return metadata;
    } catch (std::exception& ex) {
        printf("%s\n", ex.what());
        throw;
    }
}

NTP1Transaction NTP1APICalls::RetrieveData_TransactionInfo(const std::string& txHash, bool testnet)
{
    std::string     url     = NTP1Tools::GetURL_TransactionInfo(txHash, testnet);
    std::string     ntpData = cURLTools::GetFileFromHTTPS(url, NTP1_CONNECTION_TIMEOUT, false);
    NTP1Transaction tx;
    tx.importJsonData(ntpData);
    return tx;
}

std::string NTP1APICalls::RetrieveData_TransactionInfo_Str(const std::string& txHash, bool testnet)
{
    std::string url     = NTP1Tools::GetURL_TransactionInfo(txHash, testnet);
    std::string ntpData = cURLTools::GetFileFromHTTPS(url, NTP1_CONNECTION_TIMEOUT, false);
    return ntpData;
}

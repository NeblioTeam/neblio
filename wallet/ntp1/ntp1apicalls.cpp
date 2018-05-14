#include "ntp1apicalls.h"
#include "ntp1transaction.h"

NTP1APICalls::NTP1APICalls()
{

}

bool NTP1APICalls::RetrieveData_AddressContainsNTP1Tokens(const std::string& address, bool testnet)
{
    try {
        std::string addressNTPInfoURL = NTP1Tools::GetURL_AddressInfo(address, testnet);
        std::string ntpData = cURLTools::GetFileFromHTTPS(addressNTPInfoURL, false);
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(ntpData, parsedData);
        json_spirit::Array utxosArray = NTP1Tools::GetArrayField(parsedData.get_obj(), "utxos");
        for(long i = 0; i < static_cast<long>(utxosArray.size()); i++) {
            json_spirit::Array tokensArray = NTP1Tools::GetArrayField(utxosArray[i].get_obj(), "tokens");
            if(tokensArray.size() > 0) {
                return true;
            }
        }
        return false;
    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

NTP1TokenMetaData NTP1APICalls::RetrieveData_NTP1TokensMetaData(const std::string &tokenId,
                                                                const std::string &tx,
                                                                int outputIndex,
                                                                bool testnet)
{
    try {
        std::string ntp1MetaDataURL = NTP1Tools::GetURL_TokenUTXOMetaData(tokenId,
                                                                          tx,
                                                                          outputIndex,
                                                                          testnet);
        std::string ntpData = cURLTools::GetFileFromHTTPS(ntp1MetaDataURL, false);
        NTP1TokenMetaData metadata;
        metadata.importRestfulAPIJsonData(ntpData);
        return metadata;
    } catch(std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

NTP1Transaction NTP1APICalls::RetrieveData_TransactionInfo(const std::string &txHash, bool testnet)
{
    std::string url = NTP1Tools::GetURL_TransactionInfo(txHash, testnet);
    std::string ntpData = cURLTools::GetFileFromHTTPS(url, false);
    NTP1Transaction tx;
    tx.importJsonData(ntpData);
    return tx;
}

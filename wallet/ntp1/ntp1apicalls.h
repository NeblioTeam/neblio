#ifndef NTP1APICALLS_H
#define NTP1APICALLS_H

#include "ntp1tokenmetadata.h"
#include "ntp1tools.h"
#include "ntp1transaction.h"

class NTP1APICalls
{
public:
    NTP1APICalls();
    static bool     RetrieveData_AddressContainsNTP1Tokens(const std::string& address, bool testnet);
    static uint64_t RetrieveData_TotalNeblsExcludingNTP1(const std::string& address, bool testnet);
    static NTP1TokenMetaData RetrieveData_NTP1TokensMetaData(const std::string& tokenId,
                                                             const std::string& tx, int outputIndex,
                                                             bool testnet);
    static const long        NTP1_CONNECTION_TIMEOUT = 10;
    static NTP1Transaction   RetrieveData_TransactionInfo(const std::string& txHash, bool testnet);
    static std::string       RetrieveData_TransactionInfo_Str(const std::string& txHash, bool testnet);
};

#endif // NTP1APICALLS_H

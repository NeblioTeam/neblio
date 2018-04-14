#ifndef NTP1TXIN_H
#define NTP1TXIN_H

#include "ntp1outpoint.h"
#include "ntp1tools.h"
#include "ntp1tokentxdata.h"

#include <string>

/**
 * @brief The NTP1TxIn class
 * A single vin entry in a transaction
 */
class NTP1TxIn
{
    NTP1OutPoint prevout;
    std::string scriptSigHex;
    uint64_t nSequence;
    std::vector<NTP1TokenTxData> tokens;

public:
    NTP1TxIn();
    void importJsonData(const std::string& data);
    void importJsonData(const json_spirit::Value& parsedData);
    NTP1OutPoint getOutPoint() const;
    std::string getScriptSigHex() const;
    uint64_t getSequence() const;
    const NTP1TokenTxData& getToken(unsigned long index) const;
    unsigned long getNumOfTokens() const;
};

#endif // NTP1TXIN_H

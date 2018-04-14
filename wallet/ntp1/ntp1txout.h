#ifndef NTP1TXOUT_H
#define NTP1TXOUT_H

#include <inttypes.h>
#include <string>

#include "ntp1tools.h"
#include "ntp1tokentxdata.h"

/**
 * @brief The NTP1TxOut class
 * A single vout entry in a transaction
 */
class NTP1TxOut
{
    int64_t nValue;
    std::string scriptPubKeyHex;
    std::vector<NTP1TokenTxData> tokens;

public:
    NTP1TxOut();
    NTP1TxOut(int64_t nValueIn, const std::string& scriptPubKeyIn);
    void setNull();
    bool isNull() const;
    void importJsonData(const std::string& data);
    void importJsonData(const json_spirit::Value& parsedData);
    int64_t getValue() const;
    const std::string& getScriptPubKeyHex() const;
    const NTP1TokenTxData &getToken(unsigned long index) const;
    unsigned long getNumOfTokens() const;
};

#endif // NTP1TXOUT_H

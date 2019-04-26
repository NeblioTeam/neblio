#ifndef NTP1TXIN_H
#define NTP1TXIN_H

#include "ntp1outpoint.h"
#include "ntp1tokentxdata.h"
#include "ntp1tools.h"

#include <string>

/**
 * @brief The NTP1TxIn class
 * A single vin entry in a transaction
 */
class NTP1TxIn
{
    NTP1OutPoint                 prevout;
    std::string                  scriptSigHex;
    uint64_t                     nSequence;
    std::vector<NTP1TokenTxData> tokens;

    friend class NTP1Transaction;

public:
    NTP1TxIn();
    void                   setNull();
    void                   importJsonData(const std::string& data);
    void                   importJsonData(const json_spirit::Value& parsedData);
    json_spirit::Value     exportDatabaseJsonData() const;
    void                   importDatabaseJsonData(const json_spirit::Value& data);
    NTP1OutPoint           getOutPoint() const;
    NTP1OutPoint           getPrevout() const;
    void                   setPrevout(const NTP1OutPoint& value);
    std::string            getScriptSigHex() const;
    void                   setScriptSigHex(const std::string& s);
    uint64_t               getSequence() const;
    const NTP1TokenTxData& getToken(unsigned long index) const;
    unsigned long          getNumOfTokens() const;
    void                   __addToken(const NTP1TokenTxData& token);
    void                   setSequence(const uint64_t& value);
    friend inline bool     operator==(const NTP1TxIn& lhs, const NTP1TxIn& rhs);

    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(prevout);
                        READWRITE(scriptSigHex);
                        READWRITE(nSequence);
                        READWRITE(tokens);
                       )
    // clang-format on
};

bool operator==(const NTP1TxIn& lhs, const NTP1TxIn& rhs)
{
    return (lhs.prevout == rhs.prevout && lhs.scriptSigHex == rhs.scriptSigHex &&
            lhs.nSequence == rhs.nSequence && lhs.tokens == rhs.tokens);
}

#endif // NTP1TXIN_H

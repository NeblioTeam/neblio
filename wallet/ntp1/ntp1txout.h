#ifndef NTP1TXOUT_H
#define NTP1TXOUT_H

#include <inttypes.h>
#include <string>

#include "ntp1tokentxdata.h"

/**
 * @brief The NTP1TxOut class
 * A single vout entry in a transaction
 */
class NTP1TxOut
{
public:
    enum OutputType
    {
        NormalOutput = 0,
        OPReturn
    };
    OutputType type;

private:
    int64_t                      nValue;
    std::string                  scriptPubKeyHex;
    std::string                  scriptPubKeyAsm;
    std::vector<NTP1TokenTxData> tokens;
    std::string                  address;

public:
    NTP1TxOut();
    NTP1TxOut(int64_t nValueIn, const std::string& scriptPubKeyIn);
    void                   setNull();
    bool                   isNull() const;
    void                   importJsonData(const std::string& data);
    void                   importJsonData(const json_spirit::Value& parsedData);
    json_spirit::Value     exportDatabaseJsonData() const;
    void                   importDatabaseJsonData(const json_spirit::Value& data);
    int64_t                getValue() const;
    const std::string&     getScriptPubKeyHex() const;
    const NTP1TokenTxData& getToken(unsigned long index) const;
    unsigned long          getNumOfTokens() const;
    friend inline bool     operator==(const NTP1TxOut& lhs, const NTP1TxOut& rhs);
    std::string            getAddress() const;
    OutputType             getType() const;
    std::string            getScriptPubKeyAsm() const;
};

bool operator==(const NTP1TxOut& lhs, const NTP1TxOut& rhs)
{
    return (lhs.nValue == rhs.nValue && lhs.scriptPubKeyHex == rhs.scriptPubKeyHex &&
            lhs.tokens == rhs.tokens);
}

#endif // NTP1TXOUT_H

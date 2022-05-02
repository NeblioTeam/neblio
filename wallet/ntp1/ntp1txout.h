#ifndef NTP1TXOUT_H
#define NTP1TXOUT_H

#include <inttypes.h>
#include <string>

#include "ntp1tokentxdata.h"
#include "script.h"

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
        OPReturn,
        NonStandard
    };

private:
    int64_t                      nValue;
    CScript                      scriptPubKey;
    std::vector<NTP1TokenTxData> tokens;
    std::string                  address;

    friend class NTP1Transaction;

public:
    NTP1TxOut();
    NTP1TxOut(int64_t nValueIn, const CScript& scriptPubKeyIn);
    void                   setNull();
    bool                   isNull() const;
    void                   importJsonData(const std::string& data);
    void                   importJsonData(const json_spirit::Value& parsedData);
    json_spirit::Value     exportDatabaseJsonData() const;
    void                   importDatabaseJsonData(const json_spirit::Value& data);
    int64_t                getValue() const;
    std::string            getScriptPubKeyHex() const;
    const NTP1TokenTxData& getToken(unsigned long index) const;
    NTP1TokenTxData&       getToken(unsigned long index);
    unsigned long          tokenCount() const;
    friend inline bool     operator==(const NTP1TxOut& lhs, const NTP1TxOut& rhs);
    std::string            getAddress() const;
    void                   setAddress(const std::string& Address);
    OutputType             getType() const;
    std::string            getScriptPubKeyAsm() const;
    void                   setNValue(const int64_t& value);
    void                   setScriptPubKey(const CScript& value);
    void                   __addToken(const NTP1TokenTxData& token);

    void __manualSet(int64_t NValue, const CScript& ScriptPubKey,
                     const std::vector<NTP1TokenTxData>& Tokens, const std::string& Address);
    void __manualSet(int64_t NValue, const std::string& ScriptPubKeyHex,
                     const std::vector<NTP1TokenTxData>& Tokens, const std::string& Address);

    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(nValue);
                        READWRITE(scriptPubKey);
                        READWRITE(tokens);
                        READWRITE(address);
                       )
    // clang-format on
};

bool operator==(const NTP1TxOut& lhs, const NTP1TxOut& rhs)
{
    return (lhs.nValue == rhs.nValue && lhs.scriptPubKey == rhs.scriptPubKey &&
            lhs.tokens == rhs.tokens && lhs.address == rhs.address);
}

#endif // NTP1TXOUT_H

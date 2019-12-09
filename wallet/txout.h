#ifndef TXOUT_H
#define TXOUT_H

#include "globals.h"
#include "script.h"
#include "serialize.h"

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    int64_t nValue;
    CScript scriptPubKey;

    CTxOut() { SetNull(); }

    CTxOut(int64_t nValueIn, CScript scriptPubKeyIn)
    {
        nValue       = nValueIn;
        scriptPubKey = scriptPubKeyIn;
    }

    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(nValue);
                        READWRITE(scriptPubKey);
                       )
    // clang-format on

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull() const { return (nValue == -1); }

    void SetEmpty()
    {
        nValue = 0;
        scriptPubKey.clear();
    }

    bool IsEmpty() const { return (nValue == 0 && scriptPubKey.empty()); }

    uint256 GetHash() const { return SerializeHash(*this); }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue == b.nValue && a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b) { return !(a == b); }

    std::string ToStringShort() const
    {
        return strprintf(" out %s %s", FormatMoney(nValue).c_str(), scriptPubKey.ToString(true).c_str());
    }

    std::string ToString() const
    {
        if (IsEmpty())
            return "CTxOut(empty)";
        return strprintf("CTxOut(nValue=%s, scriptPubKey=%s)", FormatMoney(nValue).c_str(),
                         scriptPubKey.ToString().c_str());
    }

    void print() const { printf("%s\n", ToString().c_str()); }
};

#endif // TXOUT_H

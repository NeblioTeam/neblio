#ifndef TXIN_H
#define TXIN_H

#include     "globals.h"
#include <string>
#include "outpoint.h"
#include "script.h"

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint    prevout;
    CScript      scriptSig;
    unsigned int nSequence;

    CTxIn() { nSequence = std::numeric_limits<unsigned int>::max(); }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn = CScript(),
                   unsigned int nSequenceIn = std::numeric_limits<unsigned int>::max())
    {
        prevout   = prevoutIn;
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn = CScript(),
          unsigned int nSequenceIn = std::numeric_limits<unsigned int>::max());

    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(prevout);
                        READWRITE(scriptSig);
                        READWRITE(nSequence);
                       )
    // clang-format on

    bool IsFinal() const;

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout == b.prevout && a.scriptSig == b.scriptSig && a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b) { return !(a == b); }

    std::string ToStringShort() const;

    std::string ToString() const;

    void print() const { printf("%s\n", ToString().c_str()); }
};

#endif // TXIN_H

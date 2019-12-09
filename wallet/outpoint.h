#ifndef OUTPOINT_H
#define OUTPOINT_H

#include "uint256.h"
#include "serialize.h"

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256      hash;
    unsigned int n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, unsigned int nIn);
    IMPLEMENT_SERIALIZE(READWRITE(FLATDATA(*this));)
    void SetNull();
    bool IsNull() const { return (hash == 0 && n == (unsigned int)-1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b) { return !(a == b); }

    std::string ToString() const;

    void print() const { printf("%s\n", ToString().c_str()); }
};

#endif // OUTPOINT_H

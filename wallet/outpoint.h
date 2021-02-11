#ifndef OUTPOINT_H
#define OUTPOINT_H

#include "serialize.h"
#include "uint256.h"

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256  hash;
    uint32_t n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, uint32_t nIn);
    IMPLEMENT_SERIALIZE(READWRITE(FLATDATA(*this));)
    void SetNull()
    {
        hash = 0;
        n    = UINT32_C(-1);
    }

    bool IsNull() const { return (hash == 0 && n == UINT32_C(-1)); }

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

    void print() const;
};

#endif // OUTPOINT_H

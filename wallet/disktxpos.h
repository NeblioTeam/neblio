#ifndef DISKTXPOS_H
#define DISKTXPOS_H

#include "uint256.h"
#include "serialize.h"

/** Position on disk for a particular transaction. */
class CDiskTxPos
{
public:
    uint256      nBlockPos;
    unsigned int nTxPos;

    CDiskTxPos() { SetNull(); }

    CDiskTxPos(const uint256& nBlockPosIn, unsigned int nTxPosIn)
    {
        nBlockPos = nBlockPosIn;
        nTxPos    = nTxPosIn;
    }

    IMPLEMENT_SERIALIZE(READWRITE(FLATDATA(*this));)

    void SetNull()
    {
        nBlockPos = 0;
        nTxPos    = -1;
    }
    bool IsNull() const { return (nTxPos == (unsigned int)-1); }

    friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return (a.nBlockPos == b.nBlockPos && a.nTxPos == b.nTxPos);
    }

    friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b) { return !(a == b); }

    std::string ToString() const;

    void print() const;
};

#endif // DISKTXPOS_H

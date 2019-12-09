#ifndef TXINDEX_H
#define TXINDEX_H

#include "disktxpos.h"
#include <vector>

/**  A txdb record that contains the disk location of a transaction and the
 * locations of transactions that spend its outputs.  vSpent is really only
 * used as a flag, but having the location is very helpful for debugging.
 */
class CTxIndex
{
public:
    CDiskTxPos              pos;
    std::vector<CDiskTxPos> vSpent;

    CTxIndex();

    CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs);

    IMPLEMENT_SERIALIZE(if (!(nType & SER_GETHASH)) READWRITE(nVersion); READWRITE(pos);
                        READWRITE(vSpent);)

    void SetNull();

    bool IsNull();

    friend bool operator==(const CTxIndex& a, const CTxIndex& b)
    {
        return (a.pos == b.pos && a.vSpent == b.vSpent);
    }

    friend bool operator!=(const CTxIndex& a, const CTxIndex& b) { return !(a == b); }
    int         GetDepthInMainChain() const;
};

#endif // TXINDEX_H

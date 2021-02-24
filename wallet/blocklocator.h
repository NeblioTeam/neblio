#ifndef BLOCKLOCATOR_H
#define BLOCKLOCATOR_H

#include "boost/foreach.hpp"
#include "globals.h"
#include "serialize.h"
#include "uint256.h"
#include <vector>

class CBlockIndex;

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
class CBlockLocator
{
protected:
    std::vector<uint256> vHave;

public:
    CBlockLocator() {}

    explicit CBlockLocator(const CBlockIndex* pindex, const ITxDB& txdb);

    CBlockLocator(const std::vector<uint256>& vHaveIn);

    // clang-format off
    IMPLEMENT_SERIALIZE(
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    )
    // clang-format on

    void SetNull();

    bool IsNull();

    void Set(const CBlockIndex* pindex, const ITxDB& txdb);

    int GetDistanceBack(const ITxDB& txdb);

    boost::optional<CBlockIndex> GetBlockIndex(const ITxDB& txdb);

    uint256 GetBlockHash(const ITxDB& txdb);

    int GetHeight(const ITxDB& txdb);
};

#endif // BLOCKLOCATOR_H

#include "diskblockindex.h"

#include "block.h"
#include "globals.h"
#include "util.h"

uint256 CDiskBlockIndex::GetBlockHash() const
{
    if (fUseFastIndex && (nTime < GetAdjustedTime() - 24 * 60 * 60) && blockHash != 0)
        return blockHash;

    CBlock block;
    block.nVersion       = nVersion;
    block.hashPrevBlock  = hashPrev;
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime          = nTime;
    block.nBits          = nBits;
    block.nNonce         = nNonce;

    const_cast<CDiskBlockIndex*>(this)->blockHash = block.GetHash();

    return blockHash;
}

std::string CDiskBlockIndex::ToString() const
{
    std::string str = "CDiskBlockIndex(";
    str += CBlockIndex::ToString();
    str += strprintf("\n                hashBlock=%s, hashPrev=%s, hashNext=%s)",
                     GetBlockHash().ToString().c_str(), hashPrev.ToString().c_str(),
                     hashNext.ToString().c_str());
    return str;
}

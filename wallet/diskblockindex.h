#ifndef DISKBLOCKINDEX_H
#define DISKBLOCKINDEX_H

#include "blockindex.h"
#include "uint256.h"

/** Used to marshal pointers into hashes for db storage. */
class CDiskBlockIndex : public CBlockIndex
{
private:
    uint256 blockHash;

public:
    uint256 hashPrev;
    uint256 hashNext;

    CDiskBlockIndex()
    {
        hashPrev  = 0;
        hashNext  = 0;
        blockHash = 0;
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : 0);
        hashNext = (pnext ? pnext->GetBlockHash() : 0);
    }

    IMPLEMENT_SERIALIZE(
        if (!(nType & SER_GETHASH)) READWRITE(nVersion);

        READWRITE(hashNext); READWRITE(blockKeyInDB); READWRITE(nHeight); READWRITE(nMint);
        READWRITE(nMoneySupply); READWRITE(nFlags); READWRITE(nStakeModifier); if (IsProofOfStake()) {
            READWRITE(prevoutStake);
            READWRITE(nStakeTime);
        } else if (fRead) {
            const_cast<CDiskBlockIndex*>(this)->prevoutStake.SetNull();
            const_cast<CDiskBlockIndex*>(this)->nStakeTime = 0;
        } READWRITE(hashProof);

        // block header
        READWRITE(this->nVersion); READWRITE(hashPrev); READWRITE(hashMerkleRoot); READWRITE(nTime);
        READWRITE(nBits); READWRITE(nNonce); READWRITE(blockHash);)

    void SetBlockHash(const uint256& hash) { blockHash = hash; }

    uint256 GetBlockHash() const;

    std::string ToString() const;

    void print() const { printf("%s\n", ToString().c_str()); }
};

#endif // DISKBLOCKINDEX_H

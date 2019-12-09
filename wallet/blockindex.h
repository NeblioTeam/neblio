#ifndef BLOCKINDEX_H
#define BLOCKINDEX_H

#include "globals.h"
#include "uint256.h"
#include "outpoint.h"

class CBlock;

/** The block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block.  pprev and pnext link a path through the
 * main/longest chain.  A blockindex may have multiple pprev pointing back
 * to it, but pnext will only point forward to the longest branch, or will
 * be null if the block is not part of the longest chain.
 */
class CBlockIndex
{
public:
    const uint256*      phashBlock;
    CBlockIndexSmartPtr pprev;
    CBlockIndexSmartPtr pnext;
    uint256             blockKeyInDB;
    uint256             nChainTrust; // ppcoin: trust score of block chain
    int                 nHeight;

    int64_t nMint;
    int64_t nMoneySupply;

    unsigned int nFlags; // ppcoin: block index flags
    enum
    {
        BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
        BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
        BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
    };

    uint64_t     nStakeModifier;         // hash modifier for proof-of-stake
    unsigned int nStakeModifierChecksum; // checksum of index; in-memeory only

    // proof-of-stake specific fields
    COutPoint    prevoutStake;
    unsigned int nStakeTime;

    uint256 hashProof;

    // block header
    int          nVersion;
    uint256      hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;

    CBlockIndex();

    CBlockIndex(uint256 nBlockPosIn, CBlock& block);

    CBlock GetBlockHeader() const;

    uint256 GetBlockHash() const { return *phashBlock; }

    int64_t GetBlockTime() const { return (int64_t)nTime; }

    uint256 GetBlockTrust() const;

    bool IsInMainChain() const;

    bool CheckIndex() const { return true; }

    int64_t GetPastTimeLimit() const { return GetMedianTimePast(); }

    enum
    {
        nMedianTimeSpan = 11
    };

    int64_t GetMedianTimePast() const
    {
        int64_t  pmedian[nMedianTimeSpan];
        int64_t* pbegin = &pmedian[nMedianTimeSpan];
        int64_t* pend   = &pmedian[nMedianTimeSpan];

        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex;
             i++, pindex = boost::atomic_load(&pindex->pprev).get())
            *(--pbegin) = pindex->GetBlockTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin) / 2];
    }

    /**
     * Returns true if there are nRequired or more blocks of minVersion or above
     * in the last nToCheck blocks, starting at pstart and going backwards.
     */
    static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired,
                                unsigned int nToCheck);

    bool IsProofOfWork() const { return !(nFlags & BLOCK_PROOF_OF_STAKE); }

    bool IsProofOfStake() const { return (nFlags & BLOCK_PROOF_OF_STAKE); }

    void SetProofOfStake() { nFlags |= BLOCK_PROOF_OF_STAKE; }

    unsigned int GetStakeEntropyBit() const { return ((nFlags & BLOCK_STAKE_ENTROPY) >> 1); }

    bool SetStakeEntropyBit(unsigned int nEntropyBit)
    {
        if (nEntropyBit > 1)
            return false;
        nFlags |= (nEntropyBit ? BLOCK_STAKE_ENTROPY : 0);
        return true;
    }

    bool GeneratedStakeModifier() const { return (nFlags & BLOCK_STAKE_MODIFIER); }

    void SetStakeModifier(uint64_t nModifier, bool fGeneratedStakeModifier)
    {
        nStakeModifier = nModifier;
        if (fGeneratedStakeModifier)
            nFlags |= BLOCK_STAKE_MODIFIER;
    }

    std::string ToString() const;

    void print() const { printf("%s\n", ToString().c_str()); }
};

#endif // BLOCKINDEX_H

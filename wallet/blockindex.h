#ifndef BLOCKINDEX_H
#define BLOCKINDEX_H

#include "globals.h"
#include "logging/logger.h"
#include "outpoint.h"
#include "uint256.h"

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
    uint256             phashBlock;
    CBlockIndexSmartPtr pprev;
    CBlockIndexSmartPtr pnext;
    uint256             nChainTrust; // ppcoin: trust score of block chain
    int32_t             nHeight;

    uint32_t nFlags; // ppcoin: block index flags
    enum
    {
        BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
        BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
        BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
    };

    uint64_t nStakeModifier;         // hash modifier for proof-of-stake
    uint32_t nStakeModifierChecksum; // checksum of index; in-memeory only

    // proof-of-stake specific fields
    COutPoint prevoutStake;
    uint32_t  nStakeTime;

    uint256 hashProof;

    // block header
    int32_t  nVersion;
    uint256  hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CBlockIndex();

    CBlockIndex(uint256 blockHashIn, const CBlock& block);

    CBlock GetBlockHeader() const;

    uint256 GetBlockHash() const { return phashBlock; }

    int64_t GetBlockTime() const { return (int64_t)nTime; }

    uint256 GetBlockTrust() const;

    bool IsInMainChain(const ITxDB& txdb) const;

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

    void print() const { NLog.write(b_sev::info, "{}", ToString()); }
};

#endif // BLOCKINDEX_H

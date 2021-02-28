#ifndef BLOCKINDEX_H
#define BLOCKINDEX_H

#include "itxdb.h"
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
    uint256 blockHash;
    uint256 hashPrev;
    uint256 hashNext;

    uint256 nChainTrust; // ppcoin: trust score of block chain
    int32_t nHeight;

    uint32_t nFlags; // ppcoin: block index flags
    enum
    {
        BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
        BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
        BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
    };

    uint64_t nStakeModifier;         // hash modifier for proof-of-stake
    uint32_t nStakeModifierChecksum; // checksum of index

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

    uint256 GetBlockHash() const;

    int64_t GetBlockTime() const;

    uint256 GetBlockTrust() const;

    bool IsInMainChain(const ITxDB& txdb) const;

    bool CheckIndex() const { return true; }

    int64_t GetPastTimeLimit(const ITxDB& txdb) const;

    enum
    {
        nMedianTimeSpan = 11
    };

    int64_t GetMedianTimePast(const ITxDB& txdb) const;

    bool IsProofOfWork() const;

    bool IsProofOfStake() const;

    void SetProofOfStake();

    unsigned int GetStakeEntropyBit() const;

    bool SetStakeEntropyBit(unsigned int nEntropyBit);

    bool GeneratedStakeModifier() const;

    void SetStakeModifier(uint64_t nModifier, bool fGeneratedStakeModifier);

    std::string ToString() const;

    void print() const;

    boost::optional<CBlockIndex> getPrev(const ITxDB& txdb) const;
    boost::optional<CBlockIndex> getNext(const ITxDB& txdb) const;

    // clang-format off
        IMPLEMENT_SERIALIZE(
            if (!(nType & SER_GETHASH))
                READWRITE(nVersion);

            READWRITE(hashNext);
            READWRITE(nHeight);
            READWRITE(nFlags);
            READWRITE(nStakeModifier);
            READWRITE(nStakeModifierChecksum);

            if (IsProofOfStake()) {
                READWRITE(prevoutStake);
                READWRITE(nStakeTime);
            } else if (fRead) {
                const_cast<CBlockIndex*>(this)->prevoutStake.SetNull();
                const_cast<CBlockIndex*>(this)->nStakeTime = 0;
            }

            READWRITE(hashProof);

            // block header
            READWRITE(this->nVersion);
            READWRITE(hashPrev);
            READWRITE(hashMerkleRoot);
            READWRITE(nTime);
            READWRITE(nBits);
            READWRITE(nNonce);
            READWRITE(blockHash);
            READWRITE(nChainTrust);
        )
    // clang-format on
};

#endif // BLOCKINDEX_H

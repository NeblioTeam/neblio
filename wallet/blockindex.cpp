#include "blockindex.h"

#include "bignum.h"
#include "block.h"
#include "blockindexlrucache.h"
#include "boost/shared_ptr.hpp"
#include "util.h"

CBlockIndex::CBlockIndex()
{
    blockHash              = 0;
    hashPrev               = 0;
    hashNext               = 0;
    nHeight                = 0;
    nChainTrust            = 0;
    nFlags                 = 0;
    nStakeModifier         = 0;
    nStakeModifierChecksum = 0;
    hashProof              = 0;
    prevoutStake.SetNull();
    nStakeTime = 0;

    nVersion       = 0;
    hashMerkleRoot = 0;
    nTime          = 0;
    nBits          = 0;
    nNonce         = 0;
}

CBlockIndex::CBlockIndex(uint256 blockHashIn, const CBlock& block)
{
    blockHash              = blockHashIn;
    hashPrev               = 0;
    hashNext               = 0;
    nHeight                = 0;
    nChainTrust            = 0;
    nFlags                 = 0;
    nStakeModifier         = 0;
    nStakeModifierChecksum = 0;
    hashProof              = 0;
    if (block.IsProofOfStake()) {
        SetProofOfStake();
        prevoutStake = block.vtx[1].vin[0].prevout;
        nStakeTime   = block.vtx[1].nTime;
    } else {
        prevoutStake.SetNull();
        nStakeTime = 0;
    }

    nVersion       = block.nVersion;
    hashMerkleRoot = block.hashMerkleRoot;
    nTime          = block.nTime;
    nBits          = block.nBits;
    nNonce         = block.nNonce;
}

CBlock CBlockIndex::GetBlockHeader() const
{
    CBlock block;
    block.nVersion       = nVersion;
    block.hashPrevBlock  = hashPrev;
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime          = nTime;
    block.nBits          = nBits;
    block.nNonce         = nNonce;
    return block;
}

uint256 CBlockIndex::GetBlockHash() const { return blockHash; }

int64_t CBlockIndex::GetBlockTime() const { return (int64_t)nTime; }

std::string CBlockIndex::ToString() const
{
    return fmt::format("CBlockIndex(hashPrev={}, hashNext={}, nHeight={}, "
                       "nFlags=({})({})({}), nStakeModifier={:x}"
                       ", nStakeModifierChecksum={:x}, hashProof={}, prevoutStake=({}), nStakeTime={} "
                       "merkle={}, hashBlock={})",
                       hashPrev.ToString(), hashNext.ToString(), nHeight,
                       GeneratedStakeModifier() ? "MOD" : "-", GetStakeEntropyBit(),
                       IsProofOfStake() ? "PoS" : "PoW", nStakeModifier, nStakeModifierChecksum,
                       hashProof.ToString(), prevoutStake.ToString(), nStakeTime,
                       hashMerkleRoot.ToString(), GetBlockHash().ToString());
}

void CBlockIndex::print() const { NLog.write(b_sev::info, "{}", ToString()); }

boost::optional<CBlockIndex> CBlockIndex::getPrev(const ITxDB& txdb) const
{
    if (hashPrev != 0) {
        return txdb.ReadBlockIndex(hashPrev);
    } else {
        return boost::none;
    }
}

boost::optional<CBlockIndex> CBlockIndex::getNext(const ITxDB& txdb) const
{
    if (hashNext != 0) {
        return txdb.ReadBlockIndex(hashNext);
    } else {
        return boost::none;
    }
}

uint256 CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    if (bnTarget <= 0)
        return 0;

    return ((CBigNum(1) << 256) / (bnTarget + 1)).getuint256();
}

bool CBlockIndex::IsInMainChain(const ITxDB& txdb) const
{
    return (hashNext != 0 || blockHash == txdb.GetBestBlockHash());
}

int64_t CBlockIndex::GetPastTimeLimit(const ITxDB& txdb) const { return GetMedianTimePast(txdb); }

int64_t CBlockIndex::GetMedianTimePast(const ITxDB& txdb) const
{
    int64_t  pmedian[nMedianTimeSpan];
    int64_t* pbegin = &pmedian[nMedianTimeSpan];
    int64_t* pend   = &pmedian[nMedianTimeSpan];

    using BlockTimeCacheType = BlockIndexLRUCache<int64_t, boost::mutex>;

    static typename BlockTimeCacheType::ExtractorFunc extractorFunc =
        [](const CBlockIndex& bi) -> int64_t { return bi.GetBlockTime(); };

    static BlockTimeCacheType blockTimeCache(500, extractorFunc);

    uint256 currHash = this->GetBlockHash();
    blockTimeCache.manualAdd(*this);
    for (int i = 0; i < nMedianTimeSpan; i++) {
        const boost::optional<BlockTimeCacheType::BICacheEntry> blockTime =
            blockTimeCache.get(txdb, currHash);
        if (!blockTime) {
            NLog.write(b_sev::err, "CRITICAL ERROR: block not found while calculating target");
            break;
        }

        *(--pbegin) = blockTime->value;

        // move to the previous block
        if (blockTime->prevHash != 0) {
            currHash = blockTime->prevHash;
        } else {
            if (currHash != Params().GenesisBlockHash()) {
                NLog.write(b_sev::critical,
                           "CRITICAL ERROR: prev block has zero hash even though it's not genesis. "
                           "THIS SHOULD NEVER HAPPEN. Database corrupt?");
            }
        }
    }

    std::sort(pbegin, pend);
    return pbegin[(pend - pbegin) / 2];
}

bool CBlockIndex::IsProofOfWork() const { return !(nFlags & BLOCK_PROOF_OF_STAKE); }

bool CBlockIndex::IsProofOfStake() const { return (nFlags & BLOCK_PROOF_OF_STAKE); }

void CBlockIndex::SetProofOfStake() { nFlags |= BLOCK_PROOF_OF_STAKE; }

unsigned int CBlockIndex::GetStakeEntropyBit() const { return ((nFlags & BLOCK_STAKE_ENTROPY) >> 1); }

bool CBlockIndex::SetStakeEntropyBit(unsigned int nEntropyBit)
{
    if (nEntropyBit > 1)
        return false;
    nFlags |= (nEntropyBit ? BLOCK_STAKE_ENTROPY : 0);
    return true;
}

bool CBlockIndex::GeneratedStakeModifier() const { return (nFlags & BLOCK_STAKE_MODIFIER); }

void CBlockIndex::SetStakeModifier(uint64_t nModifier, bool fGeneratedStakeModifier)
{
    nStakeModifier = nModifier;
    if (fGeneratedStakeModifier)
        nFlags |= BLOCK_STAKE_MODIFIER;
}

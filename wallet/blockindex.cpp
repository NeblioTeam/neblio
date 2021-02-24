#include "blockindex.h"

#include "bignum.h"
#include "block.h"
#include "boost/shared_ptr.hpp"
#include "util.h"

CBlockIndex::CBlockIndex()
{
    blockHash = 0;
    hashPrev  = 0;
    hashNext  = 0;
    //    pprev                  = NULL;
    //    pnext                  = NULL;
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
    blockHash = blockHashIn;
    hashPrev  = 0;
    hashNext  = 0;
    //    pprev                  = NULL;
    //    pnext                  = NULL;
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

int64_t CBlockIndex::GetMedianTimePast() const
{
    int64_t  pmedian[nMedianTimeSpan];
    int64_t* pbegin = &pmedian[nMedianTimeSpan];
    int64_t* pend   = &pmedian[nMedianTimeSpan];

    const CTxDB txdb;

    CBlockIndex index = *this;
    for (int i = 0; i < nMedianTimeSpan; i++) {
        *(--pbegin)                     = index.GetBlockTime();
        boost::optional<CBlockIndex> bi = index.getPrev(txdb);
        if (!bi) {
            break;
        }
        index = std::move(*bi);
    }

    std::sort(pbegin, pend);
    return pbegin[(pend - pbegin) / 2];
}

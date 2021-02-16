#include "blockindex.h"

#include "bignum.h"
#include "block.h"
#include "boost/shared_ptr.hpp"
#include "util.h"

CBlockIndex::CBlockIndex()
{
    blockHash              = 0;
    pprev                  = NULL;
    pnext                  = NULL;
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
    pprev                  = NULL;
    pnext                  = NULL;
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
    block.nVersion = nVersion;
    if (pprev)
        block.hashPrevBlock = pprev->GetBlockHash();
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime          = nTime;
    block.nBits          = nBits;
    block.nNonce         = nNonce;
    return block;
}

std::string CBlockIndex::ToString() const
{
    return fmt::format("CBlockIndex(nprev={}, pnext={}, nHeight={}, "
                       "nFlags=({})({})({}), nStakeModifier={:x}"
                       ", nStakeModifierChecksum={:x}, hashProof={}, prevoutStake=({}), nStakeTime={} "
                       "merkle={}, hashBlock={})",
                       fmt::ptr(pprev.get()), fmt::ptr(pnext.get()), nHeight,
                       GeneratedStakeModifier() ? "MOD" : "-", GetStakeEntropyBit(),
                       IsProofOfStake() ? "PoS" : "PoW", nStakeModifier, nStakeModifierChecksum,
                       hashProof.ToString(), prevoutStake.ToString(), nStakeTime,
                       hashMerkleRoot.ToString(), GetBlockHash().ToString());
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
    return (pnext || this == txdb.GetBestBlockIndex().get());
}

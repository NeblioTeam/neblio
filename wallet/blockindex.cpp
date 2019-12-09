#include "blockindex.h"

#include "bignum.h"
#include "boost/shared_ptr.hpp"
#include "util.h"
#include "block.h"

CBlockIndex::CBlockIndex()
{
    phashBlock             = NULL;
    pprev                  = NULL;
    pnext                  = NULL;
    blockKeyInDB           = 0;
    nHeight                = 0;
    nChainTrust            = 0;
    nMint                  = 0;
    nMoneySupply           = 0;
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

CBlockIndex::CBlockIndex(uint256 nBlockPosIn, CBlock& block)
{
    phashBlock             = NULL;
    pprev                  = NULL;
    pnext                  = NULL;
    blockKeyInDB           = nBlockPosIn;
    nHeight                = 0;
    nChainTrust            = 0;
    nMint                  = 0;
    nMoneySupply           = 0;
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
    return strprintf("CBlockIndex(nprev=%p, pnext=%p, nBlockPos=%s nHeight=%d, "
                     "nMint=%s, nMoneySupply=%s, nFlags=(%s)(%d)(%s), nStakeModifier=%016" PRIx64
                     ", nStakeModifierChecksum=%08x, hashProof=%s, prevoutStake=(%s), nStakeTime=%d "
                     "merkle=%s, hashBlock=%s)",
                     pprev.get(), pnext.get(), blockKeyInDB.ToString().c_str(), nHeight,
                     FormatMoney(nMint).c_str(), FormatMoney(nMoneySupply).c_str(),
                     GeneratedStakeModifier() ? "MOD" : "-", GetStakeEntropyBit(),
                     IsProofOfStake() ? "PoS" : "PoW", nStakeModifier, nStakeModifierChecksum,
                     hashProof.ToString().c_str(), prevoutStake.ToString().c_str(), nStakeTime,
                     hashMerkleRoot.ToString().c_str(), GetBlockHash().ToString().c_str());
}

uint256 CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    if (bnTarget <= 0)
        return 0;

    return ((CBigNum(1) << 256) / (bnTarget + 1)).getuint256();
}

bool CBlockIndex::IsInMainChain() const
{
    return (pnext || this == boost::atomic_load(&pindexBest).get());
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired,
                                  unsigned int nToCheck)
{
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++) {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = boost::atomic_load(&pstart->pprev).get();
    }
    return (nFound >= nRequired);
}

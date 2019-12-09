#include "blocklocator.h"

#include "blockindex.h"
#include "protocol.h"

CBlockLocator::CBlockLocator(const CBlockIndex* pindex) { Set(pindex); }

CBlockLocator::CBlockLocator(uint256 hashBlock)
{
    BlockIndexMapType::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi != mapBlockIndex.end())
        Set(boost::atomic_load(&mi->second).get());
}

CBlockLocator::CBlockLocator(const std::vector<uint256>& vHaveIn) { vHave = vHaveIn; }

void CBlockLocator::SetNull() { vHave.clear(); }

bool CBlockLocator::IsNull() { return vHave.empty(); }

void CBlockLocator::Set(const CBlockIndex* pindex)
{
    vHave.clear();
    int nStep = 1;
    while (pindex) {
        vHave.push_back(pindex->GetBlockHash());

        // Exponentially larger steps back
        for (int i = 0; pindex && i < nStep; i++)
            pindex = boost::atomic_load(&pindex->pprev).get();
        if (vHave.size() > 10)
            nStep *= 2;
    }
    vHave.push_back((!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
}

int CBlockLocator::GetDistanceBack()
{
    // Retrace how far back it was in the sender's branch
    int nDistance = 0;
    int nStep     = 1;
    BOOST_FOREACH (const uint256& hash, vHave) {
        BlockIndexMapType::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
            if (pindex->IsInMainChain())
                return nDistance;
        }
        nDistance += nStep;
        if (nDistance > 10)
            nStep *= 2;
    }
    return nDistance;
}

CBlockIndexSmartPtr CBlockLocator::GetBlockIndex()
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH (const uint256& hash, vHave) {
        BlockIndexMapType::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
            if (pindex->IsInMainChain())
                return pindex;
        }
    }
    return pindexGenesisBlock;
}

uint256 CBlockLocator::GetBlockHash()
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH (const uint256& hash, vHave) {
        BlockIndexMapType::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
            if (pindex->IsInMainChain())
                return hash;
        }
    }
    return (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet);
}

int CBlockLocator::GetHeight()
{
    CBlockIndex* pindex = GetBlockIndex().get();
    if (!pindex)
        return 0;
    return pindex->nHeight;
}

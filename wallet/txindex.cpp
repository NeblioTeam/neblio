#include "txindex.h"

#include "block.h"

CTxIndex::CTxIndex() { SetNull(); }

CTxIndex::CTxIndex(const CDiskTxPos &posIn, unsigned int nOutputs)
{
    pos = posIn;
    vSpent.resize(nOutputs);
}

void CTxIndex::SetNull()
{
    pos.SetNull();
    vSpent.clear();
}

bool CTxIndex::IsNull() { return pos.IsNull(); }

int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    BlockIndexMapType::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}


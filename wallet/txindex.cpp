#include "txindex.h"

#include "block.h"
#include "itxdb.h"

CTxIndex::CTxIndex() { SetNull(); }

CTxIndex::CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs)
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

int CTxIndex::GetDepthInMainChain(const ITxDB& txdb) const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    const auto bi = mapBlockIndex.get(block.GetHash()).value_or(nullptr);
    if (!bi)
        return 0;
    if (!bi || !bi->IsInMainChain(txdb))
        return 0;
    return 1 + txdb.GetBestChainHeight().value_or(0) - bi->nHeight;
}

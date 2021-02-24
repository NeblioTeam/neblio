#include "blocklocator.h"

#include "blockindex.h"
#include "protocol.h"
#include "txdb-lmdb.h"

CBlockLocator::CBlockLocator(const CBlockIndex* pindex, const ITxDB& txdb) { Set(pindex, txdb); }

CBlockLocator::CBlockLocator(const std::vector<uint256>& vHaveIn) { vHave = vHaveIn; }

void CBlockLocator::SetNull() { vHave.clear(); }

bool CBlockLocator::IsNull() { return vHave.empty(); }

void CBlockLocator::Set(const CBlockIndex* pindex, const ITxDB& txdb)
{
    vHave.clear();
    int                          nStep = 1;
    boost::optional<CBlockIndex> index = pindex ? boost::make_optional(*pindex) : boost::none;
    while (index) {
        vHave.push_back(pindex->GetBlockHash());

        // Exponentially larger steps back
        for (int i = 0; index && i < nStep; i++) {
            // TODO: this can be optimized by using the block height
            index = index->getPrev(txdb);
        }
        if (vHave.size() > 10) {
            nStep *= 2;
        }
    }
    vHave.push_back(Params().GenesisBlockHash());
}

int CBlockLocator::GetDistanceBack(const ITxDB& txdb)
{
    // Retrace how far back it was in the sender's branch
    int nDistance = 0;
    int nStep     = 1;
    for (const uint256& hash : vHave) {
        const boost::optional<CBlockIndex> bi = txdb.ReadBlockIndex(hash);
        if (bi) {
            if (bi->IsInMainChain(txdb))
                return nDistance;
        }
        nDistance += nStep;
        if (nDistance > 10)
            nStep *= 2;
    }
    return nDistance;
}

boost::optional<CBlockIndex> CBlockLocator::GetBlockIndex(const ITxDB& txdb)
{
    // Find the first block the caller has in the main chain
    for (const uint256& hash : vHave) {
        // TODO: this can be optimized with block height
        const auto bi = txdb.ReadBlockIndex(hash);
        if (bi) {
            if (bi->IsInMainChain(txdb))
                return bi;
        }
    }
    return boost::make_optional(*pindexGenesisBlock);
}

uint256 CBlockLocator::GetBlockHash(const ITxDB& txdb)
{
    // Find the first block the caller has in the main chain
    for (const uint256& hash : vHave) {
        const auto bi = txdb.ReadBlockIndex(hash);
        if (bi) {
            if (bi->IsInMainChain(txdb))
                return hash;
        }
    }
    return Params().GenesisBlockHash();
}

int CBlockLocator::GetHeight(const ITxDB& txdb)
{
    const boost::optional<CBlockIndex> pindex = GetBlockIndex(txdb).get();
    if (!pindex)
        return 0;
    return pindex->nHeight;
}

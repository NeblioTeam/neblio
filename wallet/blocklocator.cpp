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

    vHave.push_back(index->GetBlockHash());
    while (index) {
        if (!index->IsInMainChain(txdb)) {
            for (int i = 0; index && i < nStep; i++) {
                index = index->getPrev(txdb);
            }
            vHave.push_back(index->GetBlockHash());
        } else {
            const int currentHeight = index->nHeight;
            if (currentHeight < nStep) {
                break;
            }
            const int                      targetHeight = currentHeight - nStep;
            const boost::optional<uint256> hash         = txdb.ReadBlockHashOfHeight(targetHeight);
            if (!hash) {
                NLog.write(
                    b_sev::err,
                    "CRITICAL: Could not get block hash from height in the mainchain for height: {}",
                    targetHeight);
                break;
            }
            vHave.push_back(*hash);
            index = txdb.ReadBlockIndex(*hash);
        }

        // Exponentially larger steps back
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
        const auto bi = txdb.ReadBlockIndex(hash);
        if (bi && bi->IsInMainChain(txdb)) {
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

std::size_t CBlockLocator::size() const { return vHave.size(); }

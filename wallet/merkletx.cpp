#include "merkletx.h"

#include "block.h"
#include "blockindexlrucache.h"
#include "init.h"
#include "main.h"
#include "txdb.h"
#include "txmempool.h"
#include "wallet_interface.h"

const uint256
    CMerkleTx::ABANDON_HASH(uint256("0000000000000000000000000000000000000000000000000000000000000001"));

////////////////////////////////

thread_local std::pair<uint256, int> cachedBestHeight = std::make_pair(0, 0);

static int GetBestBlockHeight(const ITxDB& txdb, const uint256& bestBlockHash)
{
    // to minimize database calls, we cache the best height vs its block hash
    int bestHeight = 0;
    if (cachedBestHeight.first == bestBlockHash) {
        return cachedBestHeight.second;
    } else {
        const boost::optional<CBlockIndex> bestBlockIndex = txdb.ReadBlockIndex(bestBlockHash);
        if (!bestBlockIndex) {
            bestHeight = 0;
        } else {
            bestHeight       = bestBlockIndex->nHeight;
            cachedBestHeight = std::make_pair(bestBlockHash, bestHeight);
        }
    }
    return bestHeight;
}

////////////////////////////////

using BlockIndexHeightIfMainChainCacheType = BlockIndexLRUCache<boost::optional<int>>;

struct ExtractorFunctor
{
    const uint256 bestBlockHash;

    explicit ExtractorFunctor(const uint256& bestBlockHashIn) : bestBlockHash(bestBlockHashIn) {}

    boost::optional<int> operator()(const CBlockIndex& bi)
    {
        return bi.IsInMainChain(bestBlockHash) ? boost::make_optional(bi.nHeight) : boost::none;
    }
};

static thread_local BlockIndexHeightIfMainChainCacheType
    blockIndexMainChainCache(2 * pwalletMain->getWalletTxsCount(), ExtractorFunctor(0));

thread_local uint256 cachedBestHash = 0;
/**
 * @brief GetBlockHeightIfMainChain
 * The BlockIndexLRUCache class is designed to only store values that are constants of blocks. However,
 * we abuse it here and store the "IsMainChain" status. This is valid because we reset the cache
 * whenever the bestBlockHash changed
 *
 * To avoid resetting the cache often, we abuse the system further by checking whether the new
 * bestBlockhash is next block right after the previous block. If that's the case, that just means the
 * IsMainChain state hasn't chained, so we skip resetting the cache.
 */
static boost::optional<int> GetBlockHeightIfMainChain(const ITxDB& txdb, const uint256& blockHash,
                                                      const uint256& bestBlockHash)
{
    if (cachedBestHash != bestBlockHash) {
        // read the current best to update the cache
        const auto bi = txdb.ReadBlockIndex(bestBlockHash);
        if (!bi) {
            NLog.write(b_sev::critical,
                       "CRITICAL ERROR: Failed to read best block index indicated with hash: {}!",
                       bestBlockHash.ToString());
            return boost::none;
        }

        // if the next block is just built upon the last best, then the state of "mainchain" is
        // unchanged in the cache
        if (bi->hashPrev != cachedBestHash) {
            // we clear the cache
            blockIndexMainChainCache.clear();
            blockIndexMainChainCache.updateCacheSize(2 * pwalletMain->getWalletTxsCount());
        }
        // we just the new best height in the extractor
        blockIndexMainChainCache.setExtractor(ExtractorFunctor(bestBlockHash));
        cachedBestHash = bestBlockHash;
    }
    const boost::optional<BlockIndexHeightIfMainChainCacheType::BICacheEntry> entry =
        blockIndexMainChainCache.get(txdb, blockHash);
    if (!entry) {
        NLog.write(b_sev::critical, "CRITICAL ERROR: A WalletTx pointed to a non-existing block: {}!",
                   blockHash.ToString());
        return boost::none;
    }
    return entry->value;
}

////////////////////////////////

CMerkleTx::CMerkleTx(const CTransaction& txIn) : CTransaction(txIn) { Init(); }

int CMerkleTx::GetDepthInMainChain(boost::optional<CBlockIndex>& pindexRet, const ITxDB& txdb,
                                   const uint256& bestBlockHash) const
{
    if (hashBlock == 0 || hashBlock == ABANDON_HASH)
        return 0;
    int nResult = 0;

    // Find the block it claims to be in
    const auto bi = txdb.ReadBlockIndex(hashBlock);
    if (!bi || !bi->IsInMainChain(txdb)) {
        nResult   = 0;
        pindexRet = boost::none;
    } else {
        const int bestHeight = GetBestBlockHeight(txdb, bestBlockHash);
        nResult              = ((nIndex == -1) ? (-1) : 1) * (bestHeight - bi->nHeight + 1);
        pindexRet            = std::move(bi);
    }

    return nResult;
}

int CMerkleTx::GetDepthInMainChain(const ITxDB& txdb, const uint256& bestBlockHash) const
{
    if (hashBlock == 0 || hashBlock == ABANDON_HASH)
        return 0;
    int nResult = 0;

    const boost::optional<int> blockHeight = GetBlockHeightIfMainChain(txdb, hashBlock, bestBlockHash);
    if (blockHeight) {
        const int bestHeight = GetBestBlockHeight(txdb, bestBlockHash);
        nResult              = ((nIndex == -1) ? (-1) : 1) * (bestHeight - *blockHeight + 1);
    }

    return nResult;
}

bool CMerkleTx::IsInMainChain(const ITxDB& txdb, const uint256& bestBlockHash) const
{
    return GetDepthInMainChain(txdb, bestBlockHash) > 0;
}

int CMerkleTx::GetBlocksToMaturity(const ITxDB& txdb, const uint256& bestBlockHash) const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    int nCbM = Params().CoinbaseMaturity(txdb);
    return std::max(0, (nCbM + 1) - GetDepthInMainChain(txdb, bestBlockHash));
}

Result<void, TxValidationState> CMerkleTx::AcceptToMemoryPool() const
{
    return ::AcceptToMemoryPool(mempool, *this);
}

bool CMerkleTx::hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }

bool CMerkleTx::isAbandoned() const { return (hashBlock == ABANDON_HASH); }

void CMerkleTx::setAbandoned() { hashBlock = ABANDON_HASH; }

int CMerkleTx::SetMerkleBranch(const ITxDB& txdb, const CBlock* pblock)
{
    AssertLockHeld(cs_main);

    CBlock blockTmp;
    if (pblock == nullptr) {
        // Load the block this tx is in
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(GetHash(), txindex))
            return 0;
        if (!blockTmp.ReadFromDisk(txindex.pos.nBlockPos, txdb))
            return 0;
        pblock = &blockTmp;
    }

    // Update the tx's hashBlock
    hashBlock = pblock->GetHash();

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)pblock->vtx.size(); nIndex++)
        if (pblock->vtx[nIndex] == *static_cast<CTransaction*>(this))
            break;
    if (nIndex == (int)pblock->vtx.size()) {
        vMerkleBranch.clear();
        nIndex = -1;
        NLog.write(b_sev::err, "ERROR: SetMerkleBranch() : couldn't find tx in block");
        return 0;
    }

    // Fill in merkle branch
    vMerkleBranch = pblock->GetMerkleBranch(nIndex);

    // Is the tx in a block that's in the main chain
    const auto bi = txdb.ReadBlockIndex(hashBlock);
    if (!bi)
        return 0;
    const boost::optional<CBlockIndex> pindex = bi;
    if (!pindex || !pindex->IsInMainChain(txdb))
        return 0;

    return txdb.GetBestBlockIndex()->nHeight - pindex->nHeight + 1;
}

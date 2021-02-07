#include "merkletx.h"

#include "block.h"
#include "main.h"
#include "txdb.h"
#include "txmempool.h"

const uint256
    CMerkleTx::ABANDON_HASH(uint256("0000000000000000000000000000000000000000000000000000000000000001"));

CMerkleTx::CMerkleTx(const CTransaction& txIn) : CTransaction(txIn) { Init(); }

int CMerkleTx::GetDepthInMainChain(const CBlockIndex*& pindexRet) const
{
    if (hashBlock == 0 || hashBlock == ABANDON_HASH)
        return 0;
    AssertLockHeld(cs_main);
    int nResult = 0;

    // Find the block it claims to be in
    const auto bi = mapBlockIndex.get(hashBlock).value_or(nullptr);
    if (!bi) {
        nResult = 0;
    } else {
        CBlockIndex* pindex = bi.get();
        if (!pindex || !pindex->IsInMainChain(CTxDB())) {
            nResult = 0;
        } else {
            pindexRet = pindex;
            nResult   = ((nIndex == -1) ? (-1) : 1) *
                      (CTxDB().GetBestChainHeight().value_or(0) - pindex->nHeight + 1);
        }
    }

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    int nCbM = Params().CoinbaseMaturity(CTxDB());
    return std::max(0, (nCbM + 1) - GetDepthInMainChain());
}

Result<void, TxValidationState> CMerkleTx::AcceptToMemoryPool() const
{
    return ::AcceptToMemoryPool(mempool, *this);
}

bool CMerkleTx::hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }

bool CMerkleTx::isAbandoned() const { return (hashBlock == ABANDON_HASH); }

void CMerkleTx::setAbandoned() { hashBlock = ABANDON_HASH; }

int CMerkleTx::SetMerkleBranch(const CBlock* pblock)
{
    AssertLockHeld(cs_main);

    const CTxDB txdb;

    CBlock blockTmp;
    if (pblock == NULL) {
        // Load the block this tx is in
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(GetHash(), txindex))
            return 0;
        if (!blockTmp.ReadFromDisk(txindex.pos.nBlockPos))
            return 0;
        pblock = &blockTmp;
    }

    // Update the tx's hashBlock
    hashBlock = pblock->GetHash();

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)pblock->vtx.size(); nIndex++)
        if (pblock->vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)pblock->vtx.size()) {
        vMerkleBranch.clear();
        nIndex = -1;
        printf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
        return 0;
    }

    // Fill in merkle branch
    vMerkleBranch = pblock->GetMerkleBranch(nIndex);

    // Is the tx in a block that's in the main chain
    const auto bi = mapBlockIndex.get(hashBlock).value_or(nullptr);
    if (!bi)
        return 0;
    CBlockIndexSmartPtr pindex = bi;
    if (!pindex || !pindex->IsInMainChain(txdb))
        return 0;

    return txdb.GetBestBlockIndex()->nHeight - pindex->nHeight + 1;
}

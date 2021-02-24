#include "merkletx.h"

#include "block.h"
#include "main.h"
#include "txdb.h"
#include "txmempool.h"

const uint256
    CMerkleTx::ABANDON_HASH(uint256("0000000000000000000000000000000000000000000000000000000000000001"));

CMerkleTx::CMerkleTx(const CTransaction& txIn) : CTransaction(txIn) { Init(); }

int CMerkleTx::GetDepthInMainChain(boost::optional<CBlockIndex>& pindexRet, const ITxDB& txdb) const
{
    if (hashBlock == 0 || hashBlock == ABANDON_HASH)
        return 0;
    AssertLockHeld(cs_main);
    int nResult = 0;

    // Find the block it claims to be in
    const auto bi = txdb.ReadBlockIndex(hashBlock);
    if (!bi) {
        nResult = 0;
    } else {
        if (!bi || !bi->IsInMainChain(txdb)) {
            nResult = 0;
        } else {
            nResult =
                ((nIndex == -1) ? (-1) : 1) * (txdb.GetBestChainHeight().value_or(0) - bi->nHeight + 1);
            pindexRet = std::move(bi);
        }
    }

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity(const ITxDB& txdb) const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    int nCbM = Params().CoinbaseMaturity(txdb);
    return std::max(0, (nCbM + 1) - GetDepthInMainChain(txdb));
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

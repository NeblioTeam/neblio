#include "forkspendsimulator.h"

#include "block.h"
#include "txdb.h"
#include <blockindexlrucache.h>
#include <boost/scope_exit.hpp>

boost::optional<int> ForkSpendSimulator::getBlockHeight(const uint256& blockHash)
{
    using BlockIndexHeightCacheType = BlockIndexLRUCache<int>;
    static thread_local typename BlockIndexHeightCacheType::ExtractorFunc extractorFunc =
        [](const CBlockIndex& bi) -> int { return bi.nHeight; };
    static thread_local BlockIndexHeightCacheType blockIndexCache(10000, extractorFunc);

    const boost::optional<BlockIndexHeightCacheType::BICacheEntry> blockIndexHeight =
        blockIndexCache.get(txdb, blockHash);
    if (blockIndexHeight) {
        return boost::make_optional(blockIndexHeight->value);
    }
    return boost::none;
}

boost::optional<CTxIndex> ForkSpendSimulator::getTxIndex(const uint256& txHash)
{
    CTxIndex txindex;

    auto idxIt = txIndexCache.find(txHash);
    if (idxIt == txIndexCache.cend()) {
        // Get prev txindex from disk
        if (!txdb.ReadTxIndex(txHash, txindex)) {
            return boost::none;
        }
    } else {
        txindex = idxIt->second;
        txIndexCache.emplace(std::make_pair(txHash, txindex));
    }

    return boost::make_optional(std::move(txindex));
}

Result<void, ForkSpendSimulator::VIUError>
ForkSpendSimulator::spendOutputVirtually(const COutPoint& output, const uint256& spenderTx)
{
    if (spent.find(output) != spent.cend()) {
        NLog.error("Output number {} in tx {} which is an input to tx {} is attempting to "
                   "double-spend in the same block",
                   output.n, output.hash.ToString(), spenderTx.ToString());
        return Err(VIUError::DoublespendAttempt_WithinTheFork);
    }

    spent.insert(output);
    return Ok();
}

Result<void, ForkSpendSimulator::VIUError> ForkSpendSimulator::unspentOrSpentAboveCommonAncestorOrError(
    const CTxIndex& txindex, const uint256& spenderTxHash, const COutPoint& input)
{
    if (!txindex.vSpent[input.n].IsNull()) {
        // if it's spent, we get the spender and check if it's above the common ancestor
        // it's OK to be above the commonAncestor and spent, because it's in another fork

        const uint256 spenderBlockHash = txindex.vSpent[input.n].nBlockPos;

        const boost::optional<int> blockIndexHeight = getBlockHeight(spenderBlockHash);

        if (!blockIndexHeight) {
            NLog.write(b_sev::err,
                       "The input of transaction {} whose index {} and hash {} is found to be "
                       "in block {} but that block is not found in the block index. This "
                       "should never happen.",
                       spenderTxHash.ToString(), input.n, input.hash.ToString(),
                       spenderBlockHash.ToString());
            return Err(VIUError::ReadSpenderBlockIndexFailed);
        }

        if (*blockIndexHeight <= commonAncestorHeight) {
            if (!txindex.vSpent[input.n].IsNull()) {
                // double spend
                NLog.error("Output number {} in tx {} which is an input to tx {} is "
                           "attempting to "
                           "double-spend in the same block",
                           input.n, input.hash.ToString(), spenderTxHash.ToString());
                return Err(VIUError::DoublespendAttempt_SpentAlreadyBeforeTheFork);
            }
        }
    }

    return Ok();
}

ForkSpendSimulator::ForkSpendSimulator(const ITxDB& txdbIn, const uint256& commonAncestorIn,
                                       int commonAncestorHeightIn)
    : txdb(txdbIn), commonAncestor(commonAncestorIn), commonAncestorHeight(commonAncestorHeightIn)
{
}

Result<void, ForkSpendSimulator::VIUError>
ForkSpendSimulator::simulateSpendingBlock(const CBlock& blockToSpend)
{
    for (const CTransaction& spenderTx : blockToSpend.vtx) {
        const uint256 spenderTxHash = spenderTx.GetHash();

        // we store the transactions of the fork by hash to be able to verify their spending later
        if (thisForkTxs.find(spenderTxHash) != thisForkTxs.cend()) {
            return Err(VIUError::TxAppearedTwiceInFork);
        }

        // after having checked that the spending is OK for this tx, we add it to the txs of this
        // fork; we only need to store the output size... no other information is needed
        const auto StoreForkTx = [this, &spenderTxHash, &spenderTx]() {
            thisForkTxs.emplace(std::make_pair(spenderTxHash, spenderTx.vout.size()));
        };
        BOOST_SCOPE_EXIT(&StoreForkTx) { StoreForkTx(); }
        BOOST_SCOPE_EXIT_END

        // coinbase doesn't spend anything
        if (spenderTx.IsCoinBase()) {
            continue;
        }

        const std::vector<CTxIn>& vin = spenderTx.vin;
        for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
            const CTxIn& txin = vin[inIdx];

            const boost::optional<CTxIndex> txindex = getTxIndex(txin.prevout.hash);
            if (!txindex) {
                // the tx index was not found

                // the only place left (after main-chain then txindex cache) for this tx is in the
                // fork itself
                auto it = thisForkTxs.find(txin.prevout.hash);
                if (it == thisForkTxs.cend()) {
                    return Err(VIUError::TxNonExistent_OutputNotFoundInMainChainOrFork);
                }

                // ensure that the output index is valid
                const unsigned outputCount = it->second;
                if (txin.prevout.n >= outputCount) {
                    return Err(VIUError::TxInputIndexOutOfRange_InFork);
                }
            }
            // check output index compared to tx index available outputs
            else if (txin.prevout.n >= txindex->vSpent.size()) {
                // tx index found, but it shows that the output index is invalid

                NLog.write(b_sev::err, "prevout.n out of range for in transaction " +
                                           spenderTxHash.ToString() + " which has input " +
                                           txin.prevout.hash.ToString() + " and out-of-range output " +
                                           std::to_string(txin.prevout.n) + " vs available size " +
                                           std::to_string(txindex->vSpent.size()));
                return Err(VIUError::TxInputIndexOutOfRange_InMainChain);
            } else {
                // tx index found, so we check if the output is spent only if it's before or at the
                // common ancestor height

                TRYV(unspentOrSpentAboveCommonAncestorOrError(*txindex, spenderTxHash, txin.prevout));
            }

            TRYV(spendOutputVirtually(txin.prevout, spenderTxHash));
        }
    }

    // update the internal state tip block
    tipBlockHash = blockToSpend.GetHash();

    return Ok();
}

boost::optional<ForkSpendSimulatorCachedObj> ForkSpendSimulator::exportCacheObj() const
{
    if (tipBlockHash) {
        ForkSpendSimulatorCachedObj res;
        res.commonAncestor            = commonAncestor;
        res.commonAncestorHeight      = commonAncestorHeight;
        res.forkTxs                   = thisForkTxs;
        res.lastProcessedTipBlockHash = *tipBlockHash;
        res.spentOutputs              = spent;

        return boost::make_optional(std::move(res));
    } else {
        return boost::none;
    }
}

Result<ForkSpendSimulator, ForkSpendSimulator::VIUError>
ForkSpendSimulator::createFromCacheObject(const ITxDB& txdb, const ForkSpendSimulatorCachedObj& obj,
                                          const uint256& currentBestBlockHash)
{
    // the common ancestor can potentially change
    // if the common ancestor is not in the main chain anymore, we should add its transactions into
    // forkTxs to be able to see whether they're double-spent
    const boost::optional<CBlockIndex> formerCommonAncestorBI = txdb.ReadBlockIndex(obj.commonAncestor);
    if (!formerCommonAncestorBI) {
        return Err(VIUError::FormerCommonAncestorNotFound);
    }

    std::unordered_map<uint256, const unsigned> newTransactionsToAdd;

    // get all tranactions from blocks that are now in the fork and were not in the mainchain when this
    // state was cached
    boost::optional<CBlockIndex> currentCommonAncestor = formerCommonAncestorBI;
    while (!formerCommonAncestorBI->IsInMainChain(currentBestBlockHash)) {
        {
            CBlock blk;
            if (!txdb.ReadBlock(currentCommonAncestor->GetBlockHash(), blk, true)) {
                return Err(VIUError::BlockCannotBeReadFromDB);
            }
            for (const CTransaction& tx : blk.vtx) {
                newTransactionsToAdd.emplace(tx.GetHash(), tx.vout.size());
            }
        }
        currentCommonAncestor = currentCommonAncestor->getPrev(txdb);
    }

    ForkSpendSimulator res(txdb, currentCommonAncestor->GetBlockHash(), currentCommonAncestor->nHeight);
    res.spent        = obj.spentOutputs;
    res.thisForkTxs  = obj.forkTxs;
    res.tipBlockHash = obj.lastProcessedTipBlockHash;
    res.thisForkTxs.insert(std::make_move_iterator(newTransactionsToAdd.begin()),
                           std::make_move_iterator(newTransactionsToAdd.end()));

    return Ok(res);
}

const char* ForkSpendSimulator::VIUErrorToString(VIUError err)
{
    switch (err) {
    case VIUError::UnknownErrorWhileCollectingTxs:
        return "UnknownErrorWhileCollectingTxs";
    case VIUError::TxInputIndexOutOfRange_InMainChain:
        return "TxInputIndexOutOfRange_InMainChain";
    case VIUError::TxInputIndexOutOfRange_InFork:
        return "TxInputIndexOutOfRange_InFork";
    case VIUError::DoublespendAttempt_SpentAlreadyBeforeTheFork:
        return "DoublespendAttempt_SpentAlreadyBeforeTheFork";
    case VIUError::DoublespendAttempt_WithinTheFork:
        return "DoublespendAttempt_WithinTheFork";
    case VIUError::BlockCannotBeReadFromDB:
        return "BlockCannotBeReadFromDB";
    case VIUError::TxNonExistent_OutputNotFoundInMainChainOrFork:
        return "TxNonExistent_OutputNotFoundInMainChainOrFork";
    case VIUError::ReadSpenderBlockIndexFailed:
        return "ReadSpenderBlockIndexFailed";
    case VIUError::BlockIndexOfPrevBlockNotFound:
        return "BlockIndexOfPrevBlockNotFound";
    case VIUError::CommonAncestorSearchFailed:
        return "CommonAncestorSearchFailed";
    case VIUError::TxAppearedTwiceInFork:
        return "TxAppearedTwiceInFork";
    case VIUError::FormerCommonAncestorNotFound:
        return "FormerCommonAncestorNotFound";
    }
    return "Unknown";
}

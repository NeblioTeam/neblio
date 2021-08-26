#include "forkspendsimulator.h"

#include "block.h"
#include "txdb.h"
#include <blockindexlrucache.h>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/scope_exit.hpp>

boost::optional<uint256> ForkSpendSimulator::getTipBlockHash() const { return tipBlockHash; }

const uint256& ForkSpendSimulator::getCommonAncestor() const { return commonAncestor; }

int ForkSpendSimulator::getCommonAncestorHeight() const { return commonAncestorHeight; }

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
                   "double-spend",
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
                           "attempting to double-spend",
                           input.n, input.hash.ToString(), spenderTxHash.ToString());
                return Err(VIUError::DoublespendAttempt_SpentAlreadyBeforeTheFork);
            }
        }
    }

    return Ok();
}

Result<void, ForkSpendSimulator::VIUError>
ForkSpendSimulator::simulateSpendingBlock_internal(const CBlock& blockToSpend)
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

    TRYV(simulateSpendingBlock_internal(blockToSpend));

    // update the internal state tip block
    tipBlockHash = blockToSpend.GetHash();

    return Ok();
}

boost::optional<ForkSpendSimulatorCachedObj> ForkSpendSimulator::exportCacheObj() const
{
    if (tipBlockHash) {
        ForkSpendSimulatorCachedObj res;
        res.commonAncestor       = commonAncestor;
        res.commonAncestorHeight = commonAncestorHeight;

        res.forkTxs.reserve(thisForkTxs.size());
        res.forkTxs.insert(thisForkTxs.cbegin(), thisForkTxs.cend());

        res.lastProcessedTipBlockHash = *tipBlockHash;

        res.spentOutputs.reserve(spent.size());
        res.spentOutputs.insert(spent.cbegin(), spent.cend());

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
    // if the common ancestor (and potentially few blocks before it) are not in the main chain anymore (a
    // block before it is now common ancestor), we should add all the transactions starting from the new
    // fork up to the old common ancestor into forkTxs to be able to see whether they're double-spent
    const boost::optional<CBlockIndex> cachedCommonAncestorBI = txdb.ReadBlockIndex(obj.commonAncestor);
    if (!cachedCommonAncestorBI) {
        return Err(VIUError::FormerCommonAncestorNotFound);
    }

    std::vector<uint256> blockHashesBetweenPrevMainChainAndFork;

    // get all tranactions from blocks that are now in the fork and were not in the mainchain when this
    // state was cached
    boost::optional<CBlockIndex> currentCommonAncestor = cachedCommonAncestorBI;
    while (!currentCommonAncestor->IsInMainChain(currentBestBlockHash)) {
        blockHashesBetweenPrevMainChainAndFork.push_back(currentCommonAncestor->GetBlockHash());

        boost::optional<CBlockIndex> prevBI = currentCommonAncestor->getPrev(txdb);
        if (!prevBI) {
            if (currentCommonAncestor->GetBlockHash() == Params().GenesisBlockHash()) {
                NLog.write(
                    b_sev::critical,
                    "VIU caching: Could not find previous block index as genesis block was reached!!!");
            } else {
                NLog.write(b_sev::critical,
                           "VIU caching: Could not find previous block index for block {} of height {}",
                           currentCommonAncestor->GetBlockHash().ToString(),
                           currentCommonAncestor->nHeight);
            }
            return Err(VIUError::BlockIndexOfPrevBlockNotFound);
        }
        currentCommonAncestor = std::move(prevBI);
    }

    // we start from a new object, respend the new fork blocks, then merge the two
    ForkSpendSimulator result(txdb, currentCommonAncestor->GetBlockHash(),
                              currentCommonAncestor->nHeight);
    // we spend the new blocks
    for (const uint256& bh : boost::adaptors::reverse(blockHashesBetweenPrevMainChainAndFork)) {
        CBlock blk;
        if (!txdb.ReadBlock(bh, blk, true)) {
            return Err(VIUError::BlockCannotBeReadFromDB);
        }

        TRYV(result.simulateSpendingBlock_internal(blk));
    }

    // then we add the blocks that come above the last blocks
    result.spent.insert(obj.spentOutputs.begin(), obj.spentOutputs.end());
    result.thisForkTxs.insert(obj.forkTxs.cbegin(), obj.forkTxs.cend());
    result.tipBlockHash = obj.lastProcessedTipBlockHash;

    return Ok(std::move(result));
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
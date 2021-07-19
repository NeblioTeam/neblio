#ifndef FORKSPENDSIMULATOR_H
#define FORKSPENDSIMULATOR_H

#include "outpoint.h"
#include "result.h"
#include "txindex.h"
#include "uint256.h"
#include <boost/optional.hpp>
#include <set>
#include <unordered_map>

class CBlock;

struct ForkSpendSimulatorCachedObj
{
    std::unordered_map<uint256, const unsigned> forkTxs;
    std::set<COutPoint>                         spentOutputs;
    uint256                                     lastProcessedTipBlockHash;
    uint256                                     commonAncestor;
    int                                         commonAncestorHeight;
};

class ForkSpendSimulator
{
public:
    enum class VIUError
    {
        UnknownErrorWhileCollectingTxs,
        TxInputIndexOutOfRange_InMainChain,
        TxInputIndexOutOfRange_InFork,
        DoublespendAttempt_SpentAlreadyBeforeTheFork,
        DoublespendAttempt_WithinTheFork,
        BlockCannotBeReadFromDB,
        TxNonExistent_OutputNotFoundInMainChainOrFork,
        ReadSpenderBlockIndexFailed,
        BlockIndexOfPrevBlockNotFound,
        CommonAncestorSearchFailed,
        TxAppearedTwiceInFork,
        FormerCommonAncestorNotFound
    };

private:
    const ITxDB& txdb;

    std::set<COutPoint> spent;

    std::unordered_map<uint256, const CTxIndex> txIndexCache;

    std::unordered_map<uint256, const unsigned> thisForkTxs;

    // the tip that the state in this class represents
    boost::optional<uint256> tipBlockHash;

    const uint256 commonAncestor;

    const int commonAncestorHeight;

    boost::optional<int> getBlockHeight(const uint256& blockHash);

    boost::optional<CTxIndex> getTxIndex(const uint256& txHash);

    Result<void, VIUError> spendOutputVirtually(const COutPoint& output, const uint256& spenderTx);

    Result<void, VIUError> unspentOrSpentAboveCommonAncestorOrError(const CTxIndex&  txindex,
                                                                    const uint256&   spenderTxHash,
                                                                    const COutPoint& input);

    Result<void, VIUError> simulateSpendingBlock_internal(const CBlock& blockToSpend);

public:
    ForkSpendSimulator(const ITxDB& txdbIn, const uint256& commonAncestorIn, int commonAncestorHeightIn);

    /**
     * @brief Attempts to virtually spend the block with the fork starting at the common ancestor given
     * in the constructor and stores Because this function alters the caches, an invalid block will cause
     * an invalid state of the caches, and will mean that for testing a certain block, a copy of this
     * object must be made before calling the function, and only on success can be continue to be used,
     * otherwise it should be disposed of
     *
     * @return Result<void, CBlock::VIUError>
     */
    Result<void, VIUError> simulateSpendingBlock(const CBlock& blockToSpend);

    boost::optional<ForkSpendSimulatorCachedObj> exportCacheObj() const;

    static Result<ForkSpendSimulator, ForkSpendSimulator::VIUError>
    createFromCacheObject(const ITxDB& txdb, const ForkSpendSimulatorCachedObj& obj,
                          const uint256& currentBestBlockHash);

    static const char*       VIUErrorToString(VIUError err);
    boost::optional<uint256> getTipBlockHash() const;
    const uint256&           getCommonAncestor() const;
    int                      getCommonAncestorHeight() const;
};

#endif // FORKSPENDSIMULATOR_H

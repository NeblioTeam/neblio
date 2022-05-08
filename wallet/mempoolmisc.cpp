#include "mempoolmisc.h"

#include "blockindex.h"
#include "consensus.h"
#include "ntp1/ntp1transaction.h"
#include "txdb.h"
#include "txmempool.h"
#include "wallet_interface.h"
#include <memory>

Result<void, TxValidationState> AcceptToMemoryPool(CTxMemPool& pool, const CTransaction& tx,
                                                   const ITxDB* txdbPtr)
{
    AssertLockHeld(cs_main);

    /**
     * Using a pointer from the outside is important because a new instance of the database does not
     * discover the changes in the database until it's flushed. We want to have the option to use a
     * previous CTxDB instance
     */
    const ITxDB*                 txdb = txdbPtr;
    std::unique_ptr<const CTxDB> txdbUniquePtr;
    if (!txdb) {
        txdbUniquePtr = MakeUnique<const CTxDB>();
        txdb          = txdbUniquePtr.get();
    }
    assert(txdb);

    TRYV(tx.CheckTransaction(txdb->GetBestChainHeight().value_or(0)));

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase()) {
        tx.DoS(100, false);
        return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "coinbase"));
    }

    // ppcoin: coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake()) {
        tx.DoS(100, false);
        return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "coinstake"));
    }

    // Rather not work on nonstandard transactions (unless -testnet)
    std::string reason;
    if (Params().NetType() == NetworkType::Mainnet &&
        !IsStandardTx(txdb->GetBestChainHeight().value_or(0), tx, reason))
        return Err(MakeInvalidTxState(TxValidationResult::TX_NOT_STANDARD, reason, "non-standard-tx"));

    // Treat non-final transactions as invalid to prevent a specific type
    // of double-spend attack, as well as DoS attacks. (if the transaction
    // can't be mined, the attacker isn't expending resources broadcasting it)
    // Basically we don't want to propagate transactions that can't included in
    // the next block.
    //
    // However, IsFinalTx() is confusing... Without arguments, it uses
    // chainActive.Height() to evaluate nLockTime; when a block is accepted, chainActive.Height()
    // is set to the value of nHeight in the block. However, when IsFinalTx()
    // is called within CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a transaction can
    // be part of the *next* block, we need to call IsFinalTx() with one more
    // than chainActive.Height().
    //
    // Timestamps on the other hand don't get any special treatment, because we
    // can't know what timestamp the next block will have, and there aren't
    // timestamp applications where it matters.
    if (!IsFinalTx(tx, *txdb, txdb->GetBestChainHeight().value_or(0) + 1)) {
        return Err(
            MakeInvalidTxState(TxValidationResult::TX_NOT_STANDARD, "non-final", "non-standard-tx"));
    }

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return Err(MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-already-in-mempool"));

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = nullptr;
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.isSpent_unsafe(outpoint)) {
                // Disable replacement feature for now
                return Err(MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-mempool-conflict"));

                // Allow replacing with a newer version of the same transaction
                if (i != 0)
                    return Err(
                        MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-mempool-conflict"));
                ptxOld = pool.mapNextTx[outpoint].ptx;
                if (IsFinalTx(*ptxOld, *txdb))
                    return Err(
                        MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-mempool-conflict"));
                if (!tx.IsNewerThan(*ptxOld))
                    return Err(
                        MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-mempool-conflict"));
                for (unsigned int j = 0; j < tx.vin.size(); j++) {
                    COutPoint outpointP = tx.vin[j].prevout;
                    if (!pool.mapNextTx.count(outpointP) || pool.mapNextTx[outpointP].ptx != ptxOld)
                        return Err(
                            MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-mempool-conflict"));
                }
                break;
            }
            if (pool.isIssaunceTokenSymbolAlreadyInMempool_unsafe(tx)) {
                return Err(
                    MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-ntp1-mempool-conflict"));
            }
        }
    }

    {
        // do we already have it?
        if (txdb->ContainsTx(hash))
            return Err(MakeInvalidTxState(TxValidationResult::TX_CONFLICT, "txn-already-known"));

        MapPrevTx                                                                mapInputs;
        std::map<uint256, CTxIndex>                                              mapUnused;
        std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>> mapUnused2;
        bool                                                                     fInvalid = false;
        if (!tx.FetchInputs(*txdb, mapUnused, false, false, mapInputs, fInvalid)) {
            if (fInvalid) {
                return Err(MakeInvalidTxState(
                    TxValidationResult::TX_INVALID_INPUTS, "bad-txns-inputs-invalid",
                    fmt::format("AcceptToMemoryPool : FetchInputs found invalid tx {}",
                                hash.ToString().substr(0, 10))));
            }
            return Err(MakeInvalidTxState(TxValidationResult::TX_MISSING_INPUTS,
                                          "bad-txns-inputs-missingorspent"));
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (!tx.AreInputsStandard(mapInputs) && Params().NetType() == NetworkType::Mainnet) {
            return Err(
                MakeInvalidTxState(TxValidationResult::TX_NOT_STANDARD, "bad-txns-nonstandard-inputs"));
        }

        // Note: if you modify this code to accept non-standard transactions, then
        // you should add code here to check that the transaction does a
        // reasonable number of ECDSA signature verifications.

        const int64_t      nFees = tx.GetValueIn(mapInputs) - tx.GetValueOut();
        const unsigned int nSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

        // Don't accept it if it can't get into a block
        const int64_t txMinFee = tx.GetMinFee(*txdb, 1000, GMF_RELAY, nSize);
        if (nFees < txMinFee) {
            return Err(MakeInvalidTxState(TxValidationResult::TX_MEMPOOL_POLICY, "insufficient fee",
                                          fmt::format("AcceptToMemoryPool : not enough fees {}, {} < {}",
                                                      hash.ToString(), nFees, txMinFee)));
        }

        // Continuously rate-limit free transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (nFees < MIN_RELAY_TX_FEE) {
            static CCriticalSection cs;
            static double           dFreeCount;
            static int64_t          nLastTime;
            int64_t                 nNow = GetTime();

            {
                LOCK(pool.cs);
                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0 / 600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount > GetArg("-limitfreerelay", 15) * 10 * 1000 && !IsFromMe(tx))
                    return Err(MakeInvalidTxState(TxValidationResult::TX_MEMPOOL_POLICY,
                                                  "fee-rejected-by-rate-limiter"));

                if (fDebug)
                    NLog.write(b_sev::debug, "Rate limit dFreeCount: {} => {}", dFreeCount,
                               dFreeCount + nSize);
                dFreeCount += nSize;
            }
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        TRYV(tx.ConnectInputs(*txdb, mapInputs, mapUnused, CDiskTxPos(1, 1), *txdb->GetBestBlockIndex(),
                              false, false));

        if (Params().PassedFirstValidNTP1Tx(txdb) &&
            Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, *txdb)) {
            try {
                std::vector<std::pair<CTransaction, NTP1Transaction>> inputsTxs =
                    NTP1Transaction::StdFetchedInputTxsToNTP1(tx, mapInputs, *txdb, false, mapUnused2,
                                                              mapUnused);
                NTP1Transaction ntp1tx;
                ntp1tx.readNTP1DataFromTx(txdb->GetBestChainHeight().value_or(0), tx, inputsTxs);
                if (EnableEnforceUniqueTokenSymbols(*txdb)) {
                    AssertNTP1TokenNameIsNotAlreadyInMainChain(ntp1tx, *txdb);
                }
            } catch (std::exception& ex) {
                NLog.write(b_sev::err,
                           "AcceptToMemoryPool: An invalid NTP1 transaction was submitted to the memory "
                           "pool; an exception was thrown: {}",
                           ex.what());
                // TX_NTP1_ERROR
                return Err(MakeInvalidTxState(
                    TxValidationResult::TX_NTP1_ERROR, "ntp1-error",
                    fmt::format(
                        "AcceptToMemoryPool: An invalid NTP1 transaction was submitted to the memory "
                        "pool; an exception was thrown: {}",
                        ex.what())));
            } catch (...) {
                return Err(MakeInvalidTxState(TxValidationResult::TX_NTP1_ERROR, "ntp1-error-unknown"));
            }
        }
    }

    // Store transaction in memory
    {
        LOCK(pool.cs);
        if (ptxOld) {
            NLog.write(b_sev::info, "AcceptToMemoryPool : replacing tx {} with new version",
                       ptxOld->GetHash().ToString());
            pool.remove(*ptxOld);
        }
        pool.addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());

    NLog.write(b_sev::info, "AcceptToMemoryPool : accepted {} (poolsz {})",
               hash.ToString().substr(0, 10), pool.mapTx.size());

    return Ok();
}

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "alert.h"
#include "block.h"
#include "blockindexlrucache.h"
#include "checkpoints.h"
#include "db.h"
#include "disktxpos.h"
#include "init.h"
#include "kernel.h"
#include "merkletx.h"
#include "net.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1/ntp1transaction.h"
#include "outpoint.h"
#include "txdb.h"
#include "txindex.h"
#include "txmempool.h"
#include "ui_interface.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/regex.hpp>

#include "NetworkForks.h"

using namespace std;
using namespace boost;

//
// Global state
//

CCriticalSection              cs_setpwalletRegistered;
set<std::shared_ptr<CWallet>> setpwalletRegistered;

CCriticalSection cs_main;

boost::atomic<bool> fImporting{false};

CMedianFilter<int> cPeerBlockCounts(5, 0);

std::unordered_map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*>           mapOrphanBlocksByPrev;
set<pair<COutPoint, unsigned int>>   setStakeSeenOrphan;

map<uint256, CTransaction> mapOrphanTransactions;
map<uint256, set<uint256>> mapOrphanTransactionsByPrev;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "Neblio Signed Message:\n";

// Settings
CAmount nTransactionFee    = MIN_TX_FEE;
CAmount nReserveBalance    = 0;
CAmount nMinimumInputValue = 0;

using BlockTimeCacheType = BlockIndexLRUCache<int64_t>;

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

// These functions dispatch to one or all registered wallets

void RegisterWallet(std::shared_ptr<CWallet> pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.insert(pwalletIn);
    }
}

void UnregisterWallet(std::shared_ptr<CWallet> pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.erase(pwalletIn);
    }
}

// check whether the passed transaction is from us
bool static IsFromMe(const CTransaction& tx)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        if (pwallet->IsFromMe(tx))
            return true;
    return false;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx, wtx))
            return true;
    return false;
}

// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->EraseFromWallet(hash);
}

// make sure all wallets know about the given transaction, in the given block
void SyncWithWallets(const ITxDB& txdb, const CTransaction& tx, const CBlock* pblock)
{
    // update NTP1 transactions
    if (pwalletMain && pwalletMain->walletNewTxUpdateFunctor) {
        pwalletMain->walletNewTxUpdateFunctor->run(tx.GetHash(), txdb.GetBestChainHeight().value_or(0));
    }

    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->SyncTransaction(txdb, tx, pblock);
}

// notify wallets about a new best chain
void SetBestChain(const CBlockLocator& loc)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

// notify wallets about an updated transaction
void UpdatedTransaction(const uint256& hashTx)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->Inventory(hash);
}

// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->ResendWalletTransactions(CTxDB(), fForce);
}

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:

    size_t nSize = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);

    if (nSize > 5000) {
        NLog.write(b_sev::warn, "ignoring large orphan tx (size: {}, hash: {})", nSize, hash.ToString());
        return false;
    }

    mapOrphanTransactions[hash] = tx;
    for (const CTxIn& txin : tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    NLog.write(b_sev::info, "stored orphan tx {} (mapsz {})", hash.ToString().substr(0, 10),
               mapOrphanTransactions.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CTransaction& tx = mapOrphanTransactions[hash];
    for (const CTxIn& txin : tx.vin) {
        mapOrphanTransactionsByPrev[txin.prevout.hash].erase(hash);
        if (mapOrphanTransactionsByPrev[txin.prevout.hash].empty())
            mapOrphanTransactionsByPrev.erase(txin.prevout.hash);
    }
    mapOrphanTransactions.erase(hash);
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans) {
        // Evict a random orphan:
        uint256                              randomhash = GetRandHash();
        map<uint256, CTransaction>::iterator it         = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}

//////////////////////////////////////////////////////////////////////////////
//
// CTransaction and CTxIndex
//

bool IsStandardTx(const ITxDB& txdb, const CTransaction& tx, string& reason)
{
    if (tx.nVersion > CTransaction::CURRENT_VERSION) {
        reason = "version";
        return false;
    }

    // Treat non-final transactions as non-standard to prevent a specific type
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
    if (!IsFinalTx(tx, txdb, txdb.GetBestChainHeight().value_or(0) + 1)) {
        reason = "non-final";
        return false;
    }
    // nTime has different purpose from nLockTime but can be used in similar attacks
    if (tx.nTime > FutureDrift(GetAdjustedTime())) {
        reason = "time-too-new";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx.vin) {
        // Biggest 'standard' txin is a 3-signature 3-of-3 CHECKMULTISIG
        // pay-to-script-hash, which is 3 ~80-byte signatures, 3
        // ~65-byte public keys, plus a few script ops.
        if (txin.scriptSig.size() > 500) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
        if (fEnforceCanonical && !txin.scriptSig.HasCanonicalPushes()) {
            reason = "scriptsig-non-canonical-push";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype   whichType;
    for (const CTxOut& txout : tx.vout) {
        if (!::IsStandard(txdb, txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }
        if (whichType == TX_NULL_DATA) {
            nDataOut++;
        }
        if (txout.nValue == 0) {
            reason = "dust";
            return false;
        }
        if (fEnforceCanonical && !txout.scriptPubKey.HasCanonicalPushes()) {
            reason = "scriptpubkey-non-canonical-push";
            return false;
        }
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool IsFinalTx(const CTransaction& tx, const ITxDB& txdb, int nBlockHeight, int64_t nBlockTime)
{
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = txdb.GetBestChainHeight().value_or(0);
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime <
        ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (const CTxIn& txin : tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

bool IsIssuedTokenBlacklisted(std::pair<CTransaction, NTP1Transaction>& txPair)
{
    const auto& prevout0      = txPair.first.vin[0].prevout;
    std::string storedTokenId = txPair.second.getTokenIdIfIssuance(prevout0.hash.ToString(), prevout0.n);
    return Params().IsNTP1TokenBlacklisted(storedTokenId);
}

void AssertNTP1TokenNameIsNotAlreadyInMainChain(const std::string& sym, const uint256& txHash,
                                                const ITxDB& txdb)
{
    // make sure that case doesn't matter by converting to upper case
    std::vector<uint256> storedSymbolsTxHashes;
    if (txdb.ReadNTP1TxsWithTokenSymbol(sym, storedSymbolsTxHashes)) {
        for (const uint256& h : storedSymbolsTxHashes) {
            if (!IsTxInMainChain(txdb, h)) {
                continue;
            }
            auto pair = std::make_pair(CTransaction::FetchTxFromDisk(h), NTP1Transaction());
            FetchNTP1TxFromDisk(pair, txdb, false);
            std::string storedSymbol = pair.second.getTokenSymbolIfIssuance();
            // blacklisted tokens can be duplicated, since they won't be used ever again
            if (IsIssuedTokenBlacklisted(pair)) {
                continue;
            }
            // make sure that case doesn't matter by converting to upper case
            std::transform(storedSymbol.begin(), storedSymbol.end(), storedSymbol.begin(), ::toupper);
            if (AreTokenSymbolsEquivalent(sym, storedSymbol) && txHash != h) {
                throw std::runtime_error(
                    "Failed to accept issuance of token " + sym + " from transaction " +
                    txHash.ToString() +
                    "; this token symbol already exists in transaction: " + h.ToString());
            }
        }
    } else {
        throw runtime_error("Unable to verify whether a token with the symbol " + sym +
                            " already exists. Reading the database failed.");
    }
}

void AssertNTP1TokenNameIsNotAlreadyInMainChain(const NTP1Transaction& ntp1tx, const ITxDB& txdb)
{
    if (ntp1tx.getTxType() == NTP1TxType_ISSUANCE) {
        std::string sym = ntp1tx.getTokenSymbolIfIssuance();
        AssertNTP1TokenNameIsNotAlreadyInMainChain(sym, ntp1tx.getTxHash(), txdb);
    } else if (ntp1tx.getTxType() == NTP1TxType_UNKNOWN) {
        throw std::runtime_error("Attempted to " + std::string(__func__) +
                                 " on an uninitialized NTP1 transaction");
    }
}

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

    TRYV(tx.CheckTransaction(*txdb));

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
    string reason;
    if (Params().NetType() == NetworkType::Mainnet && !IsStandardTx(*txdb, tx, reason))
        return Err(MakeInvalidTxState(TxValidationResult::TX_NOT_STANDARD, reason, "non-standard-tx"));

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

        MapPrevTx                                                           mapInputs;
        map<uint256, CTxIndex>                                              mapUnused;
        map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>> mapUnused2;
        bool                                                                fInvalid = false;
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
                ntp1tx.readNTP1DataFromTx(*txdb, tx, inputsTxs);
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

// Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock
bool GetTransaction(const uint256& hash, CTransaction& tx, uint256& hashBlock)
{
    {
        if (mempool.lookup(hash, tx)) {
            return true;
        }
        const CTxDB txdb;
        CTxIndex    txindex;
        if (tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex)) {
            CBlock block;
            if (block.ReadFromDisk(txindex.pos.nBlockPos, txdb, false))
                hashBlock = block.GetHash();
            return true;
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

uint256 static GetOrphanRoot(const CBlock* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock->GetHash();
}

// ppcoin: find block wanted by given orphan block
uint256 WantedByOrphan(const CBlock* pblockOrphan)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblockOrphan->hashPrevBlock))
        pblockOrphan = mapOrphanBlocks[pblockOrphan->hashPrevBlock];
    return pblockOrphan->hashPrevBlock;
}

//
// maximum nBits value could possible be required nTime after
//
unsigned int ComputeMaxBits(CBigNum bnTargetLimit, unsigned int nBase, int64_t nTime)
{
    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    bnResult *= 2;
    while (nTime > 0 && bnResult < bnTargetLimit) {
        // Maximum 200% adjustment per day...
        bnResult *= 2;
        nTime -= 24 * 60 * 60;
    }
    if (bnResult > bnTargetLimit)
        bnResult = bnTargetLimit;
    return bnResult.GetCompact();
}

//
// minimum amount of work that could possibly be required nTime after
// minimum proof-of-work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    return ComputeMaxBits(Params().PoWLimit(), nBase, nTime);
}

//
// minimum amount of stake that could possibly be required nTime after
// minimum proof-of-stake required was nBase
//
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int /*nBlockTime*/)
{
    return ComputeMaxBits(Params().PoSLimit(), nBase, nTime);
}

// ppcoin: find last block index up to pindex
CBlockIndex GetLastBlockIndex(CBlockIndex index, bool fProofOfStake, const ITxDB& txdb)
{
    // if we're looking for a PoW block, we start at the latest one,
    // better than looping over the whole history
    if (!fProofOfStake && index.nHeight > Params().LastPoWBlock()) {
        const boost::optional<uint256> oHash = txdb.ReadBlockHashOfHeight(Params().LastPoWBlock());
        if (oHash) {
            if (auto bindex = txdb.ReadBlockIndex(*oHash)) {
                index = std::move(*bindex);
            } else {
                NLog.write(b_sev::critical, "Failed to read last PoW block's block index");
            }
        }
    }

    while (index.hashPrev != 0 && index.IsProofOfStake() != fProofOfStake) {
        const boost::optional<CBlockIndex> bindex = index.getPrev(txdb);
        if (bindex) {
            index = std::move(*bindex);
        } else if (index.blockHash == Params().GenesisBlockHash()) {
            return index;
        } else {
            NLog.write(b_sev::critical,
                       "Failed to get prev block index, even though it's not genesis block");
            break;
        }
    }
    return index;
}

static unsigned int GetNextTargetRequiredV1(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    const CTxDB txdb;

    CBigNum bnTargetLimit = fProofOfStake ? Params().PoSLimit() : Params().PoWLimit();

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex pindexPrev = GetLastBlockIndex(*pindexLast, fProofOfStake, txdb);
    if (pindexPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const boost::optional<CBlockIndex> indexPrevPrev = pindexPrev.getPrev(txdb);
    if (!indexPrevPrev) {
        NLog.write(b_sev::err,
                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex pindexPrevPrev = GetLastBlockIndex(*indexPrevPrev, fProofOfStake, txdb);
    if (pindexPrevPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev.GetBlockTime() - pindexPrevPrev.GetBlockTime();

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev.nBits);
    unsigned int nTS       = Params().TargetSpacing(CTxDB());
    int64_t      nInterval = Params().TargetTimeSpan() / nTS;
    bnNew *= ((nInterval - 1) * nTS + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTS);

    if (bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

/**
 * Calculates the average spacing between blocks correctly, by sorting the times of the last X blocks,
 * and calculating the average from adjacent differences
 *
 * @brief CalculateActualBlockSpacingForV3
 * @param pindexLast
 * @return the average time spacing between blocks
 */
int64_t CalculateActualBlockSpacingForV3(const ITxDB& txdb, const CBlockIndex* pindexLast,
                                         BlockTimeCacheType& blockTimeCache)
{

    // get the latest blocks from the blocks. The amount of blocks is: TARGET_AVERAGE_BLOCK_COUNT
    int64_t forkBlock =
        Params().GetNetForks().getFirstBlockOfFork(NetworkFork::NETFORK__4_RETARGET_CORRECTION);
    // we start counting block times from the fork
    int64_t numOfBlocksToAverage = pindexLast->nHeight - (forkBlock + 1);
    // minimum number of blocks to calculate a difference is 2, and max is TARGET_AVERAGE_BLOCK_COUNT
    if (numOfBlocksToAverage <= 1) {
        numOfBlocksToAverage = 2;
    }
    if (numOfBlocksToAverage > TARGET_AVERAGE_BLOCK_COUNT) {
        numOfBlocksToAverage = TARGET_AVERAGE_BLOCK_COUNT;
    }

    // push block times to a vector
    std::vector<int64_t> blockTimes;
    std::vector<int64_t> blockTimeDifferences;
    blockTimes.reserve(numOfBlocksToAverage);
    blockTimeDifferences.reserve(numOfBlocksToAverage);
    uint256 currHash = pindexLast->GetBlockHash();
    blockTimeCache.manualAdd(*pindexLast);
    blockTimes.resize(numOfBlocksToAverage);
    for (int64_t i = 0; i < numOfBlocksToAverage; i++) {
        const boost::optional<BlockTimeCacheType::BICacheEntry> t = blockTimeCache.get(txdb, currHash);
        if (!t) {
            NLog.write(b_sev::critical, "CRITICAL ERROR: block not found while calculating target");
            break;
        }
        // fill the blocks in reverse order
        blockTimes.at(numOfBlocksToAverage - i - 1) = t->value;
        // move to the previous block
        if (t->prevHash != 0) {
            currHash = t->prevHash;
        } else {
            NLog.write(b_sev::critical,
                       "CRITICAL ERROR: prev block has zero hash even though it's not genesis. "
                       "THIS SHOULD NEVER HAPPEN. Database corrupt?");
        }
    }

    // sort block times to avoid negative values
    std::sort(blockTimes.begin(), blockTimes.end());
    // calculate adjacent differences
    std::adjacent_difference(blockTimes.cbegin(), blockTimes.cend(),
                             std::back_inserter(blockTimeDifferences));
    assert(blockTimeDifferences.size() >= 2);
    // calculate the average (n-1 because adjacent differences have size n-1)
    // begin()+1 because the first value is just a copy of the first value
    return std::accumulate(blockTimeDifferences.cbegin() + 1, blockTimeDifferences.cend(), 0) /
           (numOfBlocksToAverage - 1);
}

static unsigned int GetNextTargetRequiredV2(const ITxDB& txdb, const CBlockIndex* pindexLast,
                                            bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? Params().PoSLimit() : Params().PoWLimit();

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex pindexPrev = GetLastBlockIndex(*pindexLast, fProofOfStake, txdb);
    if (pindexPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const boost::optional<CBlockIndex> indexPrevPrev = pindexPrev.getPrev(txdb);
    if (!indexPrevPrev) {
        NLog.write(b_sev::err,
                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex pindexPrevPrev = GetLastBlockIndex(*indexPrevPrev, fProofOfStake, txdb);

    int64_t      nActualSpacing = pindexPrev.GetBlockTime() - pindexPrevPrev.GetBlockTime();
    unsigned int nTS            = Params().TargetSpacing(pindexLast->nHeight);
    if (nActualSpacing < 0)
        nActualSpacing = nTS;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev.nBits);
    int64_t nInterval = Params().TargetTimeSpan() / nTS;
    bnNew *= ((nInterval - 1) * nTS + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTS);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

static unsigned int GetNextTargetRequiredV3(const ITxDB& txdb, const CBlockIndex* pindexLast,
                                            bool fProofOfStake, BlockTimeCacheType& blockTimeCache)
{
    CBigNum bnTargetLimit = fProofOfStake ? Params().PoSLimit() : Params().PoWLimit();

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex pindexPrev = GetLastBlockIndex(*pindexLast, fProofOfStake, txdb);
    if (pindexPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const boost::optional<CBlockIndex> indexPrevPrev = pindexPrev.getPrev(txdb);
    if (!indexPrevPrev) {
        NLog.write(b_sev::err,
                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex pindexPrevPrev = GetLastBlockIndex(*indexPrevPrev, fProofOfStake, txdb);
    if (pindexPrevPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = CalculateActualBlockSpacingForV3(txdb, pindexLast, blockTimeCache);

    const unsigned int nTS = Params().TargetSpacing(pindexLast->nHeight);
    if (nActualSpacing < 0)
        nActualSpacing = nTS;

    /** if any of these assert fires, it means that you changed these parameteres.
     *  Be aware that the parameters k and l are fine tuned to produce a max shift in the difficulty in
     * the range [-3%,+5%]
     * This can be calculated with:
     * ((nInterval - l + k)*nTS + (m + l)*nActualSpacing)/((nInterval + k)*nTS + m*nActualSpacing),
     * with nActualSpacing being in the range [0,FutureDrift] = [0,600] If you change any of these
     * values, make sure you tune these variables again. A very high percentage on either side makes it
     * easier to change/manipulate the difficulty when mining
     */
    assert(FutureDrift(0) == 10 * 60);
    assert(Params().TargetSpacing(pindexLast->nHeight) == 30);
    assert(Params().TargetTimeSpan() == 2 * 60 * 60);

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum newTarget;
    newTarget.SetCompact(pindexPrev.nBits); // target from previous block
    int64_t nInterval = Params().TargetTimeSpan() / nTS;

    static constexpr const int k = 15;
    static constexpr const int l = 7;
    static constexpr const int m = 90;
    newTarget *= (nInterval - l + k) * nTS + (m + l) * nActualSpacing;
    newTarget /= (nInterval + k) * nTS + m * nActualSpacing;

    if (newTarget <= 0 || newTarget > bnTargetLimit)
        newTarget = bnTargetLimit;

    return newTarget.GetCompact();
}

unsigned int GetNextTargetRequired(const ITxDB& txdb, const CBlockIndex* pindexLast, bool fProofOfStake)
{
    if (!fProofOfStake && Params().MineBlocksOnDemand())
        return pindexLast->nBits;
    if (fProofOfStake && Params().MineBlocksOnDemand())
        return Params().PoWLimit().GetCompact();

    static thread_local typename BlockTimeCacheType::ExtractorFunc extractorFunc =
        [](const CBlockIndex& bi) { return bi.GetBlockTime(); };

    static thread_local BlockTimeCacheType blockIndexBlockTimeCache(500, extractorFunc);

    if (pindexLast->nHeight < 2000)
        return GetNextTargetRequiredV1(pindexLast, fProofOfStake);
    else if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__4_RETARGET_CORRECTION,
                                                    pindexLast->nHeight))
        return GetNextTargetRequiredV3(txdb, pindexLast, fProofOfStake, blockIndexBlockTimeCache);
    else
        return GetNextTargetRequiredV2(txdb, pindexLast, fProofOfStake);
}

bool CheckProofOfWork(const uint256& hash, unsigned int nBits, bool silent)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > Params().PoWLimit()) {
        if (silent) {
            return false;
        } else {
            return NLog.error("CheckProofOfWork() : nBits below minimum work");
        }
    }

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256()) {
        if (silent) {
            return false;
        } else {
            return NLog.error("CheckProofOfWork() : hash doesn't match nBits");
        }
    }

    return true;
}

// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    return std::max(cPeerBlockCounts.median(), Checkpoints::GetTotalBlocksEstimate());
}

bool IsInitialBlockDownload(const ITxDB& txdb)
{
    // Once this function has returned false, it must remain false.
    static std::atomic<bool> latchToFalse{false};
    // Optimization: pre-test latch before taking the lock.
    if (latchToFalse.load(std::memory_order_acquire))
        return false;

    if (txdb.GetBestChainHeight().value_or(0) < Checkpoints::GetTotalBlocksEstimate())
        return true;
    if (fImporting)
        return true;
    static int64_t                      nLastUpdate;
    static boost::optional<CBlockIndex> pindexLastBest;
    const boost::optional<CBlockIndex>  pindexBestPtr = txdb.GetBestBlockIndex();
    if (!pindexBestPtr) {
        NLog.write(b_sev::critical, "CRITICAL ERROR: Best block index return none!");
        return false;
    }

    if (!pindexLastBest || pindexBestPtr->GetBlockHash() != pindexLastBest->GetBlockHash()) {
        pindexLastBest = pindexBestPtr;
        nLastUpdate    = GetTime();
    }

    const int64_t timeNow       = GetTime();
    const int64_t bestBlockTime = pindexBestPtr->GetBlockTime();

    const bool lastTwoBlocksCameMuchFasterThanBlockTime = timeNow - nLastUpdate < 15;
    const bool lastBlockIsTooOld                        = bestBlockTime < timeNow - 8 * 60 * 60;

    const bool tooNew = (!lastTwoBlocksCameMuchFasterThanBlockTime && !lastBlockIsTooOld);
    if (tooNew) {
        latchToFalse.store(true, std::memory_order_seq_cst);
    }
    return tooNew;
}

CDiskTxPos CreateFakeSpentTxPos(const uint256& blockhash)
{
    /// this creates a fake tx position that helps in marking an output as spent
    CDiskTxPos fakeTxPos;
    fakeTxPos.nBlockPos = blockhash;
    fakeTxPos.nTxPos    = 1; // invalid position just to mark the tx as spent temporarily
    assert(!fakeTxPos.IsNull());
    return fakeTxPos;
}

void FetchNTP1TxFromDisk(std::pair<CTransaction, NTP1Transaction>& txPair, const ITxDB& txdb,
                         bool /*recoverProtection*/, unsigned /*recurseDepth*/)
{
    if (!NTP1Transaction::IsTxNTP1(&txPair.first)) {
        return;
    }
    if (!txdb.ReadNTP1Tx(txPair.first.GetHash(), txPair.second)) {
        NLog.write(b_sev::err, "Failed to fetch NTP1 transaction {}", txPair.first.GetHash().ToString());
        return;
    }
    txPair.second.updateDebugStrHash();
}

void WriteNTP1TxToDbAndDisk(const NTP1Transaction& ntp1tx, ITxDB& txdb)
{
    if (ntp1tx.getTxType() == NTP1TxType_UNKNOWN) {
        throw std::runtime_error(
            "Attempted to write an NTP1 transaction to database with unknown type.");
    }
    if (!txdb.WriteNTP1Tx(ntp1tx.getTxHash(), ntp1tx)) {
        throw std::runtime_error("Unable to write NTP1 transaction to database: " +
                                 ntp1tx.getTxHash().ToString());
    }
    if (ntp1tx.getTxType() == NTP1TxType_ISSUANCE) {
        if (ntp1tx.getTxInCount() <= 0) {
            throw std::runtime_error(
                "Unable to check for token id blacklisting because the size of the input is zero.");
        }
        NTP1OutPoint prevout = ntp1tx.getTxIn(0).getPrevout();
        assert(!prevout.isNull());
        std::string tokenId =
            ntp1tx.getTokenIdIfIssuance(prevout.getHash().ToString(), prevout.getIndex());
        if (!Params().IsNTP1TokenBlacklisted(tokenId)) {
            if (!txdb.WriteNTP1TxWithTokenSymbol(ntp1tx.getTokenSymbolIfIssuance(), ntp1tx)) {
                throw std::runtime_error("Unable to write NTP1 transaction to database: " +
                                         ntp1tx.getTxHash().ToString());
            }
        }
    }
}

void WriteNTP1TxToDiskFromRawTx(const CTransaction& tx, ITxDB& txdb)
{
    if (Params().PassedFirstValidNTP1Tx(&txdb)) {
        // read previous transactions (inputs) which are necessary to validate an NTP1
        // transaction
        std::string opReturnArg;
        if (!NTP1Transaction::IsTxNTP1(&tx, &opReturnArg)) {
            return;
        }

        std::vector<std::pair<CTransaction, NTP1Transaction>> inputsWithNTP1 =
            NTP1Transaction::GetAllNTP1InputsOfTx(tx, txdb, true);

        // write NTP1 transactions' data
        NTP1Transaction ntp1tx;
        ntp1tx.readNTP1DataFromTx(txdb, tx, inputsWithNTP1);

        WriteNTP1TxToDbAndDisk(ntp1tx, txdb);
    }
}

void AssertIssuanceUniquenessInBlock(
    std::unordered_map<std::string, uint256>& issuedTokensSymbolsInThisBlock, const ITxDB& txdb,
    const CTransaction&                                                        tx,
    const map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>& mapQueuedNTP1Inputs,
    const map<uint256, CTxIndex>&                                              queuedAcceptedTxs)
{
    std::string opRet;
    if (NTP1Transaction::IsTxNTP1(&tx, &opRet)) {
        auto script = NTP1Script::ParseScript(opRet);
        if (script->getTxType() == NTP1Script::TxType_Issuance) {
            std::vector<std::pair<CTransaction, NTP1Transaction>> inputsTxs =
                NTP1Transaction::GetAllNTP1InputsOfTx(tx, txdb, false, mapQueuedNTP1Inputs,
                                                      queuedAcceptedTxs);

            NTP1Transaction ntp1tx;
            ntp1tx.readNTP1DataFromTx(txdb, tx, inputsTxs);
            AssertNTP1TokenNameIsNotAlreadyInMainChain(ntp1tx, txdb);
            if (ntp1tx.getTxType() == NTP1TxType_ISSUANCE) {
                std::string currSymbol = ntp1tx.getTokenSymbolIfIssuance();
                // make sure that case doesn't matter by converting to upper case
                std::transform(currSymbol.begin(), currSymbol.end(), currSymbol.begin(), ::toupper);
                if (issuedTokensSymbolsInThisBlock.find(currSymbol) !=
                    issuedTokensSymbolsInThisBlock.end()) {
                    throw std::runtime_error(
                        "The token name " + currSymbol +
                        " already exists in the block: " /* + this->GetHash().ToString()*/);
                }
                issuedTokensSymbolsInThisBlock.insert(std::make_pair(currSymbol, ntp1tx.getTxHash()));
            }
        }
    }
}

void static PruneOrphanBlocks()
{
    static const size_t MAX_SIZE_P =
        (size_t)std::max(INT64_C(0), GetArg("-maxorphanblocks", DEFAULT_MAX_ORPHAN_BLOCKS));
    if (mapOrphanBlocksByPrev.size() <= MAX_SIZE_P)
        return;

    // Pick a random orphan block.
    int                                       pos = insecure_rand() % mapOrphanBlocksByPrev.size();
    std::multimap<uint256, CBlock*>::iterator it  = mapOrphanBlocksByPrev.begin();
    std::advance(it, pos);

    // As long as this block has other orphans depending on it, move to one of those successors.
    do {
        std::multimap<uint256, CBlock*>::iterator it2 =
            mapOrphanBlocksByPrev.find(it->second->GetHash());
        if (it2 == mapOrphanBlocksByPrev.end())
            break;
        it = it2;
    } while (true);

    NLog.write(
        b_sev::info,
        "Removing block {} from orphans map as the size of the orphans has exceeded the maximum {}; "
        "current size: {}",
        it->second->GetHash().ToString(), MAX_SIZE_P, mapOrphanBlocksByPrev.size());
    uint256 hash = it->second->GetHash();
    delete it->second;
    mapOrphanBlocksByPrev.erase(it);
    mapOrphanBlocks.erase(hash);
}

void WriteNTP1BlockTransactionsToDisk(const std::vector<CTransaction>& vtx, ITxDB& txdb)
{
    if (Params().PassedFirstValidNTP1Tx(&txdb)) {
        for (const CTransaction& tx : vtx) {
            WriteNTP1TxToDiskFromRawTx(tx, txdb);
        }
    }
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock)
{
    AssertLockHeld(cs_main);
    const uint256 hash = pblock->GetHash();
    {
        const CTxDB txdb;

        // Check for duplicate
        if (auto v = txdb.ReadBlockIndex(hash))
            return NLog.error("ProcessBlock() : already have block {} {}", v->nHeight, hash.ToString());
        if (mapOrphanBlocks.count(hash))
            return NLog.error("ProcessBlock() : already have block (orphan) {}", hash.ToString());

        // ppcoin: check proof-of-stake
        // Limited duplicity on stake: prevents block flood attack
        // Duplicate stake allowed only when there is orphan child block
        if (pblock->IsProofOfStake() && txdb.WasStakeSeen(pblock->GetProofOfStake()).value_or(false) &&
            !mapOrphanBlocksByPrev.count(hash)) {
            return NLog.error("ProcessBlock() : duplicate proof-of-stake ({}, {}) for block {}",
                              pblock->GetProofOfStake().first.ToString(),
                              pblock->GetProofOfStake().second, hash.ToString());
        }

        // Preliminary checks
        if (!pblock->CheckBlock(txdb, hash))
            return NLog.error("ProcessBlock() : CheckBlock FAILED");

        const boost::optional<CBlockIndex> checkpoint = Checkpoints::GetLastCheckpoint(txdb);
        if (checkpoint && pblock->hashPrevBlock != txdb.GetBestBlockHash()) {
            // Extra checks to prevent "fill up memory by spamming with bogus blocks"
            int64_t deltaTime = pblock->GetBlockTime() - checkpoint->nTime;
            CBigNum bnNewBlock;
            bnNewBlock.SetCompact(pblock->nBits);
            CBigNum bnRequired;

            if (pblock->IsProofOfStake()) {
                const CBlockIndex& bi = GetLastBlockIndex(*checkpoint, true, txdb);
                bnRequired.SetCompact(ComputeMinStake(bi.nBits, deltaTime, pblock->nTime));
            } else {
                const CBlockIndex& bi = GetLastBlockIndex(*checkpoint, false, txdb);
                bnRequired.SetCompact(ComputeMinWork(bi.nBits, deltaTime));
            }

            if (bnNewBlock > bnRequired) {
                if (pfrom)
                    pfrom->Misbehaving(100);
                return NLog.error("ProcessBlock() : block with too little {}",
                                  pblock->IsProofOfStake() ? "proof-of-stake" : "proof-of-work");
            }
        }

        const boost::optional<CBlockIndex> prevBlockIndex = txdb.ReadBlockIndex(pblock->hashPrevBlock);

        // If don't already have its previous block, shunt it off to holding area until we get it
        if (!prevBlockIndex) {
            NLog.write(b_sev::info, "ProcessBlock: ORPHAN BLOCK, prev={}",
                       pblock->hashPrevBlock.ToString());
            // ppcoin: check proof-of-stake
            if (pblock->IsProofOfStake()) {
                // Limited duplicity on stake: prevents block flood attack
                // Duplicate stake allowed only when there is orphan child block
                if (setStakeSeenOrphan.count(pblock->GetProofOfStake()) &&
                    !mapOrphanBlocksByPrev.count(hash))
                    return NLog.error(
                        "ProcessBlock() : duplicate proof-of-stake ({}, {}) for orphan block {}",
                        pblock->GetProofOfStake().first.ToString(), pblock->GetProofOfStake().second,
                        hash.ToString());
                else
                    setStakeSeenOrphan.insert(pblock->GetProofOfStake());
            }
            PruneOrphanBlocks();
            CBlock* pblock2 = new CBlock(*pblock);
            mapOrphanBlocks.insert(make_pair(hash, pblock2));
            mapOrphanBlocksByPrev.insert(make_pair(pblock2->hashPrevBlock, pblock2));

            // Ask this guy to fill in what we're missing
            if (pfrom) {
                const boost::optional<CBlockIndex> bestBlockIndex = txdb.GetBestBlockIndex();
                pfrom->PushGetBlocks(&*bestBlockIndex, GetOrphanRoot(pblock2));
                // ppcoin: getblocks may not obtain the ancestor block rejected
                // earlier by duplicate-stake check so we ask for it again directly
                if (!IsInitialBlockDownload(txdb))
                    pfrom->AskFor(CInv(MSG_BLOCK, WantedByOrphan(pblock2)));
            }
            return true;
        }

        // Store to disk
        NLog.write(b_sev::info, "Attempting to accept block of height {} with hash {}",
                   prevBlockIndex->nHeight + 1, hash.ToString());
        if (!pblock->AcceptBlock(*prevBlockIndex, hash))
            return NLog.error("ProcessBlock() : AcceptBlock FAILED");
    }

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++) {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev); ++mi) {
            CBlock* pblockOrphan = (*mi).second;

            // we use a new instance of CTxDB to ensure that newly added blocks are included
            const CTxDB txdbNew;

            const boost::optional<CBlockIndex> prevBlockIdx = txdbNew.ReadBlockIndex(hashPrev);
            if (!prevBlockIdx) {
                NLog.write(b_sev::critical,
                           "CRITICAL ERROR: A prev block was not found after having been "
                           "added! This should NEVER happen.");
                continue;
            }

            if (pblockOrphan->AcceptBlock(*prevBlockIdx, pblockOrphan->GetHash()))
                vWorkQueue.push_back(pblockOrphan->GetHash());

            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            setStakeSeenOrphan.erase(pblockOrphan->GetProofOfStake());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    NLog.write(b_sev::info, "ProcessBlock: ACCEPTED: {}", hash.ToString());

    return true;
}

CMerkleBlock::CMerkleBlock(const CBlock& block, CBloomFilter& filter)
{
    header = block.GetBlockHeader();

    vector<bool>    vMatch;
    vector<uint256> vHashes;

    vMatch.reserve(block.vtx.size());
    vHashes.reserve(block.vtx.size());

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        uint256 hash = block.vtx[i].GetHash();
        if (filter.IsRelevantAndUpdate(block.vtx[i])) {
            vMatch.push_back(true);
            vMatchedTxn.push_back(make_pair(i, hash));
        } else {
            vMatch.push_back(false);
        }
        vHashes.push_back(hash);
    }

    txn = CPartialMerkleTree(vHashes, vMatch);
}

uint256 CPartialMerkleTree::CalcHash(int height, unsigned int pos, const std::vector<uint256>& vTxid)
{
    if (height == 0) {
        // hash at height 0 is the txids themself
        return vTxid[pos];
    } else {
        // calculate left hash
        uint256 left = CalcHash(height - 1, pos * 2, vTxid), right;
        // calculate right hash if not beyong the end of the array - copy left hash otherwise1
        if (pos * 2 + 1 < CalcTreeWidth(height - 1))
            right = CalcHash(height - 1, pos * 2 + 1, vTxid);
        else
            right = left;
        // combine subhashes
        return Hash(BEGIN(left), END(left), BEGIN(right), END(right));
    }
}

void CPartialMerkleTree::TraverseAndBuild(int height, unsigned int pos,
                                          const std::vector<uint256>& vTxid,
                                          const std::vector<bool>&    vMatch)
{
    // determine whether this node is the parent of at least one matched txid
    bool fParentOfMatch = false;
    for (unsigned int p = pos << height; p < (pos + 1) << height && p < nTransactions; p++)
        fParentOfMatch |= vMatch[p];
    // store as flag bit
    vBits.push_back(fParentOfMatch);
    if (height == 0 || !fParentOfMatch) {
        // if at height 0, or nothing interesting below, store hash and stop
        vHash.push_back(CalcHash(height, pos, vTxid));
    } else {
        // otherwise, don't store any hash, but descend into the subtrees
        TraverseAndBuild(height - 1, pos * 2, vTxid, vMatch);
        if (pos * 2 + 1 < CalcTreeWidth(height - 1))
            TraverseAndBuild(height - 1, pos * 2 + 1, vTxid, vMatch);
    }
}

uint256 CPartialMerkleTree::TraverseAndExtract(int height, unsigned int pos, unsigned int& nBitsUsed,
                                               unsigned int& nHashUsed, std::vector<uint256>& vMatch)
{
    if (nBitsUsed >= vBits.size()) {
        // overflowed the bits array - failure
        fBad = true;
        return 0;
    }
    bool fParentOfMatch = vBits[nBitsUsed++];
    if (height == 0 || !fParentOfMatch) {
        // if at height 0, or nothing interesting below, use stored hash and do not descend
        if (nHashUsed >= vHash.size()) {
            // overflowed the hash array - failure
            fBad = true;
            return 0;
        }
        const uint256& hash = vHash[nHashUsed++];
        if (height == 0 && fParentOfMatch) // in case of height 0, we have a matched txid
            vMatch.push_back(hash);
        return hash;
    } else {
        // otherwise, descend into the subtrees to extract matched txids and hashes
        uint256 left = TraverseAndExtract(height - 1, pos * 2, nBitsUsed, nHashUsed, vMatch), right;
        if (pos * 2 + 1 < CalcTreeWidth(height - 1))
            right = TraverseAndExtract(height - 1, pos * 2 + 1, nBitsUsed, nHashUsed, vMatch);
        else
            right = left;
        // and combine them before returning
        return Hash(BEGIN(left), END(left), BEGIN(right), END(right));
    }
}

CPartialMerkleTree::CPartialMerkleTree(const std::vector<uint256>& vTxid,
                                       const std::vector<bool>&    vMatch)
    : nTransactions(vTxid.size()), fBad(false)
{
    // reset state
    vBits.clear();
    vHash.clear();

    // calculate height of tree
    int nHeight = 0;
    while (CalcTreeWidth(nHeight) > 1)
        nHeight++;

    // traverse the partial tree
    TraverseAndBuild(nHeight, 0, vTxid, vMatch);
}

CPartialMerkleTree::CPartialMerkleTree() : nTransactions(0), fBad(true) {}

uint256 CPartialMerkleTree::ExtractMatches(std::vector<uint256>& vMatch)
{
    vMatch.clear();
    // An empty set will not work
    if (nTransactions == 0)
        return 0;
    // check for excessively high numbers of transactions
    unsigned int nSizeLimit = MaxBlockSize(CTxDB());
    if (nTransactions >
        nSizeLimit / 60) // 60 is the lower bound for the size of a serialized CTransaction
        return 0;
    // there can never be more hashes provided than one for every txid
    if (vHash.size() > nTransactions)
        return 0;
    // there must be at least one bit per node in the partial tree, and at least one node per hash
    if (vBits.size() < vHash.size())
        return 0;
    // calculate height of tree
    int nHeight = 0;
    while (CalcTreeWidth(nHeight) > 1)
        nHeight++;
    // traverse the partial tree
    unsigned int nBitsUsed = 0, nHashUsed = 0;
    uint256      hashMerkleRoot = TraverseAndExtract(nHeight, 0, nBitsUsed, nHashUsed, vMatch);
    // verify that no problems occured during the tree traversal
    if (fBad)
        return 0;
    // verify that all bits were consumed (except for the padding caused by serializing it as a byte
    // sequence)
    if ((nBitsUsed + 7) / 8 != (vBits.size() + 7) / 8)
        return 0;
    // verify that all hashes were consumed
    if (nHashUsed != vHash.size())
        return 0;
    return hashMerkleRoot;
}

bool CheckDiskSpace(std::uintmax_t nAdditionalBytes)
{
    std::uintmax_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes) {
        fShutdown         = true;
        string strMessage = _("Warning: Disk space is low!");
        strMiscWarning    = strMessage;
        NLog.write(b_sev::err, "*** {}", strMessage);
        uiInterface.ThreadSafeMessageBox(strMessage, "neblio",
                                         CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION |
                                             CClientUIInterface::MODAL);
        StartShutdown();
        return false;
    }
    return true;
}

bool LoadBlockIndex(bool fAllowNew)
{
    LOCK(cs_main);

    //
    // Load block index
    //
    {
        CTxDB txdb;
        txdb.resyncIfNecessary();
        if (!txdb.LoadBlockIndex())
            return false;

        //
        // Init with genesis block
        //
        if (!txdb.GetBestBlockIndex()) {
            if (!fAllowNew)
                return false;

            CBlock genesisBlock = Params().GenesisBlock();

            //// debug print
            genesisBlock.print();

            NLog.write(b_sev::info, "block.GetHash() == {}", genesisBlock.GetHash().ToString());
            NLog.write(b_sev::info, "block.hashMerkleRoot == {}",
                       genesisBlock.hashMerkleRoot.ToString());
            NLog.write(b_sev::info, "block.nTime = {}", genesisBlock.nTime);
            NLog.write(b_sev::info, "block.nNonce = {}", genesisBlock.nNonce);

            assert(genesisBlock.hashMerkleRoot == Params().GenesisBlock().hashMerkleRoot);
            assert(genesisBlock.GetHash() == Params().GenesisBlockHash());
            assert(genesisBlock.CheckBlock(txdb, Params().GenesisBlockHash()));

            // Start new block file
            if (!genesisBlock.WriteToDisk(boost::none, genesisBlock.GetHash(), genesisBlock.GetHash()))
                return NLog.error("LoadBlockIndex() : writing genesis block to disk failed");
        }
    }

    return true;
}

void PrintBlockTree()
{
    AssertLockHeld(cs_main);

    const CTxDB txdb;

    const auto blockIndexMap = txdb.ReadAllBlockIndexEntries();

    if (!blockIndexMap) {
        NLog.write(b_sev::err, "Failed to read block index from database");
        return;
    }

    // pre-compute tree structure
    map<uint256, vector<boost::optional<CBlockIndex>>> mapNext;
    for (BlockIndexMapType::MapType::const_iterator mi = blockIndexMap->cbegin();
         mi != blockIndexMap->cend(); ++mi) {
        const boost::optional<CBlockIndex> pindex = mi->second;
        mapNext[pindex->GetBlockHash()].push_back(pindex);
        // test
        // while (rand() % 3 == 0)
        //    mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, boost::optional<CBlockIndex>>> vStack;
    vStack.push_back(make_pair(0, *pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty()) {
        int                          nCol   = vStack.back().first;
        boost::optional<CBlockIndex> pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        std::stringstream b;
        if (nCol > nPrevCol) {
            for (int i = 0; i < nCol - 1; i++)
                b << "| ";
            b << "|";
        } else if (nCol < nPrevCol) {
            for (int i = 0; i < nCol; i++)
                b << "| ";
            b << "|";
        }
        NLog.write(b_sev::info, b.str());
        b.str("");

        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            b << "| ";
        NLog.write(b_sev::info, "{}", b.str());

        // print item
        CBlock block;
        block.ReadFromDisk(&*pindex, txdb);
        NLog.write(b_sev::info, "{} ({}) {}  {:08x}  {}  tx {}", pindex->nHeight,
                   pindex->GetBlockHash().ToString(), block.GetHash().ToString(), block.nBits,
                   DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()), block.vtx.size());

        PrintWallets(block);

        // put the main time-chain first
        vector<boost::optional<CBlockIndex>>& vNext = mapNext[pindex->GetBlockHash()];
        for (unsigned int i = 0; i < vNext.size(); i++) {
            if (vNext[i]->getNext(txdb)) {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol + i, vNext[i]));
    }
}

bool LoadExternalBlockFile(FILE* fileIn)
{
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    {
        try {
            CAutoFile    blkdat(fileIn, SER_DISK, CLIENT_VERSION);
            unsigned int nPos = 0;
            while (nPos != (unsigned int)-1 && blkdat.good() && !fRequestShutdown && !fShutdown) {
                unsigned char pchData[65536];
                do {
                    fseek(blkdat, nPos, SEEK_SET);
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat);
                    if (nRead <= 8) {
                        nPos = (unsigned int)-1;
                        break;
                    }
                    void* nFind = memchr(pchData, Params().MessageStart()[0],
                                         nRead + 1 - CMessageHeader::MESSAGE_START_SIZE);
                    if (nFind) {
                        if (memcmp(nFind, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE) ==
                            0) {
                            nPos +=
                                ((unsigned char*)nFind - pchData) + CMessageHeader::MESSAGE_START_SIZE;
                            break;
                        }
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                    } else
                        nPos += sizeof(pchData) - CMessageHeader::MESSAGE_START_SIZE + 1;
                } while (!fRequestShutdown && !fShutdown);
                if (nPos == (unsigned int)-1)
                    break;
                unsigned int nSizeLimit = MaxBlockSize(CTxDB());

                fseek(blkdat, nPos, SEEK_SET);

                unsigned int nSize;
                blkdat >> nSize;

                // this is just for debugging
                // static const unsigned int fileStartFrom = 0;
                // if (nPos < fileStartFrom) {
                //     nPos += 4 + nSize;
                //     NLog.write(b_sev::info, "Skipping block at file pos: {}", nPos);
                //     continue;
                // }

                if (nSize > 0 && nSize <= nSizeLimit) {
                    CBlock block;
                    blkdat >> block;
                    NLog.write(b_sev::info, "Reading block at file pos: {}", nPos);

                    LOCK(cs_main);

                    if (ProcessBlock(nullptr, &block)) {
                        nLoaded++;
                        nPos += 4 + nSize;
                    }
                }
            }
        } catch (std::exception& e) {
            NLog.write(b_sev::err, "{} : Deserialize or I/O error caught during load", FUNCTIONSIG);
        }
    }
    NLog.write(b_sev::info, "Loaded {} blocks from external file in {} ms", nLoaded,
               GetTimeMillis() - nStart);
    return nLoaded > 0;
}

struct CImportingNow
{
    CImportingNow()
    {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow()
    {
        assert(fImporting == true);
        fImporting = false;
    }
};

void ThreadImport(const std::vector<boost::filesystem::path> vFiles)
{
    RenameThread("bitcoin-loadblk");

    CImportingNow imp;
    vnThreadsRunning[THREAD_IMPORT]++;

    // -loadblock=
    // uiInterface.InitMessage(_("Starting block import..."));
    for (const boost::filesystem::path& path : vFiles) {
        FILE* file = fopen(path.string().c_str(), "rb");
        if (file)
            LoadExternalBlockFile(file);
    }

    // hardcoded $DATADIR/bootstrap.dat
    filesystem::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (filesystem::exists(pathBootstrap)) {
        // uiInterface.InitMessage(_("Importing bootstrap blockchain data file."));

        FILE* file = fopen(pathBootstrap.string().c_str(), "rb");
        if (file) {
            filesystem::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LoadExternalBlockFile(file);
            RenameOver(pathBootstrap, pathBootstrapOld);
        }
    }

    vnThreadsRunning[THREAD_IMPORT]--;
}

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

extern map<uint256, CAlert> mapAlerts;
extern CCriticalSection     cs_mapAlerts;

string GetWarnings(string strFor)
{
    int    nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (GetBoolArg("-testsafemode"))
        strRPC = "test";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "") {
        nPriority    = 1000;
        strStatusBar = strMiscWarning;
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        for (PAIRTYPE(const uint256, CAlert) & item : mapAlerts) {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority) {
                nPriority    = alert.nPriority;
                strStatusBar = alert.strStatusBar;
                if (nPriority > 1000)
                    strRPC = strStatusBar;
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}

//////////////////////////////////////////////////////////////////////////////
//
// Messages
//

bool static AlreadyHave(const CTxDB& txdb, const CInv& inv)
{
    switch (inv.type) {
    case MSG_TX: {
        bool txInMap = false;
        txInMap      = mempool.exists(inv.hash);
        return txInMap || mapOrphanTransactions.count(inv.hash) || txdb.ContainsTx(inv.hash);
    }

    case MSG_BLOCK:
        return txdb.ReadBlockIndex(inv.hash) || mapOrphanBlocks.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    static map<CService, CPubKey> mapReuseKey;
    RandAddSeedPerfmon();
    if (fDebug)
        NLog.write(b_sev::debug, "received: {} ({} bytes)", strCommand, vRecv.size());
    const boost::optional<std::string> dropMessageTest = mapArgs.get("-dropmessagestest");
    if (dropMessageTest && GetRand(atoi(*dropMessageTest)) == 0) {
        NLog.write(b_sev::info, "dropmessagestest DROPPING RECV MESSAGE");
        return true;
    }

    if (strCommand == "version") {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0) {
            pfrom->Misbehaving(1);
            return false;
        }

        int64_t  nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        int minPeerVer = MinPeerVersion(CTxDB());
        if (pfrom->nVersion < minPeerVer) {
            // disconnect from peers older than this proto version
            NLog.write(b_sev::info, "partner {} using obsolete version {}; disconnecting",
                       pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty())
            vRecv >> pfrom->strSubVer;
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;
        if (!vRecv.empty())
            vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
        else
            pfrom->fRelayTxes = true;

        if (pfrom->fInbound && addrMe.IsRoutable()) {
            pfrom->addrLocal = addrMe;
            SeenLocal(addrMe);
        }

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1) {
            NLog.write(b_sev::info, "connected to self at {}, disconnecting", pfrom->addr.ToString());
            pfrom->fDisconnect = true;
            return true;
        }

        // record my external IP reported by peer
        if (addrFrom.IsRoutable() && addrMe.IsRoutable())
            addrSeenByPeer.get() = addrMe;

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        if (GetBoolArg("-synctime", true))
            AddTimeData(pfrom->addr, nTime);

        // Change version
        pfrom->PushMessage("verack");
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound) {
            // Advertise our address
            if (!fNoListen && !IsInitialBlockDownload(CTxDB())) {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable())
                    pfrom->PushAddress(addr);
            }

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION ||
                addrman.get().size() < 1000) {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            addrman.get().Good(pfrom->addr);
        } else {
            if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom) {
                auto lock = addrman.get_lock();
                addrman.get_unsafe().Add(addrFrom, addrFrom);
                addrman.get_unsafe().Good(addrFrom);
            }
        }

        // Ask the first connected node for block updates
        // For regtest, we need to sync immediately after connection; this is important for tests that
        // split and reconnect the network
        CTxDB      txdb;
        static int nAskedForBlocks = 0;
        if ((!pfrom->fClient && !pfrom->fOneShot && !fImporting) &&
            (((pfrom->nStartingHeight > (txdb.GetBestChainHeight().value_or(0) - 144)) &&
              (pfrom->nVersion < NOBLKS_VERSION_START || pfrom->nVersion >= NOBLKS_VERSION_END) &&
              (nAskedForBlocks < 1 || vNodes.size() <= 1)) ||
             Params().NetType() == NetworkType::Regtest)) {
            nAskedForBlocks++;
            const boost::optional<CBlockIndex> best = txdb.GetBestBlockIndex();
            pfrom->PushGetBlocks(&*best, uint256(0));
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            for (PAIRTYPE(const uint256, CAlert) & item : mapAlerts)
                item.second.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        NLog.write(b_sev::info,
                   "receive version message: version {}, blocks={}, us={}, them={}, peer={}",
                   pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString(), addrFrom.ToString(),
                   pfrom->addr.ToString());

        cPeerBlockCounts.input(pfrom->nStartingHeight);
    }

    else if (pfrom->nVersion == 0) {
        // Must have a version message before anything else
        pfrom->Misbehaving(1);
        return false;
    }

    else if (strCommand == "verack") {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
    }

    else if (strCommand == "addr") {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.get().size() > 1000)
            return true;
        if (vAddr.size() > 1000) {
            pfrom->Misbehaving(20);
            return NLog.error("message addr size() = {}", vAddr.size());
        }

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t          nNow   = GetAdjustedTime();
        int64_t          nSince = nNow - 10 * 60;
        for (CAddress& addr : vAddr) {
            if (fShutdown)
                return true;
            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable()) {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256  hashRand =
                        hashSalt ^ (hashAddr << 32) ^ ((GetTime() + hashAddr) / (24 * 60 * 60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    for (CNode* pnode : vNodes) {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey         = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes =
                        fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin();
                         mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.get().Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }

    else if (strCommand == "inv") {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ) {
            pfrom->Misbehaving(20);
            return NLog.error("message inv size() = {}", vInv.size());
        }

        // find last block in inv vector
        unsigned int nLastBlock = (unsigned int)(-1);
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK) {
                nLastBlock = vInv.size() - 1 - nInv;
                break;
            }
        }
        const CTxDB txdb;
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            const CInv& inv = vInv[nInv];

            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);

            {
                bool fAlreadyHave = AlreadyHave(txdb, inv);
                if (fDebug)
                    NLog.write(b_sev::debug, "got inventory: {}  {}", inv.ToString(),
                               fAlreadyHave ? "have" : "new");

                if (!fAlreadyHave) {
                    if (!fImporting)
                        pfrom->AskFor(inv);
                } else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                    const boost::optional<CBlockIndex> best = txdb.GetBestBlockIndex();
                    pfrom->PushGetBlocks(&*best, GetOrphanRoot(mapOrphanBlocks[inv.hash]));
                } else if (nInv == nLastBlock) {
                    // In case we are on a very long side-chain, it is possible that we already have
                    // the last block in an inv bundle sent in response to getblocks. Try to detect
                    // this situation and push another getblocks to continue.
                    const boost::optional<CBlockIndex> bi = txdb.ReadBlockIndex(inv.hash);
                    if (bi) {
                        pfrom->PushGetBlocks(&*bi, uint256(0));
                    } else {
                        pfrom->PushGetBlocks(nullptr, uint256(0));
                    }
                    if (fDebug)
                        NLog.write(b_sev::debug, "force request: {}", inv.ToString());
                }
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }

    else if (strCommand == "getdata") {
        const CTxDB txdb;

        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ) {
            pfrom->Misbehaving(20);
            return NLog.error("message getdata size() = {}", vInv.size());
        }

        if (fDebugNet || (vInv.size() != 1))
            NLog.write(b_sev::debug, "received getdata ({} invsz)", vInv.size());

        for (const CInv& inv : vInv) {
            if (fShutdown)
                return true;
            if (fDebugNet || (vInv.size() == 1))
                NLog.write(b_sev::debug, "received getdata for: {}", inv.ToString());

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK) {
                // Send block from disk
                auto mi = txdb.ReadBlockIndex(inv.hash);
                if (mi) {
                    CBlock block;
                    block.ReadFromDisk(&*mi, txdb);
                    if (inv.type == MSG_BLOCK)
                        pfrom->PushMessage("block", block);
                    else // MSG_FILTERED_BLOCK)
                    {
                        LOCK(pfrom->cs_filter);
                        if (pfrom->pfilter) {
                            CMerkleBlock merkleBlock(block, *pfrom->pfilter);
                            // CMerkleBlock just contains hashes, so also push any transactions in the
                            // block the client did not see This avoids hurting performance by
                            // pointlessly requiring a round-trip Note that there is currently no way for
                            // a node to request any single transactions we didnt send here - they must
                            // either disconnect and retry or request the full block. Thus, the protocol
                            // spec specified allows for us to provide duplicate txn here, however we
                            // MUST always provide at least what the remote peer needs
                            typedef std::pair<unsigned int, uint256> PairType;
                            for (PairType& pair : merkleBlock.vMatchedTxn)
                                if (!pfrom->setInventoryKnown.count(CInv(MSG_TX, pair.second)))
                                    pfrom->PushMessage("tx", block.vtx[pair.first]);
                            pfrom->PushMessage("merkleblock", merkleBlock);
                        }
                        // else
                        // no response
                    }

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue) {
                        // ppcoin: send latest proof-of-work block to allow the
                        // download node to accept as orphan (proof-of-stake
                        // block might be rejected by stake connection check)
                        vector<CInv> vInvP;
                        vInvP.push_back(CInv(
                            MSG_BLOCK,
                            GetLastBlockIndex(*txdb.GetBestBlockIndex(), false, txdb).GetBlockHash()));
                        pfrom->PushMessage("inv", vInvP);
                        pfrom->hashContinue = 0;
                    }
                }
            } else if (inv.IsKnownType()) {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                    CTransaction tx;
                    if (mempool.lookup(inv.hash, tx)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                    }
                }
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }

    else if (strCommand == "getblocks") {
        CBlockLocator locator;
        uint256       hashStop;
        vRecv >> locator >> hashStop;

        if (locator.size() > MAX_LOCATOR_SZ) {
            NLog.write(b_sev::err, "locator size {} > {}, disconnect peer={} with addr={}",
                       locator.size(), MAX_LOCATOR_SZ, pfrom->nodeid, pfrom->addr.ToString());
            pfrom->fDisconnect = true;
            return true;
        }

        const CTxDB txdb;

        // Find the last block the caller has in the main chain
        boost::optional<CBlockIndex> pindex = locator.GetBlockIndex(txdb);

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->getNext(txdb);
        int nLimit = 500;
        NLog.write(b_sev::info, "getblocks {} to {} limit {}", (pindex ? pindex->nHeight : -1),
                   hashStop.ToString(), nLimit);
        while (pindex) {
            if (pindex->GetBlockHash() == hashStop) {
                NLog.write(b_sev::info, "  getblocks stopping at {} {}", pindex->nHeight,
                           pindex->GetBlockHash().ToString());
                unsigned int nSMA = Params().StakeMinAge(txdb);
                // ppcoin: tell downloading node about the latest block if it's
                // without risk being rejected due to stake connection check
                uint256 bestBlockHash = txdb.GetBestBlockHash();
                if (hashStop != bestBlockHash &&
                    pindex->GetBlockTime() + nSMA > txdb.GetBestBlockIndex()->GetBlockTime())
                    pfrom->PushInventory(CInv(MSG_BLOCK, bestBlockHash));
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0) {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                NLog.write(b_sev::info, "  getblocks stopping at limit {} {}", pindex->nHeight,
                           pindex->GetBlockHash().ToString());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
            pindex = pindex->getNext(txdb);
        }
    }

    else if (strCommand == "getheaders") {
        CBlockLocator locator;
        uint256       hashStop;
        vRecv >> locator >> hashStop;

        if (locator.size() > MAX_LOCATOR_SZ) {
            NLog.write(b_sev::err, "locator size {} > {}, disconnect peer={} with addr={}",
                       locator.size(), MAX_LOCATOR_SZ, pfrom->nodeid, pfrom->addr.ToString());
            pfrom->fDisconnect = true;
            return true;
        }

        const CTxDB txdb;

        boost::optional<CBlockIndex> pindex = boost::none;
        if (locator.IsNull()) {
            // If locator is null, return the hashStop block
            const auto mi = txdb.ReadBlockIndex(hashStop);
            if (!mi)
                return true;
            pindex = mi;
        } else {
            // Find the last block the caller has in the main chain
            pindex = locator.GetBlockIndex(txdb);
            if (pindex) {
                pindex = pindex->getNext(txdb);
            }
        }

        vector<CBlock> vHeaders;
        int            nLimit = 2000;
        NLog.write(b_sev::info, "getheaders {} to {}", (pindex ? pindex->nHeight : -1),
                   hashStop.ToString());
        while (pindex) {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
            pindex = pindex->getNext(txdb);
        }
        pfrom->PushMessage("headers", vHeaders);
    }

    else if (strCommand == "tx") {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction    tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        const CTxDB txdb;

        const Result<void, TxValidationState> mempoolRes = AcceptToMemoryPool(mempool, tx);
        if (mempoolRes.isOk()) {
            SyncWithWallets(txdb, tx, nullptr);
            RelayTransaction(tx);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (unsigned int i = 0; i < vWorkQueue.size(); i++) {
                uint256 hashPrev = vWorkQueue[i];
                for (set<uint256>::iterator mi = mapOrphanTransactionsByPrev[hashPrev].begin();
                     mi != mapOrphanTransactionsByPrev[hashPrev].end(); ++mi) {
                    const uint256&      orphanTxHash = *mi;
                    const CTransaction& orphanTx     = mapOrphanTransactions[orphanTxHash];

                    const Result<void, TxValidationState> mempoolOrphanRes =
                        AcceptToMemoryPool(mempool, orphanTx);
                    if (mempoolOrphanRes.isOk()) {
                        NLog.write(b_sev::info, "   accepted orphan tx {}", orphanTxHash.ToString());
                        SyncWithWallets(txdb, tx, nullptr);
                        RelayTransaction(orphanTx);
                        mapAlreadyAskedFor.erase(CInv(MSG_TX, orphanTxHash));
                        vWorkQueue.push_back(orphanTxHash);
                        vEraseQueue.push_back(orphanTxHash);
                    } else if (mempoolOrphanRes.unwrapErr(RESULT_PRE).GetResult() !=
                               TxValidationResult::TX_MISSING_INPUTS) {
                        // invalid orphan
                        vEraseQueue.push_back(orphanTxHash);
                        NLog.write(b_sev::info, "   removed invalid orphan tx {}",
                                   orphanTxHash.ToString());
                    }
                }
            }

            for (uint256 hash : vEraseQueue)
                EraseOrphanTx(hash);
        } else if (mempoolRes.unwrapErr(RESULT_PRE).GetResult() ==
                   TxValidationResult::TX_MISSING_INPUTS) {
            AddOrphanTx(tx);

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nMaxOrphanTx = (unsigned int)std::max(
                INT64_C(0), GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
            unsigned int nEvicted = LimitOrphanTxSize(nMaxOrphanTx);
            if (nEvicted > 0)
                NLog.write(b_sev::warn, "mapOrphan overflow, removed {} tx", nEvicted);
        }

        if (tx.reject) {
            pfrom->PushMessage("reject", std::string("tx"), tx.reject->chRejectCode,
                               tx.reject->strRejectReason.substr(0, MAX_REJECT_MESSAGE_LENGTH),
                               tx.reject->hashTx);
        }
        if (tx.nDoS) {
            pfrom->Misbehaving(tx.nDoS);
        }
    }

    else if (strCommand == "block") {
        CBlock block;
        vRecv >> block;
        uint256 hashBlock = block.GetHash();

        NLog.write(b_sev::info, "received block {}", hashBlock.ToString());

        CInv inv(MSG_BLOCK, hashBlock);
        pfrom->AddInventoryKnown(inv);

        if (ProcessBlock(pfrom, &block)) {
            mapAlreadyAskedFor.erase(inv);
        } else if (block.reject) {
            pfrom->PushMessage("reject", std::string("block"), block.reject->chRejectCode,
                               block.reject->strRejectReason, block.reject->hashBlock);
        }

        if (block.nDoS) {
            pfrom->Misbehaving(block.nDoS);
        }
    }

    else if (strCommand == "getaddr") {
        // Don't return addresses older than nCutOff timestamp
        int64_t nCutOff = GetTime() - (nNodeLifespan * 24 * 60 * 60);
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.get().GetAddr();
        for (const CAddress& addr : vAddr)
            if (addr.nTime > nCutOff)
                pfrom->PushAddress(addr);
    }

    else if (strCommand == "mempool") {
        std::vector<uint256> vtxid;
        LOCK2(mempool.cs, pfrom->cs_filter);
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        for (uint256& hash : vtxid) {
            CInv                inv(MSG_TX, hash);
            const CTransaction* txFromMempool = mempool.lookup_unsafe(hash);
            // this tx should exist because we locked then used mempool.queryHashes()
            assert(txFromMempool);
            if ((pfrom->pfilter && pfrom->pfilter->IsRelevantAndUpdate(*txFromMempool)) ||
                (!pfrom->pfilter))
                vInv.push_back(inv);
            if (vInv.size() == MAX_INV_SZ)
                break;
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }

    else if (strCommand == "ping") {
        if (pfrom->nVersion > BIP0031_VERSION) {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }

    else if (strCommand == "alert") {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0) {
            if (alert.ProcessAlert()) {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    for (CNode* pnode : vNodes)
                        alert.RelayTo(pnode);
                }
            } else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                pfrom->Misbehaving(10);
            }
        }
    }

    else if (strCommand == "filterload") {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints())
            // There is no excuse for sending a too-large filter
            pfrom->Misbehaving(100);
        else {
            LOCK(pfrom->cs_filter);
            delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter(filter);
        }
        pfrom->fRelayTxes = true;
    }

    else if (strCommand == "filteradd") {
        vector<unsigned char> vData;
        vRecv >> vData;

        // Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
        // and thus, the maximum size any matched object can have) in a filteradd message
        if (vData.size() > 520) {
            pfrom->Misbehaving(100);
        } else {
            LOCK(pfrom->cs_filter);
            if (pfrom->pfilter)
                pfrom->pfilter->insert(vData);
            else
                pfrom->Misbehaving(100);
        }
    }

    else if (strCommand == "filterclear") {
        LOCK(pfrom->cs_filter);
        delete pfrom->pfilter;
        pfrom->pfilter    = nullptr;
        pfrom->fRelayTxes = true;
    }

    else {
        // Ignore unknown commands for extensibility
    }

    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" ||
            strCommand == "getdata" || strCommand == "ping")
            AddressCurrentlyConnected(pfrom->addr);

    return true;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
    if (fDebug)
        NLog.write(b_sev::debug, "ProcessMessages({} messages)", pfrom->vRecvMsg.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        if (fDebug)
            NLog.write(b_sev::debug, "ProcessMessages(message {} msgsz, {} bytes, complete:{})",
                       msg.hdr.nMessageSize, msg.vRecv.size(), msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, Params().MessageStart(),
                   CMessageHeader::MESSAGE_START_SIZE) != 0) {
            NLog.write(b_sev::err, "PROCESSMESSAGE: INVALID MESSAGESTART");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid(Params().MessageStart())) {
            NLog.write(b_sev::err, "PROCESSMESSAGE: ERRORS IN HEADER {}", hdr.GetCommand());
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv     = msg.vRecv;
        uint256      hash      = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum) {
            NLog.write(
                b_sev::err,
                "ProcessMessages({}, {} bytes) : CHECKSUM ERROR nChecksum={:08x} hdr.nChecksum={:08x}",
                strCommand, nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try {
            {
                LOCK(cs_main);
                fRet = ProcessMessage(pfrom, strCommand, vRecv);
            }
            if (fShutdown)
                break;
        } catch (std::ios_base::failure& e) {
            pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED,
                               std::string("error parsing message"));
            if (strstr(e.what(), "end of data")) {
                // Allow exceptions from under-length message on vRecv
                NLog.write(b_sev::err,
                           "ProcessMessages({}, {} bytes) : Exception '{}' caught, normally caused by a "
                           "message being shorter than its stated length",
                           strCommand, nMessageSize, e.what());
            } else if (strstr(e.what(), "size too large")) {
                // Allow exceptions from over-long size
                NLog.write(b_sev::err, "ProcessMessages({}, {} bytes) : Exception '{}' caught",
                           strCommand, nMessageSize, e.what());
            } else {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        } catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(nullptr, "ProcessMessages()");
        }

        if (!fRet)
            NLog.write(b_sev::err, "ProcessMessage({}, {} bytes) FAILED", strCommand, nMessageSize);
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}

bool SendMessages(CNode* pto, bool fSendTrickle)
{
    TRY_LOCK(cs_main, lockMain);
    if (lockMain) {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        // Keep-alive ping. We send a nonce of zero because we don't use it anywhere
        // right now.
        if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSendMsg.empty()) {
            uint64_t nonce = 0;
            if (pto->nVersion > BIP0031_VERSION)
                pto->PushMessage("ping", nonce);
            else
                pto->PushMessage("ping");
        }

        // Resend wallet transactions that haven't gotten in a block yet
        ResendWalletTransactions();

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload(CTxDB()) && (GetTime() - nLastRebroadcast > 24 * 60 * 60)) {
            {
                LOCK(cs_vNodes);
                for (CNode* pnode : vNodes) {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    if (nLastRebroadcast)
                        pnode->setAddrKnown.clear();

                    // Rebroadcast our address
                    if (!fNoListen) {
                        CAddress addr = GetLocalAddress(&pnode->addr);
                        if (addr.IsRoutable())
                            pnode->PushAddress(addr);
                    }
                }
            }
            nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle) {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            for (const CAddress& addr : pto->vAddrToSend) {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second) {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000) {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }

        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            for (const CInv& inv : pto->vInventoryToSend) {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle) {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint256 hashRand  = inv.hash ^ hashSalt;
                    hashRand          = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    // always trickle our own transactions
                    if (!fTrickleWait) {
                        CWalletTx wtx;
                        if (GetTransaction(inv.hash, wtx))
                            if (wtx.fFromMe)
                                fTrickleWait = true;
                    }

                    if (fTrickleWait) {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second) {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000) {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);

        //
        // Message: getdata
        //
        vector<CInv> vGetData;
        int64_t      nNow = GetTime() * 1000000;
        const CTxDB  txdb;
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow) {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(txdb, inv)) {
                if (fDebugNet)
                    NLog.write(b_sev::debug, "sending getdata: {}", inv.ToString());
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000) {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
                mapAlreadyAskedFor.set(inv, nNow);
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);
    }
    return true;
}

bool EnableEnforceUniqueTokenSymbols(const ITxDB& txdb)
{
    if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, txdb)) {
        return true;
    } else {
        return false;
    }
}

/** Maximum size of a block */
unsigned int MaxBlockSize(const ITxDB& txdb)
{
    if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, txdb)) {
        return MAX_BLOCK_SIZE;
    } else {
        return OLD_MAX_BLOCK_SIZE;
    }
}

/** Minimum Peer Version */
int MinPeerVersion(const ITxDB& txdb)
{
    if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__5_COLD_STAKING, txdb)) {
        return MIN_PEER_PROTO_VERSION;
    } else {
        return OLD_MIN_PEER_PROTO_VERSION;
    }
}

// Amount compression:
// * If the amount is 0, output 0
// * first, divide the amount (in base units) by the largest power of 10 possible; call the exponent e (e
// is max 9)
// * if e<9, the last digit of the resulting number cannot be 0; store it as d, and drop it (divide by
// 10)
//   * call the result n
//   * output 1 + 10*(9*n + d - 1) + e
// * if e==9, we only know the resulting number is not zero, so output 1 + 10*(n - 1) + 9
// (this is decodable, as d is in [1-9] and e is in [0-9])

uint64_t CTxOutCompressor::CompressAmount(uint64_t n)
{
    if (n == 0)
        return 0;
    int e = 0;
    while (((n % 10) == 0) && e < 9) {
        n /= 10;
        e++;
    }
    if (e < 9) {
        int d = (n % 10);
        assert(d >= 1 && d <= 9);
        n /= 10;
        return 1 + (n * 9 + d - 1) * 10 + e;
    } else {
        return 1 + (n - 1) * 10 + 9;
    }
}

uint64_t CTxOutCompressor::DecompressAmount(uint64_t x)
{
    // x = 0  OR  x = 1+10*(9*n + d - 1) + e  OR  x = 1+10*(n - 1) + 9
    if (x == 0)
        return 0;
    x--;
    // x = 10*(9*n + d - 1) + e
    int e = x % 10;
    x /= 10;
    uint64_t n = 0;
    if (e < 9) {
        // x = 9*n + d - 1
        int d = (x % 9) + 1;
        x /= 9;
        // x = n
        n = x * 10 + d;
    } else {
        n = x + 1;
    }
    while (e) {
        n *= 10;
        e--;
    }
    return n;
}

bool IsTxInMainChain(const ITxDB& txdb, const uint256& txHash)
{
    CTransaction tx;
    uint256      hashBlock;
    if (GetTransaction(txHash, tx, hashBlock)) {
        const auto mi = txdb.ReadBlockIndex(hashBlock);
        if (mi) {
            return mi->IsInMainChain(txdb);
        } else {
            throw std::runtime_error("Unable to find the block that has the transaction " +
                                     txHash.ToString());
        }
    }
    throw std::runtime_error("Unable to retrieve the transaction " + txHash.ToString());
}

void ExportBootstrapBlockchain(const filesystem::path& filename, std::atomic<bool>& stopped,
                               std::atomic<double>& progress, boost::promise<void>& result)
{
    RenameThread("Export-blockchain");
    try {
        progress.store(0, std::memory_order_relaxed);

        std::vector<CBlockIndex> chainBlocksIndices;

        const CTxDB txdb;

        {
            boost::optional<CBlockIndex> pblockindex = txdb.GetBestBlockIndex();
            chainBlocksIndices.push_back(*pblockindex);
            while (pblockindex->nHeight > 0 && !stopped.load() && !fShutdown) {
                pblockindex = pblockindex->getPrev(txdb);
                chainBlocksIndices.push_back(*pblockindex);
            }
        }

        if (stopped.load() || fShutdown) {
            throw std::runtime_error("Operation was stopped.");
        }

        boost::filesystem::ofstream outFile(filename, ios::binary);
        if (!outFile.good()) {
            throw std::runtime_error("Failed to open file for writing. Make sure you have sufficient "
                                     "permissions and diskspace.");
        }

        size_t threadsholdSize = 1 << 24; // 4 MB

        CDataStream  serializedBlocks(SER_DISK, CLIENT_VERSION);
        size_t       written = 0;
        const size_t total   = chainBlocksIndices.size();
        for (const CBlockIndex& blockIndex : boost::adaptors::reverse(chainBlocksIndices)) {
            progress.store(static_cast<double>(written) / static_cast<double>(total),
                           std::memory_order_relaxed);
            if (stopped.load() || fShutdown) {
                throw std::runtime_error("Operation was stopped.");
            }
            CBlock block;
            block.ReadFromDisk(&blockIndex, txdb, true);

            // every block starts with pchMessageStart
            unsigned int nSize = block.GetSerializeSize(SER_DISK, CLIENT_VERSION);
            serializedBlocks << FLATDATA(Params().MessageStart()) << nSize;
            serializedBlocks << block;
            if (serializedBlocks.size() > threadsholdSize) {
                outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
                serializedBlocks.clear();
            }
            written++;
        }
        if (serializedBlocks.size() > 0) {
            outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
            serializedBlocks.clear();
            if (!outFile.good()) {
                throw std::runtime_error("An error was raised while writing the file. Make sure you "
                                         "have sufficient permissions and diskspace.");
            }
        }
        progress.store(1, std::memory_order_seq_cst);
        result.set_value();
    } catch (std::exception& ex) {
        result.set_exception(boost::current_exception());
    } catch (...) {
        result.set_exception(boost::current_exception());
    }
}

class BlockIndexTraversorBase
{
    // shared_ptr is necessary because the visitor is passed by value during traversal
    std::shared_ptr<std::deque<uint256>> hashes;

public:
    BlockIndexTraversorBase() { hashes = std::make_shared<std::deque<uint256>>(); }
    template <typename Vertex, typename Graph>
    void discover_vertex(Vertex u, const Graph& g)
    {
        uint256 hash = boost::get(boost::vertex_bundle, g)[u];
        hashes->push_back(hash);
    }

    std::deque<uint256> getTraversedList() const { return *hashes; }
};

class DFSBlockIndexVisitor : public boost::default_dfs_visitor
{
    BlockIndexTraversorBase base;

public:
    template <typename Vertex, typename Graph>
    void discover_vertex(Vertex u, const Graph& g)
    {
        base.discover_vertex(u, g);
    }

    std::deque<uint256> getTraversedList() const { return base.getTraversedList(); }
};

class BFSBlockIndexVisitor : public boost::default_bfs_visitor
{
    BlockIndexTraversorBase base;

public:
    template <typename Vertex, typename Graph>
    void discover_vertex(Vertex u, const Graph& g)
    {
        base.discover_vertex(u, g);
    }

    std::deque<uint256> getTraversedList() { return base.getTraversedList(); }
};

std::pair<BlockIndexGraphType, VerticesDescriptorsMapType> GetBlockIndexAsGraph(const ITxDB& txdb)
{
    BlockIndexGraphType graph;

    // copy block index to avoid conflicts
    const boost::optional<std::map<uint256, CBlockIndex>> tempBlockIndex =
        txdb.ReadAllBlockIndexEntries();
    if (!tempBlockIndex) {
        throw std::runtime_error("Failed to retrieve the block index from the database");
    }

    VerticesDescriptorsMapType verticesDescriptors;

    // add all vertices, which are block hashes
    for (const auto& bi : *tempBlockIndex) {
        verticesDescriptors[bi.first] = boost::add_vertex(bi.first, graph);
    }

    // add edges, which are previous blocks connected to subsequent blocks
    for (const auto& bi : *tempBlockIndex) {
        if (bi.first != Params().GenesisBlockHash()) {
            const CBlockIndex& prev = tempBlockIndex->at(bi.first);
            boost::add_edge(verticesDescriptors.at(prev.blockHash), verticesDescriptors.at(bi.first),
                            graph);
        }
    }
    return std::make_pair(graph, verticesDescriptors);
}

std::deque<uint256> TraverseBlockIndexGraph(const BlockIndexGraphType&        graph,
                                            const VerticesDescriptorsMapType& descriptors,
                                            GraphTraverseType                 traverseType)
{
    uint256 startBlockHash = Params().GenesisBlockHash();

    if (traverseType == GraphTraverseType::DepthFirst) {
        DFSBlockIndexVisitor vis;
        boost::depth_first_search(graph,
                                  boost::visitor(vis).root_vertex(descriptors.at(startBlockHash)));
        return vis.getTraversedList();
    } else if (traverseType == GraphTraverseType::BreadthFirst) {
        BFSBlockIndexVisitor vis;
        boost::breadth_first_search(graph, descriptors.at(startBlockHash), boost::visitor(vis));
        return vis.getTraversedList();
    } else {
        throw std::runtime_error("Unknown graph traversal type");
    }
}

void ExportBootstrapBlockchainWithOrphans(const filesystem::path& filename, std::atomic<bool>& stopped,
                                          std::atomic<double>& progress, boost::promise<void>& result,
                                          GraphTraverseType traverseType)
{
    RenameThread("Export-blockchain");
    try {
        const CTxDB txdb;

        progress.store(0, std::memory_order_relaxed);

        BlockIndexGraphType        graph;
        VerticesDescriptorsMapType verticesDescriptors;
        std::tie(graph, verticesDescriptors) = GetBlockIndexAsGraph(txdb);

        std::deque<uint256> blocksHashes =
            TraverseBlockIndexGraph(graph, verticesDescriptors, traverseType);

        if (stopped.load() || fShutdown) {
            throw std::runtime_error("Operation was stopped.");
        }

        boost::filesystem::ofstream outFile(filename, ios::binary);
        if (!outFile.good()) {
            throw std::runtime_error("Failed to open file for writing. Make sure you have sufficient "
                                     "permissions and diskspace.");
        }

        size_t threadsholdSize = 1 << 24; // 4 MB

        CDataStream          serializedBlocks(SER_DISK, CLIENT_VERSION);
        size_t               written = 0;
        const std::uintmax_t total   = boost::num_vertices(graph);

        for (const uint256& h : blocksHashes) {
            progress.store(static_cast<double>(written) / static_cast<double>(total),
                           std::memory_order_relaxed);
            if (stopped.load() || fShutdown) {
                throw std::runtime_error("Operation was stopped.");
            }
            CBlock block;
            block.ReadFromDisk(h, txdb, true);

            // every block starts with pchMessageStart
            unsigned int nSize = block.GetSerializeSize(SER_DISK, CLIENT_VERSION);
            serializedBlocks << FLATDATA(Params().MessageStart()) << nSize;
            serializedBlocks << block;
            if (serializedBlocks.size() > threadsholdSize) {
                outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
                serializedBlocks.clear();
            }
            written++;
        }
        if (serializedBlocks.size() > 0) {
            outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
            serializedBlocks.clear();
            if (!outFile.good()) {
                throw std::runtime_error("An error was raised while writing the file. Make sure you "
                                         "have sufficient permissions and diskspace.");
            }
        }
        progress.store(1, std::memory_order_seq_cst);
        result.set_value();
    } catch (std::exception& ex) {
        result.set_exception(boost::current_exception());
    } catch (...) {
        result.set_exception(boost::current_exception());
    }
}

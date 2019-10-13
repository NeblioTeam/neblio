// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "alert.h"
#include "checkpoints.h"
#include "db.h"
#include "init.h"
#include "kernel.h"
#include "net.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1/ntp1transaction.h"
#include "txdb.h"
#include "ui_interface.h"
#include "zerocoin/Zerocoin.h"
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

std::set<uint256> UnrecoverableNTP1Txs;

CCriticalSection              cs_setpwalletRegistered;
set<std::shared_ptr<CWallet>> setpwalletRegistered;

CCriticalSection cs_main;

CTxMemPool              mempool;
boost::atomic<uint32_t> nTransactionsUpdated{0};

// THERE IS ANOTHER ONE OF THOSE IN NTP1Transaction, change that if you wanna change this until this is
// fixed and there's only one variable
const std::string  NTP1OpReturnRegexStr = R"(^OP_RETURN\s+(4e54(?:01|03)[a-fA-F0-9]*)$)";
const boost::regex NTP1OpReturnRegex(NTP1OpReturnRegexStr);
const std::string  OpReturnRegexStr = R"(^OP_RETURN\s+(.*)$)";
const boost::regex OpReturnRegex(OpReturnRegexStr);

BlockIndexMapType                  mapBlockIndex;
set<pair<COutPoint, unsigned int>> setStakeSeen;
libzerocoin::Params*               ZCParams;

// Set PoW difficulty to easiest
CBigNum bnProofOfWorkLimit(~uint256(0) >> 1);
CBigNum bnProofOfWorkLimitTestNet(~uint256(0) >> 1);
// Set PoS difficulty to standard
CBigNum bnProofOfStakeLimit(~uint256(0) >> 20);

unsigned int nTargetSpacing         = 30;           // Block spacing 30 seconds
unsigned int nOldTargetSpacing      = 2 * 60;       // Old Block spacing 2 minutes
unsigned int nStakeMinAge           = 24 * 60 * 60; // Minimum stake age
unsigned int nOldTestnetStakeMinAge = 60;           // Minimum stake age on testnet before the hard fork
unsigned int nStakeMaxAge           = 7 * 24 * 60 * 60; // Maximum stake age 7 days
unsigned int nModifierInterval      = 10 * 60;          // time to elapse before new modifier is computed

// static const int64_t nTargetTimespan = 16 * 60;  // 16 mins
static const int64_t nTargetTimespan = 2 * 60 * 60; // 2 hours

int                 nCoinbaseMaturity    = 120; // Coin Base Maturity
int                 nOldCoinbaseMaturity = 30;  // Old Coin Base Maturity
CBlockIndexSmartPtr pindexGenesisBlock   = nullptr;
boost::atomic<int>  nBestHeight{-1};

uint256 nBestChainTrust   = 0;
uint256 nBestInvalidTrust = 0;

uint256             hashBestChain = 0;
CBlockIndexSmartPtr pindexBest{nullptr};
int64_t             nTimeBestReceived = 0;
boost::atomic<bool> fImporting{false};

CMedianFilter<int> cPeerBlockCounts(5, 0); // Amount of blocks that other nodes claim to have

std::unordered_map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*>           mapOrphanBlocksByPrev;
set<pair<COutPoint, unsigned int>>   setStakeSeenOrphan;

map<uint256, CTransaction> mapOrphanTransactions;
map<uint256, set<uint256>> mapOrphanTransactionsByPrev;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "Neblio Signed Message:\n";

// Settings
int64_t nTransactionFee    = MIN_TX_FEE;
int64_t nReserveBalance    = 0;
int64_t nMinimumInputValue = 0;

extern enum Checkpoints::CPMode CheckpointsMode;

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
bool static IsFromMe(CTransaction& tx)
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
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fConnect)
{
    // update NTP1 transactions
    if (pwalletMain->walletNewTxUpdateFunctor) {
        pwalletMain->walletNewTxUpdateFunctor->run(tx.GetHash(), nBestHeight);
    }

    if (!fConnect) {
        // ppcoin: wallets need to refund inputs when disconnecting coinstake
        if (tx.IsCoinStake()) {
            for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
                if (pwallet->IsFromMe(tx))
                    pwallet->DisableTransaction(tx);
        }
        return;
    }

    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->AddToWalletIfInvolvingMe(tx, pblock, fUpdate);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
{
    for (const std::shared_ptr<CWallet>& pwallet : setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

// notify wallets about an updated transaction
void static UpdatedTransaction(const uint256& hashTx)
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
        pwallet->ResendWalletTransactions(fForce);
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
        printf("ignoring large orphan tx (size: %" PRIszu ", hash: %s)\n", nSize,
               hash.ToString().c_str());
        return false;
    }

    mapOrphanTransactions[hash] = tx;
    for (const CTxIn& txin : tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    printf("stored orphan tx %s (mapsz %" PRIszu ")\n", hash.ToString().substr(0, 10).c_str(),
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

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
{
    SetNull();
    if (!txdb.ReadTxIndex(prevout.hash, txindexRet))
        return false;
    if (!txdb.ReadTx(txindexRet.pos, *this))
        return false;
    if (prevout.n >= vout.size()) {
        SetNull();
        return false;
    }
    return true;
}

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout)
{
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool IsStandardTx(const CTransaction& tx, string& reason)
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
    if (!IsFinalTx(tx, nBestHeight + 1)) {
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
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
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

bool IsFinalTx(const CTransaction& tx, int nBlockHeight, int64_t nBlockTime)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = nBestHeight;
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

//
// Check transaction inputs, and make sure any
// pay-to-script-hash transactions are evaluating IsStandard scripts
//
// Why bother? To avoid denial-of-service attacks; an attacker
// can submit a standard HASH... OP_EQUAL transaction,
// which will get accepted into blocks. The redemption
// script can be anything; an attacker could use a very
// expensive-to-check-upon-redemption script like:
//   DUP CHECKSIG DROP ... repeated 100 times... OP_1
//
bool CTransaction::AreInputsStandard(const MapPrevTx& mapInputs) const
{
    if (IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < vin.size(); i++) {
        const CTxOut& prev = GetOutputFor(vin[i], mapInputs);

        vector<vector<unsigned char>> vSolutions;
        txnouttype                    whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig the
        // IsStandard() call returns false
        vector<vector<unsigned char>> stack;
        if (!EvalScript(stack, vin[i].scriptSig, *this, i, false, 0))
            return false;

        if (whichType == TX_SCRIPTHASH) {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end()); // Get the redeemScript
            // Removed the check to make sure the redeemScript subscript fits one of the four standard
            // transaction types Instead, make sure that the redeemScript doesn't have too many signature
            // check Ops
            if (subscript.GetSigOpCount(true) > MAX_P2SH_SIGOPS) {
                return false;
            }
        } else {
            // Not a TX_SCRIPTHASH scriptPubKey
            int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
            if (nArgsExpected < 0)
                return false;
            // If stack is different than expected, not standard
            if (stack.size() != (unsigned int)nArgsExpected)
                return false;
        }
    }

    return true;
}

unsigned int CTransaction::GetLegacySigOpCount() const
{
    unsigned int nSigOps = 0;
    for (const CTxIn& txin : vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const CTxOut& txout : vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

int CMerkleTx::SetMerkleBranch(const CBlock* pblock)
{
    AssertLockHeld(cs_main);

    CBlock blockTmp;
    if (pblock == NULL) {
        // Load the block this tx is in
        CTxIndex txindex;
        if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
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
    BlockIndexMapType::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    return boost::atomic_load(&pindexBest)->nHeight - pindex->nHeight + 1;
}

bool CTransaction::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vin empty"));
    if (vout.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vout empty"));
    // Size limits
    unsigned int nSizeLimit = MaxBlockSize();
    if (::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > nSizeLimit)
        return DoS(100, error("CTransaction::CheckTransaction() : size limits failed"));

    // Check for negative or overflow output values
    int64_t nValueOut = 0;
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];
        if (txout.IsEmpty() && !IsCoinBase() && !IsCoinStake())
            return DoS(100,
                       error("CTransaction::CheckTransaction() : txout empty for user transaction"));
        if (txout.nValue < 0)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue negative"));
        if (txout.nValue > MAX_MONEY)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue too high"));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return DoS(100, error("CTransaction::CheckTransaction() : txout total out of range"));
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    for (const CTxIn& txin : vin) {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase()) {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return DoS(100, error("CTransaction::CheckTransaction() : coinbase script size is invalid"));
    } else {
        for (const CTxIn& txin : vin)
            if (txin.prevout.IsNull())
                return DoS(10, error("CTransaction::CheckTransaction() : prevout is null"));
    }

    return true;
}

int64_t CTransaction::GetMinFee(unsigned int nBlockSize, enum GetMinFee_mode mode,
                                unsigned int nBytes) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

    unsigned int nNewBlockSize = nBlockSize + nBytes;
    int64_t      nMinFee       = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee) {
        for (const CTxOut& txout : vout)
            if (txout.nValue < CENT)
                nMinFee = nBaseFee;
    }

    // Raise the price as the block approaches full
    unsigned int nSizeLimit = MaxBlockSize();
    if (nBlockSize != 1 && nNewBlockSize >= nSizeLimit / 2) {
        if (nNewBlockSize >= nSizeLimit)
            return MAX_MONEY;
        nMinFee *= nSizeLimit / (nSizeLimit - nNewBlockSize);
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

bool CTransaction::ReadFromDisk(CDiskTxPos pos, CTxDB& txdb) { return txdb.ReadTx(pos, *this); }

bool IsIssuedTokenBlacklisted(std::pair<CTransaction, NTP1Transaction>& txPair)
{
    const auto& prevout0      = txPair.first.vin[0].prevout;
    std::string storedTokenId = txPair.second.getTokenIdIfIssuance(prevout0.hash.ToString(), prevout0.n);
    return IsNTP1TokenBlacklisted(storedTokenId);
}

void AssertNTP1TokenNameIsNotAlreadyInMainChain(std::string sym, const uint256& txHash, CTxDB& txdb)
{
    // make sure that case doesn't matter by converting to upper case
    std::transform(sym.begin(), sym.end(), sym.begin(), ::toupper);
    std::vector<uint256> storedSymbolsTxHashes;
    if (txdb.ReadNTP1TxsWithTokenSymbol(sym, storedSymbolsTxHashes)) {
        for (const uint256& h : storedSymbolsTxHashes) {
            if (!IsTxInMainChain(h)) {
                continue;
            }
            auto pair = std::make_pair(FetchTxFromDisk(h), NTP1Transaction());
            FetchNTP1TxFromDisk(pair, txdb, false);
            std::string storedSymbol = pair.second.getTokenSymbolIfIssuance();
            // blacklisted tokens can be duplicated, since they won't be used ever again
            if (IsIssuedTokenBlacklisted(pair)) {
                continue;
            }
            // make sure that case doesn't matter by converting to upper case
            std::transform(storedSymbol.begin(), storedSymbol.end(), storedSymbol.begin(), ::toupper);
            if (sym == storedSymbol && txHash != h) {
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

void AssertNTP1TokenNameIsNotAlreadyInMainChain(const NTP1Transaction& ntp1tx, CTxDB& txdb)
{
    if (ntp1tx.getTxType() == NTP1TxType_ISSUANCE) {
        std::string sym = ntp1tx.getTokenSymbolIfIssuance();
        AssertNTP1TokenNameIsNotAlreadyInMainChain(sym, ntp1tx.getTxHash(), txdb);
    } else if (ntp1tx.getTxType() == NTP1TxType_UNKNOWN) {
        throw std::runtime_error("Attempted to " + std::string(__func__) +
                                 " on an uninitialized NTP1 transaction");
    }
}

bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction& tx, bool* pfMissingInputs)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!tx.CheckTransaction())
        return error("AcceptToMemoryPool : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return tx.DoS(100, error("AcceptToMemoryPool : coinbase as individual tx"));

    // ppcoin: coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return tx.DoS(100, error("AcceptToMemoryPool : coinstake as individual tx"));

    // Rather not work on nonstandard transactions (unless -testnet)
    string reason;
    if (!fTestNet && !IsStandardTx(tx, reason))
        return error("AcceptToMemoryPool : nonstandard transaction: %s", reason.c_str());

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = NULL;
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint)) {
                // Disable replacement feature for now
                return false;

                // Allow replacing with a newer version of the same transaction
                if (i != 0)
                    return false;
                ptxOld = pool.mapNextTx[outpoint].ptx;
                if (IsFinalTx(*ptxOld))
                    return false;
                if (!tx.IsNewerThan(*ptxOld))
                    return false;
                for (unsigned int i = 0; i < tx.vin.size(); i++) {
                    COutPoint outpoint = tx.vin[i].prevout;
                    if (!pool.mapNextTx.count(outpoint) || pool.mapNextTx[outpoint].ptx != ptxOld)
                        return false;
                }
                break;
            }
        }
    }

    {
        CTxDB txdb;

        // do we already have it?
        if (txdb.ContainsTx(hash))
            return false;

        MapPrevTx                                                           mapInputs;
        map<uint256, CTxIndex>                                              mapUnused;
        map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>> mapUnused2;
        bool                                                                fInvalid = false;
        if (!tx.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid)) {
            if (fInvalid)
                return error("AcceptToMemoryPool : FetchInputs found invalid tx %s",
                             hash.ToString().substr(0, 10).c_str());
            if (pfMissingInputs)
                *pfMissingInputs = true;
            return false;
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (!tx.AreInputsStandard(mapInputs) && !fTestNet)
            return error("AcceptToMemoryPool : nonstandard transaction input");

        // Note: if you modify this code to accept non-standard transactions, then
        // you should add code here to check that the transaction does a
        // reasonable number of ECDSA signature verifications.

        int64_t      nFees = tx.GetValueIn(mapInputs) - tx.GetValueOut();
        unsigned int nSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

        // Don't accept it if it can't get into a block
        int64_t txMinFee = tx.GetMinFee(1000, GMF_RELAY, nSize);
        if (nFees < txMinFee)
            return error("AcceptToMemoryPool : not enough fees %s, %" PRId64 " < %" PRId64,
                         hash.ToString().c_str(), nFees, txMinFee);

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
                    return error("AcceptToMemoryPool : free transaction rejected by rate limiter");
                if (fDebug)
                    printf("Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount + nSize);
                dFreeCount += nSize;
            }
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!tx.ConnectInputs(txdb, mapInputs, mapUnused, CDiskTxPos(1, 1),
                              boost::atomic_load(&pindexBest), false, false)) {
            return error("AcceptToMemoryPool : ConnectInputs failed %s",
                         hash.ToString().substr(0, 10).c_str());
        }

        if (PassedFirstValidNTP1Tx(nBestHeight, fTestNet) &&
            GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
            try {
                std::vector<std::pair<CTransaction, NTP1Transaction>> inputsTxs =
                    StdFetchedInputTxsToNTP1(tx, mapInputs, txdb, false, mapUnused2, mapUnused);
                NTP1Transaction ntp1tx;
                ntp1tx.readNTP1DataFromTx(tx, inputsTxs);
                if (EnableEnforceUniqueTokenSymbols()) {
                    AssertNTP1TokenNameIsNotAlreadyInMainChain(ntp1tx, txdb);
                }
            } catch (std::exception& ex) {
                printf("AcceptToMemoryPool: An invalid NTP1 transaction was submitted to the memory "
                       "pool; an exception was "
                       "thrown: %s\n",
                       ex.what());
                return false;
            } catch (...) {
                printf("AcceptToMemoryPool: An invalid NTP1 transaction was submitted to the memory "
                       "pool; an unknown "
                       "exception was "
                       "thrown.");
                return false;
            }
        }
    }

    // Store transaction in memory
    {
        LOCK(pool.cs);
        if (ptxOld) {
            printf("AcceptToMemoryPool : replacing tx %s with new version\n",
                   ptxOld->GetHash().ToString().c_str());
            pool.remove(*ptxOld);
        }
        pool.addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());

    printf("AcceptToMemoryPool : accepted %s (poolsz %" PRIszu ")\n",
           hash.ToString().substr(0, 10).c_str(), pool.mapTx.size());

    return true;
}

bool CTxMemPool::addUnchecked(const uint256& hash, CTransaction& tx)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptToMemoryPool to properly check the transaction first.
    {
        mapTx[hash] = tx;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
            mapNextTx[tx.vin[i].prevout] = CInPoint(&mapTx[hash], i);
        nTransactionsUpdated++;
    }
    return true;
}

bool CTxMemPool::remove(const CTransaction& tx, bool fRecursive)
{
    // Remove transaction from memory pool
    {
        LOCK(cs);
        uint256 hash = tx.GetHash();
        if (mapTx.count(hash)) {
            if (fRecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                        remove(*it->second.ptx, true);
                }
            }
            for (const CTxIn& txin : tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
            nTransactionsUpdated++;
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction& tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    for (const CTxIn& txin : tx.vin) {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction& txConflict = *it->second.ptx;
            if (txConflict != tx)
                remove(txConflict, true);
        }
    }
    return true;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
    ++nTransactionsUpdated;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (map<uint256, CTransaction>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back((*mi).first);
}

int CMerkleTx::GetDepthInMainChainINTERNAL(CBlockIndex*& pindexRet) const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;
    AssertLockHeld(cs_main);

    // Find the block it claims to be in
    BlockIndexMapType::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    // Make sure the merkle branch connects to this block
    if (!fMerkleVerified) {
        if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
            return 0;
        fMerkleVerified = true;
    }

    pindexRet = pindex.get();
    return boost::atomic_load(&pindexBest)->nHeight - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(CBlockIndex*& pindexRet) const
{
    AssertLockHeld(cs_main);
    int nResult = GetDepthInMainChainINTERNAL(pindexRet);
    if (nResult == 0 && !mempool.exists(GetHash()))
        return -1; // Not in chain, not in mempool

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    int nCbM = CoinbaseMaturity();
    return max(0, (nCbM + 0) - GetDepthInMainChain());
}

bool CMerkleTx::AcceptToMemoryPool() { return ::AcceptToMemoryPool(mempool, *this, NULL); }

bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb)
{

    {
        // Add previous supporting transactions first
        for (CMerkleTx& tx : vtxPrev) {
            if (!(tx.IsCoinBase() || tx.IsCoinStake())) {
                uint256 hash = tx.GetHash();
                if (!mempool.exists(hash) && !txdb.ContainsTx(hash))
                    tx.AcceptToMemoryPool();
            }
        }
        return AcceptToMemoryPool();
    }
    return false;
}

bool CWalletTx::AcceptWalletTransaction()
{
    CTxDB txdb("r");
    return AcceptWalletTransaction(txdb);
}

int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    BlockIndexMapType::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}

// Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock
bool GetTransaction(const uint256& hash, CTransaction& tx, uint256& hashBlock)
{
    {
        LOCK(cs_main);
        {
            if (mempool.lookup(hash, tx)) {
                return true;
            }
        }
        CTxDB    txdb("r");
        CTxIndex txindex;
        if (tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex)) {
            CBlock block;
            if (block.ReadFromDisk(txindex.pos.nBlockPos, false))
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

static CBlockIndexSmartPtr pblockindexFBBHLast;
CBlockIndexSmartPtr        FindBlockByHeight(int nHeight)
{
    CBlockIndexSmartPtr pblockindex;
    if (nHeight < nBestHeight / 2) {
        pblockindex = boost::atomic_load(&pindexGenesisBlock);
    } else {
        pblockindex = boost::atomic_load(&pindexBest);
    }
    if (pblockindexFBBHLast &&
        abs(nHeight - pblockindex->nHeight) > abs(nHeight - pblockindexFBBHLast->nHeight)) {
        pblockindex = pblockindexFBBHLast;
    }
    while (pblockindex->nHeight > nHeight) {
        pblockindex = pblockindex->pprev;
    }
    while (pblockindex->nHeight < nHeight) {
        pblockindex = pblockindex->pnext;
    }
    pblockindexFBBHLast = pblockindex;
    return pblockindex;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
    if (!fReadTransactions) {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->blockKeyInDB, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, CTxDB& txdb, bool fReadTransactions)
{
    if (!fReadTransactions) {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->blockKeyInDB, txdb, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

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

// miner's coin base reward
int64_t GetProofOfWorkReward(int64_t nFees)
{
    // Miner reward: 2000 coin for 500 Blocks = 1,000,000 coin
    int64_t nSubsidy = 2000 * COIN;

    if (nBestHeight == 0) {
        // Total premine coin, after the first 501 blocks are mined there will be a total of 125,000,000
        nSubsidy = 124000000 * COIN;
    }

    // 0 reward for PoW blocks after 500
    if (nBestHeight > 500) {
        nSubsidy = 0;
    }

    if (fDebug)
        printf("GetProofOfWorkReward() : create=%s nSubsidy=%" PRId64 "\n",
               FormatMoney(nSubsidy).c_str(), nSubsidy);

    return nSubsidy + nFees;
}

// miner's coin stake reward based on coin age spent (coin-days)
int64_t GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees)
{
    // CBlockLocator locator;

    int64_t nRewardCoinYear = COIN_YEAR_REWARD; // 10% reward up to end

    printf("Block Number %d \n", nBestHeight.load());

    int64_t nSubsidy = nCoinAge * nRewardCoinYear * 33 / (365 * 33 + 8);
    printf("coin-Subsidy %" PRId64 "\n", nSubsidy);
    printf("coin-Age %" PRId64 "\n", nCoinAge);
    printf("Coin Reward %" PRId64 "\n", nRewardCoinYear);
    if (fDebug)
        printf("GetProofOfStakeReward(): create=%s nCoinAge=%" PRId64 "\n",
               FormatMoney(nSubsidy).c_str(), nCoinAge);

    return nSubsidy + nFees;
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
    return ComputeMaxBits(bnProofOfWorkLimit, nBase, nTime);
}

//
// minimum amount of stake that could possibly be required nTime after
// minimum proof-of-stake required was nBase
//
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int /*nBlockTime*/)
{
    return ComputeMaxBits(bnProofOfStakeLimit, nBase, nTime);
}

// ppcoin: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = boost::atomic_load(&pindex->pprev).get();
    return pindex;
}

static unsigned int GetNextTargetRequiredV1(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? bnProofOfStakeLimit : bnProofOfWorkLimit;

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev =
        GetLastBlockIndex(boost::atomic_load(&pindexPrev->pprev).get(), fProofOfStake);
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    unsigned int nTS       = TargetSpacing();
    int64_t      nInterval = nTargetTimespan / nTS;
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
int64_t CalculateActualBlockSpacingForV3(const CBlockIndex* pindexLast)
{
    // get the latest blocks from the blocks. The amount of blocks is: TARGET_AVERAGE_BLOCK_COUNT
    int64_t forkBlock = GetNetForks().getFirstBlockOfFork(NetworkFork::NETFORK__4_RETARGET_CORRECTION);
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
    const CBlockIndex* currIndex = pindexLast;
    blockTimes.resize(numOfBlocksToAverage);
    for (int64_t i = 0; i < numOfBlocksToAverage; i++) {
        // fill the blocks in reverse order
        blockTimes.at(numOfBlocksToAverage - i - 1) = currIndex->GetBlockTime();
        // move to the previous block
        currIndex = boost::atomic_load(&currIndex->pprev).get();
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

static unsigned int GetNextTargetRequiredV2(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? bnProofOfStakeLimit : bnProofOfWorkLimit;

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev =
        GetLastBlockIndex(boost::atomic_load(&pindexPrev->pprev).get(), fProofOfStake);
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    int64_t      nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    unsigned int nTS            = TargetSpacing();
    if (nActualSpacing < 0)
        nActualSpacing = nTS;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    int64_t nInterval = nTargetTimespan / nTS;
    bnNew *= ((nInterval - 1) * nTS + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTS);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

static unsigned int GetNextTargetRequiredV3(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? bnProofOfStakeLimit : bnProofOfWorkLimit;

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev =
        GetLastBlockIndex(boost::atomic_load(&pindexPrev->pprev).get(), fProofOfStake);
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = CalculateActualBlockSpacingForV3(pindexLast);

    const unsigned int nTS = TargetSpacing();
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
    assert(nTS == 30);
    assert(nTargetTimespan == 2 * 60 * 60);

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum newTarget;
    newTarget.SetCompact(pindexPrev->nBits); // target from previous block
    int64_t nInterval = nTargetTimespan / nTS;

    static constexpr const int k = 15;
    static constexpr const int l = 7;
    static constexpr const int m = 90;
    newTarget *= (nInterval - l + k) * nTS + (m + l) * nActualSpacing;
    newTarget /= (nInterval + k) * nTS + m * nActualSpacing;

    if (newTarget <= 0 || newTarget > bnTargetLimit)
        newTarget = bnTargetLimit;

    return newTarget.GetCompact();
}

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    if (pindexLast->nHeight < 2000)
        return GetNextTargetRequiredV1(pindexLast, fProofOfStake);
    else if (GetNetForks().isForkActivated(NetworkFork::NETFORK__4_RETARGET_CORRECTION))
        return GetNextTargetRequiredV3(pindexLast, fProofOfStake);
    else
        return GetNextTargetRequiredV2(pindexLast, fProofOfStake);
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > bnProofOfWorkLimit)
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    return std::max(cPeerBlockCounts.median(), Checkpoints::GetTotalBlocksEstimate());
}

// DO NOT call this function it's NOT thread-safe. Use IsInitialBlockDownload or
// IsInitialBlockDownload_tolerant
bool __IsInitialBlockDownload_internal()
{
    if (pindexBest == NULL || nBestHeight < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static int64_t             nLastUpdate;
    static CBlockIndexSmartPtr pindexLastBest;
    CBlockIndexSmartPtr        pindexBestPtr = boost::atomic_load(&pindexBest);
    if (pindexBestPtr != pindexLastBest) {
        pindexLastBest = pindexBestPtr;
        nLastUpdate    = GetTime();
    }
    return (GetTime() - nLastUpdate < 15 && pindexBestPtr->GetBlockTime() < GetTime() - 8 * 60 * 60);
}
bool IsInitialBlockDownload_tolerant()
{
    // will try to lock. If failed, will return false
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain) {
        return false;
    }
    return __IsInitialBlockDownload_internal();
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    return __IsInitialBlockDownload_internal();
}

void static InvalidChainFound(const CBlockIndexSmartPtr& pindexNew, CTxDB& txdb)
{
    if (pindexNew->nChainTrust > nBestInvalidTrust) {
        nBestInvalidTrust = pindexNew->nChainTrust;
        txdb.WriteBestInvalidTrust(CBigNum(nBestInvalidTrust));
        uiInterface.NotifyBlocksChanged();
    }

    uint256 nBestInvalidBlockTrust = pindexNew->nChainTrust - pindexNew->pprev->nChainTrust;

    CBlockIndexSmartPtr pindexBestPtr = boost::atomic_load(&pindexBest);

    uint256 nBestBlockTrust = pindexBestPtr->nHeight != 0
                                  ? (pindexBestPtr->nChainTrust - pindexBestPtr->pprev->nChainTrust)
                                  : pindexBestPtr->nChainTrust;

    printf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
           pindexNew->GetBlockHash().ToString().c_str(), pindexNew->nHeight,
           CBigNum(pindexNew->nChainTrust).ToString().c_str(), nBestInvalidBlockTrust.Get64(),
           DateTimeStrFormat("%x %H:%M:%S", pindexNew->GetBlockTime()).c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
           hashBestChain.ToString().c_str(), nBestHeight.load(),
           CBigNum(pindexBestPtr->nChainTrust).ToString().c_str(), nBestBlockTrust.Get64(),
           DateTimeStrFormat("%x %H:%M:%S", pindexBestPtr->GetBlockTime()).c_str());
}

void CBlock::UpdateTime(const CBlockIndex* /*pindexPrev*/)
{
    nTime = max(GetBlockTime(), GetAdjustedTime());
}

bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!IsCoinBase()) {
        for (const CTxIn& txin : vin) {
            COutPoint prevout = txin.prevout;

            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            if (!txdb.UpdateTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : UpdateTxIndex failed");
        }
    }

    // Remove transaction from index
    // This can fail if a duplicate of this transaction was in a chain that got
    // reorganized away. This is only possible if this transaction was completely
    // spent, so erasing it would be a no-op anyway.
    txdb.EraseTxIndex(*this);

    return true;
}

bool CTransaction::FetchInputs(CTxDB& txdb, const map<uint256, CTxIndex>& mapTestPool, bool fBlock,
                               bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
    // FetchInputs can return false either because we just haven't seen some inputs
    // (in which case the transaction should be stored as an orphan)
    // or because the transaction is malformed (in which case the transaction should
    // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
    fInvalid = false;

    if (IsCoinBase())
        return true; // Coinbase transactions have no inputs to fetch.

    for (unsigned int i = 0; i < vin.size(); i++) {
        COutPoint prevout = vin[i].prevout;
        if (inputsRet.count(prevout.hash))
            continue; // Got it already

        // Read txindex
        CTxIndex& txindex = inputsRet[prevout.hash].first;
        bool      fFound  = true;
        if ((fBlock || fMiner) && mapTestPool.count(prevout.hash)) {
            // Get txindex from current proposed changes
            txindex = mapTestPool.find(prevout.hash)->second;
        } else {
            // Read txindex from txdb
            fFound = txdb.ReadTxIndex(prevout.hash, txindex);
        }
        if (!fFound && (fBlock || fMiner))
            return fMiner ? false
                          : error("FetchInputs() : %s prev tx %s index entry not found",
                                  GetHash().ToString().c_str(), prevout.hash.ToString().c_str());

        // Read txPrev
        CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (!fFound || txindex.pos == CDiskTxPos(1, 1)) {
            // Get prev tx from single transactions in memory
            if (!mempool.lookup(prevout.hash, txPrev))
                return error("FetchInputs() : %s mempool Tx prev not found %s",
                             GetHash().ToString().c_str(), prevout.hash.ToString().c_str());
            if (!fFound)
                txindex.vSpent.resize(txPrev.vout.size());
        } else {
            // Get prev tx from disk
            if (!txPrev.ReadFromDisk(txindex.pos, txdb))
                return error("FetchInputs() : %s ReadFromDisk prev tx %s failed",
                             GetHash().ToString().c_str(), prevout.hash.ToString().c_str());
        }
    }

    // Make sure all prevout.n indexes are valid:
    for (unsigned int i = 0; i < vin.size(); i++) {
        const COutPoint prevout = vin[i].prevout;
        assert(inputsRet.count(prevout.hash) != 0);
        const CTxIndex&     txindex = inputsRet[prevout.hash].first;
        const CTransaction& txPrev  = inputsRet[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size()) {
            // Revisit this if/when transaction replacement is implemented and allows
            // adding inputs:
            fInvalid = true;
            return DoS(100, error("FetchInputs() : %s prevout.n out of range %d %" PRIszu " %" PRIszu
                                  " prev tx %s\n%s",
                                  GetHash().ToString().c_str(), prevout.n, txPrev.vout.size(),
                                  txindex.vSpent.size(), prevout.hash.ToString().c_str(),
                                  txPrev.ToString().c_str()));
        }
    }

    return true;
}

const CTxOut& CTransaction::GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const
{
    MapPrevTx::const_iterator mi = inputs.find(input.prevout.hash);
    if (mi == inputs.end())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.hash not found");

    const CTransaction& txPrev = (mi->second).second;
    if (input.prevout.n >= txPrev.vout.size())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.n out of range");

    return txPrev.vout[input.prevout.n];
}

int64_t CTransaction::GetValueIn(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    int64_t nResult = 0;
    for (unsigned int i = 0; i < vin.size(); i++) {
        nResult += GetOutputFor(vin[i], inputs).nValue;
    }
    return nResult;
}

unsigned int CTransaction::GetP2SHSigOpCount(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < vin.size(); i++) {
        const CTxOut& prevout = GetOutputFor(vin[i], inputs);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(vin[i].scriptSig);
    }
    return nSigOps;
}

bool CTransaction::ConnectInputs(CTxDB& /*txdb*/, MapPrevTx inputs, map<uint256, CTxIndex>& mapTestPool,
                                 const CDiskTxPos&               posThisTx,
                                 const ConstCBlockIndexSmartPtr& pindexBlock, bool fBlock, bool fMiner)
{
    // Take over previous transactions' spent pointers
    // fBlock is true when this is called from AcceptBlock when a new best-block is added to the
    // blockchain fMiner is true when called from the internal bitcoin miner
    // ... both are false when called from CTransaction::AcceptToMemoryPool
    if (!IsCoinBase()) {
        int64_t nValueIn = 0;
        int64_t nFees    = 0;
        for (unsigned int i = 0; i < vin.size(); i++) {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex&     txindex = inputs[prevout.hash].first;
            CTransaction& txPrev  = inputs[prevout.hash].second;

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
                return DoS(100, error("ConnectInputs() : %s prevout.n out of range %d %" PRIszu
                                      " %" PRIszu " prev tx %s\n%s",
                                      GetHash().ToString().c_str(), prevout.n, txPrev.vout.size(),
                                      txindex.vSpent.size(), prevout.hash.ToString().c_str(),
                                      txPrev.ToString().c_str()));

            // If prev is coinbase or coinstake, check that it's matured
            int nCbM = CoinbaseMaturity();
            if (txPrev.IsCoinBase() || txPrev.IsCoinStake())
                for (ConstCBlockIndexSmartPtr pindex = boost::atomic_load(&pindexBlock);
                     pindex && pindexBlock->nHeight - pindex->nHeight < nCbM;
                     pindex = boost::atomic_load(&pindex->pprev)) {
                    static_assert(std::is_same<decltype(pindex->blockKeyInDB),
                                               decltype(txindex.pos.nBlockPos)>::value,
                                  "Expected same types");
                    if (pindex->blockKeyInDB == txindex.pos.nBlockPos) {
                        return error("ConnectInputs() : tried to spend %s at depth %d",
                                     txPrev.IsCoinBase() ? "coinbase" : "coinstake",
                                     pindexBlock->nHeight - pindex->nHeight);
                    }
                }

            // ppcoin: check transaction timestamp
            if (txPrev.nTime > nTime)
                return DoS(
                    100,
                    error("ConnectInputs() : transaction timestamp earlier than input transaction"));

            // Check for negative or overflow input values
            nValueIn += txPrev.vout[prevout.n].nValue;
            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return DoS(100, error("ConnectInputs() : txin values out of range"));
        }
        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.
        for (unsigned int i = 0; i < vin.size(); i++) {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex&     txindex = inputs[prevout.hash].first;
            CTransaction& txPrev  = inputs[prevout.hash].second;

            // Check for conflicts (double-spend)
            // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
            // for an attacker to attempt to split the network.
            if (!txindex.vSpent[prevout.n].IsNull())
                return fMiner ? false
                              : error("ConnectInputs() : %s prev tx already used at %s",
                                      GetHash().ToString().c_str(),
                                      txindex.vSpent[prevout.n].ToString().c_str());

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.
            if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate()))) {
                // Verify signature
                bool fStrictPayToScriptHash = true;
                if (!VerifySignature(txPrev, *this, i, fStrictPayToScriptHash, false, 0)) {
                    // only during transition phase for P2SH: do not invoke anti-DoS code for
                    // potentially old clients relaying bad P2SH transactions
                    if (fStrictPayToScriptHash && VerifySignature(txPrev, *this, i, false, false, 0))
                        return error("ConnectInputs() : %s P2SH VerifySignature failed",
                                     GetHash().ToString().c_str());

                    return DoS(100, error("ConnectInputs() : %s VerifySignature failed",
                                          GetHash().ToString().c_str()));
                }
            }

            // Mark outpoints as spent
            txindex.vSpent[prevout.n] = posThisTx;

            // Write back
            if (fBlock || fMiner) {
                mapTestPool[prevout.hash] = txindex;
            }
        }

        if (!IsCoinStake()) {
            if (nValueIn < GetValueOut())
                return DoS(100, error("ConnectInputs() : %s value in < value out",
                                      GetHash().ToString().c_str()));

            // Tally transaction fees
            int64_t nTxFee = nValueIn - GetValueOut();
            if (nTxFee < 0)
                return DoS(100, error("ConnectInputs() : %s nTxFee < 0", GetHash().ToString().c_str()));

            // enforce transaction fees for every block
            if (nTxFee < GetMinFee())
                return fBlock ? DoS(100,
                                    error("ConnectInputs() : %s not paying required fee=%s, paid=%s",
                                          GetHash().ToString().c_str(), FormatMoney(GetMinFee()).c_str(),
                                          FormatMoney(nTxFee).c_str()))
                              : false;

            nFees += nTxFee;
            if (!MoneyRange(nFees))
                return DoS(100, error("ConnectInputs() : nFees out of range"));
        }
    }

    return true;
}

bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndexSmartPtr& pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size() - 1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev) {
        CDiskBlockIndex blockindexPrev(boost::atomic_load(&pindex->pprev).get());
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    for (CTransaction& tx : vtx)
        SyncWithWallets(tx, this, false, false);

    return true;
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

/// returns all the blocks from the tip of the main chain up to the common ancestor (without the common
/// ancestor)
CBlock::CommonAncestorSuccessorBlocks CBlock::GetBlocksUpToCommonAncestorInMainChain() const
{
    CommonAncestorSuccessorBlocks res;

    // we expect both the main chain and the fork's previous block to be in the block index

    // main chain part
    CBlockIndexSmartPtr I = boost::atomic_load(&pindexBest);

    // fork part
    CBlockIndexSmartPtr T                  = nullptr;
    const uint256       commonAncestorHash = this->hashPrevBlock;
    auto                itTarget           = mapBlockIndex.find(commonAncestorHash);

    if (itTarget != mapBlockIndex.end()) {
        T = boost::atomic_load(&itTarget->second);
        // keep stepping back from the orphan (new block) until we find the main chain
        while (!T->IsInMainChain()) {
            // this map will be empty if the fork from main chain has only this block
            res.inFork.push_back(T->GetBlockHash());
            //            std::cout << "Block in fork chain: " << T->GetBlockHash().ToString() << "\t" <<
            //            T->nHeight
            //                      << std::endl;
            T = boost::atomic_load(&T->pprev);
        }
        //        std::cout << "Traversing " << itBest->second->nHeight - itTarget->second->nHeight << "
        //        blocks" << std::endl;
    } else {
        throw std::runtime_error("Failed to find target block " + commonAncestorHash.ToString() +
                                 "in block index in " + std::string(__PRETTY_FUNCTION__));
    }

    // the fork should be in temporal order because it's to be spent in order later
    std::reverse(res.inFork.begin(), res.inFork.end());

    assert(I != nullptr);
    assert(T != nullptr);
    // step back until we meet the main chain point in the fork
    while (I->GetBlockHash() != T->GetBlockHash() && I->GetBlockHash() != hashGenesisBlock &&
           I->GetBlockHash() != hashGenesisBlockTestNet) {
        res.inMainChain.insert(I->GetBlockHash());
        //        std::cout << "Block in main chain: " << I->GetBlockHash().ToString() << "\t" <<
        //        I->nHeight << std::endl;
        I = boost::atomic_load(&I->pprev);
    }
    //    std::cout << "End block hashes" << std::endl << std::endl;
    return res;
}

/**
 * @brief CBlock::GetAlternateChainTxsUpToCommonAncestor
 * @return
 * This function will get all the blocks from the main chain up to the common ancestor with this block;
 * then, it'll unspend all the transactions in these blocks and return them in the map. This is necessary
 * to solve the problem of stake attack described in VerifyInputsUnspent()
 */
CBlock::ChainReplaceTxs CBlock::GetAlternateChainTxsUpToCommonAncestor(CTxDB& txdb) const
{
    ChainReplaceTxs result;

    CommonAncestorSuccessorBlocks commonAncestory = GetBlocksUpToCommonAncestorInMainChain();
    std::vector<CTransaction>     mainChainBlocksTxs; // to be disconnected
    std::vector<CTransaction>     forkChainBlocksTxs; // to be reconnected

    // get all txs in blocks up to the common ancestor in the main chain
    for (const uint256& bh : commonAncestory.inMainChain) {
        CBlock blk;
        if (!txdb.ReadBlock(bh, blk, true)) {
            throw std::runtime_error("In main chain search, block " + bh.ToString() +
                                     " was not found in the database");
        }
        auto getHashFunc = [](const CTransaction& tx) { return tx.GetHash(); };
        std::transform(blk.vtx.cbegin(), blk.vtx.cend(),
                       std::inserter(result.disconnectedRootTxs, result.disconnectedRootTxs.begin()),
                       getHashFunc);
        std::move(blk.vtx.begin(), blk.vtx.end(), std::back_inserter(mainChainBlocksTxs));
    }

    // get all txs in the fork leading to this transaction (in order to test spending them later)
    for (const uint256& bh : commonAncestory.inFork) {
        CBlock blk;
        //        std::cout << "In fork block hash: " << bh.ToString() << std::endl;
        if (!txdb.ReadBlock(bh, blk, true)) {
            throw std::runtime_error("In fork chain search, block " + bh.ToString() +
                                     " was not found in the database");
        }
        //        for (const CTransaction& t : blk.vtx) {
        //            std::cout << "In fork tx hash: " << t.GetHash().ToString() << std::endl;
        //        }
        std::copy(blk.vtx.cbegin(), blk.vtx.cend(), std::back_inserter(forkChainBlocksTxs));
    }

    // unspend transactions that go from the main-chain up to the common ancessor (excluding the common
    // ancestor)
    //    std::cout << "Start disconnected txs" << std::endl;
    for (const CTransaction& tx : mainChainBlocksTxs) {
        //        std::cout << "Disconnecting inputs of: " << tx.GetHash().ToString() << std::endl;
        const std::vector<CTxIn>& vin = tx.vin;
        for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
            const CTxIn& txin = vin[inIdx];
            if (tx.IsCoinBase()) {
                continue;
            }
            uint256  outputTxHash  = txin.prevout.hash;
            unsigned outputNumInTx = txin.prevout.n;

            auto     idxIt = result.modifiedOutputsTxs.find(outputTxHash);
            CTxIndex txindex;
            if (idxIt == result.modifiedOutputsTxs.cend()) {
                // Get prev txindex from disk
                if (!txdb.ReadTxIndex(outputTxHash, txindex)) {
                    throw std::runtime_error(std::string(__PRETTY_FUNCTION__) +
                                             ": ReadTxIndex failed for transaction " +
                                             outputTxHash.ToString());
                }
            } else {
                // Get txindex from the map
                txindex = idxIt->second;
            }

            // check range
            if (outputNumInTx >= txindex.vSpent.size())
                throw std::runtime_error(
                    std::string(__PRETTY_FUNCTION__) + ": prevout.n out of range for transaction " +
                    outputTxHash.ToString() + " and output " + std::to_string(outputNumInTx));

            // unspend, which is equivalent to disconnecting the blockchain (without updating the
            // database)
            txindex.vSpent[outputNumInTx].SetNull();

            // this way, all main-chain spent transactions are in this map, while marked unspent
            result.modifiedOutputsTxs[outputTxHash] = txindex;
        }
    }

    //    std::cout << "End disconnected txs" << std::endl;

    //    std::cout << "Start reconnecting txs" << std::endl;
    // respend the transactions on the fork (to test whether this block is valid at this chain)
    for (const CTransaction& tx : forkChainBlocksTxs) {
        //        std::cout << "Reconnecting inputs in: " << tx.GetHash().ToString() << std::endl;
        const std::vector<CTxIn>& vin = tx.vin;
        for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
            const CTxIn& txin = vin[inIdx];
            if (tx.IsCoinBase()) {
                continue;
            }
            uint256  outputTxHash  = txin.prevout.hash;
            unsigned outputNumInTx = txin.prevout.n;

            auto     idxIt = result.modifiedOutputsTxs.find(outputTxHash);
            CTxIndex txindex;
            if (idxIt == result.modifiedOutputsTxs.cend()) {
                // Get prev txindex from disk
                if (!txdb.ReadTxIndex(outputTxHash, txindex)) {
                    throw std::runtime_error(std::string(__PRETTY_FUNCTION__) +
                                             ": ReadTxIndex failed for transaction " +
                                             outputTxHash.ToString());
                }
            } else {
                txindex = idxIt->second;
            }

            // check range
            if (outputNumInTx >= txindex.vSpent.size()) {
                throw std::runtime_error(
                    std::string(__PRETTY_FUNCTION__) + ": prevout.n out of range for transaction " +
                    outputTxHash.ToString() + " and output " + std::to_string(outputNumInTx));
            }

            // spend the output (without updating the database)
            txindex.vSpent[outputNumInTx] = CreateFakeSpentTxPos(txindex.pos.nBlockPos);

            result.modifiedOutputsTxs[outputTxHash] = txindex;
        }
        // since we're respending the transactions, we should store them in modified list because they
        // can be spent in subsequent blocks
        auto idxIt = result.modifiedOutputsTxs.find(tx.GetHash());
        if (idxIt == result.modifiedOutputsTxs.cend()) {
            result.modifiedOutputsTxs[tx.GetHash()] =
                CTxIndex(CDiskTxPos(this->GetHash(), 3), tx.vout.size());
        }
    }
    //    std::cout << "End reconnecting txs" << std::endl;
    return result;
}

bool CBlock::VerifyInputsUnspent(CTxDB& txdb) const
{
    // this function solves the problem in
    // https://medium.com/@dsl_uiuc/fake-stake-attacks-on-chain-based-proof-of-stake-cryptocurrencies-b8b05723f806
    // this function doesn't modify the database or the block being analyzed

    // in order to do this, we reverse/disconnect transactions up to the common ancestor (without
    // modifying the database), then we replay the new chain up the this block, finally we check whether
    // this block is valid in the chain constructed from common ancestor to this

    // queued transactions are the inputs that we already found (even from this block). The map stores
    // whether transactions are spent already. This solves the problem of spending an output in the same
    // block where it's created
    ChainReplaceTxs alternateChainTxs;

    try {
        alternateChainTxs = GetAlternateChainTxsUpToCommonAncestor(txdb);
    } catch (std::exception& ex) {
        return error("Failed to verify unspent inputs for block %s; error: %s",
                     this->GetHash().ToString().c_str(), ex.what());
    }

    std::unordered_map<uint256, CTxIndex>& queuedTxs = alternateChainTxs.modifiedOutputsTxs;
    const std::unordered_set<uint256>&     disconnectedTxsFromMainChain =
        alternateChainTxs.disconnectedRootTxs;

    for (const CTransaction& tx : vtx) {
        //        std::cout << "Working on tx: " << tx.GetHash().ToString() << std::endl;
        {
            // since this function checks whether all transactions are spent, this should exclude BIP30
            // cases, where a duplicate transaction is being rewritten
            CTxIndex  txindexOld;
            uint256&& txHash = tx.GetHash();
            // if the transaction already exists in the blockchain and is not part of the reversed
            // transactions
            bool foundInForkedBlocks =
                disconnectedTxsFromMainChain.find(txHash) != disconnectedTxsFromMainChain.cend();
            if (txdb.ContainsTx(txHash) && txdb.ReadTxIndex(txHash, txindexOld) &&
                !foundInForkedBlocks) {

                printf("Found a transaction %s in a new block while already in the blockchain in block "
                       "%s; this is a BIP30 possible attack\n",
                       txHash.ToString().c_str(), txindexOld.pos.nBlockPos.ToString().c_str());
                continue;
            }
        }

        {
            // if an output in the transaction is spent in the same block, it should also be found in the
            // queued transactions list in order for the tests below to work because it's not in the
            // blockchain yet
            auto it = queuedTxs.find(tx.GetHash());
            if (it == queuedTxs.cend()) {
                // unspent tx (all vSpent are null)
                CTxIndex txindex(CDiskTxPos(this->GetHash(), 2), tx.vout.size());
                queuedTxs[tx.GetHash()] = txindex;
            }
        }

        // coinbase don't have any inputs
        if (tx.IsCoinBase()) {
            continue;
        }

        // loop over inputs of this transaction, and check whether the outputs are already spent
        const std::vector<CTxIn>& vin = tx.vin;
        for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
            uint256  outputTxHash  = vin[inIdx].prevout.hash;
            unsigned outputNumInTx = vin[inIdx].prevout.n;
            CTxIndex txindex;
            auto     it                = queuedTxs.find(vin[inIdx].prevout.hash);
            bool     inputFoundInQueue = (it != queuedTxs.cend());
            if (inputFoundInQueue) {
                if (it->second.vSpent[outputNumInTx].IsNull()) {
                    // tx is not spent yet, so we mark it as spent
                    it->second.vSpent[outputNumInTx] = CreateFakeSpentTxPos(this->GetHash());
                } else {
                    return error("Output number %u in tx %s which is an input to tx %s is attempting to "
                                 "double-spend in the same block %s",
                                 outputNumInTx, outputTxHash.ToString().c_str(),
                                 tx.GetHash().ToString().c_str(), this->GetHash().ToString().c_str());
                }
            } else if (txdb.ContainsTx(outputTxHash) && txdb.ReadTxIndex(outputTxHash, txindex)) {
                queuedTxs[outputTxHash] = txindex;
                if (txindex.vSpent[outputNumInTx].IsNull()) {
                    queuedTxs.find(outputTxHash)->second.vSpent[outputNumInTx] =
                        CreateFakeSpentTxPos(this->GetHash());
                } else {
                    return error("Output number %u in tx %s which is an input to tx %s and is being "
                                 "attempted to spend it in block %s, this is a "
                                 "double-spend attempt",
                                 outputNumInTx, outputTxHash.ToString().c_str(),
                                 tx.GetHash().ToString().c_str(), this->GetHash().ToString().c_str());
                }
            } else {
                return error("Output number %u in tx %s which is an input to tx %s and is being "
                             "attempted to spend it in block %s. it's an invalid tx",
                             outputNumInTx, outputTxHash.ToString().c_str(),
                             tx.GetHash().ToString().c_str(),
                             vin[inIdx].prevout.hash.ToString().c_str());
            }
        }
    }
    return true;
}

bool CBlock::CheckBIP30Attack(CTxDB& txdb, const uint256& hashTx)
{
    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00
    // UTC. Now that the whole chain is irreversibly beyond that time it is applied to all blocks
    // except the two in the chain that violate it. This prevents exploiting the issue against nodes
    // in their initial block download.

    CTxIndex txindexOld;
    if (txdb.ContainsTx(hashTx) && txdb.ReadTxIndex(hashTx, txindexOld)) {
        for (CDiskTxPos& pos : txindexOld.vSpent)
            if (pos.IsNull())
                return false;
    }
    return true;
}

bool CBlock::ConnectBlock(CTxDB& txdb, const CBlockIndexSmartPtr& pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(!fJustCheck, !fJustCheck, false))
        return false;

    //// issue here: it doesn't know the version
    unsigned int nTxPos;
    if (fJustCheck)
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably
        // shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    else
        nTxPos = ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION) - (2 * GetSizeOfCompactSize(0)) +
                 GetSizeOfCompactSize(vtx.size());

    // Comment by Sam: mapQueuedChanges is the list of transactions that already happened in the same
    // block. This is necessary for verifying outputs that are being spent in the same blocks

    map<uint256, CTxIndex> mapQueuedChanges;
    int64_t                nFees        = 0;
    int64_t                nValueIn     = 0;
    int64_t                nValueOut    = 0;
    int64_t                nStakeReward = 0;

    map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>> mapQueuedNTP1Inputs;

    unsigned int nSigOps = 0;

    // map of issued token names in this block vs token hashes
    // this is used to prevent duplicate token names
    std::unordered_map<std::string, uint256> issuedTokensSymbolsInThisBlock;

    for (CTransaction& tx : vtx) {
        uint256 hashTx = tx.GetHash();

        std::vector<std::pair<CTransaction, NTP1Transaction>> inputsWithNTP1;

        if (!CheckBIP30Attack(txdb, hashTx)) {
            return error(
                "Block %s was rejected as it seems that an attempt of BIP30 attack was attempted\n",
                this->GetHash().ToString().c_str());
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock() : too many sigops"));

        CDiskTxPos posThisTx(pindex->blockKeyInDB, nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
            nValueOut += tx.GetValueOut();
        else {
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
                return false;

            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nSigOps > MAX_BLOCK_SIGOPS) {
                return DoS(100, error("ConnectBlock() : too many sigops"));
            }

            int64_t nTxValueIn  = tx.GetValueIn(mapInputs);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
                nStakeReward = nTxValueOut - nTxValueIn;

            if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
                try {
                    if (IsTxNTP1(&tx)) {
                        // check if there are inputs already cached
                        inputsWithNTP1 =
                            GetAllNTP1InputsOfTx(tx, txdb, true, mapQueuedNTP1Inputs, mapQueuedChanges);

                        // write NTP1 transactions' data
                        NTP1Transaction ntp1tx;
                        ntp1tx.readNTP1DataFromTx(tx, inputsWithNTP1);
                    }
                } catch (std::exception& ex) {
                    return error("Error while verifying NTP1Transaction validity in ConnectBlock(): "
                                 "%s\n",
                                 ex.what());
                } catch (...) {
                    return error("Error while verifying NTP1Transaction validity in ConnectBlock(). "
                                 "Unknown exception thrown\n");
                }
            }

            if (EnableEnforceUniqueTokenSymbols()) {
                try {
                    AssertIssuanceUniquenessInBlock(issuedTokensSymbolsInThisBlock, txdb, tx,
                                                    mapQueuedNTP1Inputs, mapQueuedChanges);
                } catch (std::exception& ex) {
                    return error("Error while verifying the uniqueness of issued token symbol in "
                                 "ConnectBlock(): "
                                 "%s\n",
                                 ex.what());
                } catch (...) {
                    return error("Error while verifying the uniqueness of issued token symbol in "
                                 "ConnectBlock(). "
                                 "Unknown exception thrown\n");
                }
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false)) {
                return false;
            }
        }

        mapQueuedChanges[hashTx]          = CTxIndex(posThisTx, tx.vout.size());
        mapQueuedNTP1Inputs[tx.GetHash()] = inputsWithNTP1;
    }

    if (IsProofOfWork()) {
        int64_t nReward = GetProofOfWorkReward(nFees);
        // Check coinbase reward
        if (vtx[0].GetValueOut() > nReward)
            return DoS(50, error("ConnectBlock() : coinbase reward exceeded (actual=%" PRId64
                                 " vs calculated=%" PRId64 ")",
                                 vtx[0].GetValueOut(), nReward));
    }
    if (IsProofOfStake()) {
        // ppcoin: coin stake tx earns reward instead of paying fee
        uint64_t nCoinAge;
        if (!vtx[1].GetCoinAge(txdb, nCoinAge))
            return error("ConnectBlock() : %s unable to get coin age for coinstake",
                         vtx[1].GetHash().ToString().c_str());

        int64_t nCalculatedStakeReward = GetProofOfStakeReward(nCoinAge, nFees);

        if (nStakeReward > nCalculatedStakeReward)
            return DoS(100, error("ConnectBlock() : coinstake pays too much(actual=%" PRId64
                                  " vs calculated=%" PRId64 ")",
                                  nStakeReward, nCalculatedStakeReward));
    }

    // ppcoin: track money supply and mint amount info
    pindex->nMint        = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = (pindex->pprev ? pindex->pprev->nMoneySupply : 0) + nValueOut - nValueIn;
    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex.get())))
        return error("Connect() : WriteBlockIndex for pindex failed");

    if (fJustCheck)
        return true;

    // Write queued txindex changes
    for (map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin(); mi != mapQueuedChanges.end();
         ++mi) {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    // This scope does NTP1 data writing
    {
        try {
            WriteNTP1BlockTransactionsToDisk(vtx, txdb);
        } catch (std::exception& ex) {
            if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
                return error("Unable to get NTP1 transaction written in ConnectBlock(). Error: %s\n",
                             ex.what());
            }
        } catch (...) {
            if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
                return error("Unable to get NTP1 transaction written in ConnectBlock(). An unknown "
                             "exception was "
                             "thrown");
            }
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev) {
        CDiskBlockIndex blockindexPrev(boost::atomic_load(&pindex->pprev).get());
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex failed");
    }

    // Watch for transactions paying to me
    for (CTransaction& tx : vtx)
        SyncWithWallets(tx, this, true);

    return true;
}

bool static Reorganize(CTxDB& txdb, CBlockIndexSmartPtr& pindexNew,
                       const bool createDbTransaction = true)
{
    printf("REORGANIZE\n");

    // Find the fork
    CBlockIndexSmartPtr pfork   = boost::atomic_load(&pindexBest);
    CBlockIndexSmartPtr plonger = pindexNew;
    while (pfork != plonger) {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = boost::atomic_load(&plonger->pprev)))
                return error("Reorganize() : plonger->pprev is null");
        if (pfork == plonger)
            break;
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
    }

    // List of what to disconnect
    vector<CBlockIndexSmartPtr> vDisconnect;
    for (CBlockIndexSmartPtr pindex = boost::atomic_load(&pindexBest); pindex != pfork;
         pindex                     = boost::atomic_load(&pindex->pprev)) {
        vDisconnect.push_back(pindex);
    }

    // List of what to connect
    vector<CBlockIndexSmartPtr> vConnect;
    for (CBlockIndexSmartPtr pindex = pindexNew; pindex != pfork;
         pindex                     = boost::atomic_load(&pindex->pprev)) {
        vConnect.push_back(pindex);
    }
    reverse(vConnect.begin(), vConnect.end());

    printf("REORGANIZE: Disconnect %" PRIszu " blocks; %s..%s\n", vDisconnect.size(),
           pfork->GetBlockHash().ToString().c_str(),
           boost::atomic_load(&pindexBest)->GetBlockHash().ToString().c_str());
    printf("REORGANIZE: Connect %" PRIszu " blocks; %s..%s\n", vConnect.size(),
           pfork->GetBlockHash().ToString().c_str(), pindexNew->GetBlockHash().ToString().c_str());

    // Disconnect shorter branch
    list<CTransaction> vResurrect;
    for (CBlockIndexSmartPtr& pindex : vDisconnect) {
        CBlock block;
        if (!block.ReadFromDisk(pindex.get()))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, pindex))
            return error("Reorganize() : DisconnectBlock %s failed",
                         pindex->GetBlockHash().ToString().c_str());

        // Queue memory transactions to resurrect.
        // We only do this for blocks after the last checkpoint (reorganisation before that
        // point should only happen with -reindex/-loadblock, or a misbehaving peer.
        BOOST_REVERSE_FOREACH(const CTransaction& tx, block.vtx)
        if (!(tx.IsCoinBase() || tx.IsCoinStake()) &&
            pindex->nHeight > Checkpoints::GetTotalBlocksEstimate())
            vResurrect.push_front(tx);
    }

    // Connect longer branch
    vector<CTransaction> vDelete;
    for (unsigned int i = 0; i < vConnect.size(); i++) {
        CBlockIndexSmartPtr pindex = vConnect[i];
        CBlock              block;
        if (!block.ReadFromDisk(pindex.get(), txdb))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.ConnectBlock(txdb, pindex)) {
            // Invalid block
            return error("Reorganize() : ConnectBlock %s failed",
                         pindex->GetBlockHash().ToString().c_str());
        }

        // Queue memory transactions to delete
        for (const CTransaction& tx : block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");

    // Make sure it's successfully written to disk before changing memory structure
    if (createDbTransaction && !txdb.TxnCommit())
        return error("Reorganize() : TxnCommit failed");

    // Disconnect shorter branch
    for (CBlockIndexSmartPtr& pindex : vDisconnect)
        if (pindex->pprev)
            pindex->pprev->pnext = nullptr;

    // Connect longer branch
    for (CBlockIndexSmartPtr& pindex : vConnect)
        if (pindex->pprev)
            pindex->pprev->pnext = pindex;

    // Resurrect memory transactions that were in the disconnected branch
    for (CTransaction& tx : vResurrect)
        AcceptToMemoryPool(mempool, tx, NULL);

    // Delete redundant memory transactions that are in the connected branch
    for (CTransaction& tx : vDelete) {
        mempool.remove(tx);
        mempool.removeConflicts(tx);
    }

    printf("REORGANIZE: done\n");

    return true;
}

// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDB& txdb, const CBlockIndexSmartPtr& pindexNew,
                               const bool createDbTransaction)
{
    uint256 hash = GetHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash)) {
        if (createDbTransaction) {
            txdb.TxnAbort();
        }
        InvalidChainFound(pindexNew, txdb);
        return false;
    }
    if (createDbTransaction && !txdb.TxnCommit())
        return error("SetBestChain() : TxnCommit failed");

    // Add to current best branch
    pindexNew->pprev->pnext = pindexNew;

    // Delete redundant memory transactions
    for (CTransaction& tx : vtx)
        mempool.remove(tx);

    return true;
}

bool CBlock::SetBestChain(CTxDB& txdb, const CBlockIndexSmartPtr& pindexNew,
                          const bool createDbTransaction)
{
    uint256 hash = GetHash();

    if (createDbTransaction && !txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == NULL && hash == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet)) {
        txdb.WriteHashBestChain(hash);
        if (createDbTransaction && !txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    } else if (hashPrevBlock == hashBestChain) {
        if (!SetBestChainInner(txdb, pindexNew, createDbTransaction))
            return error("SetBestChain() : SetBestChainInner failed");
    } else {
        // the first block in the new chain that will cause it to become the new best chain
        CBlockIndexSmartPtr pindexIntermediate = pindexNew;

        // list of blocks that need to be connected afterwards
        std::vector<CBlockIndexSmartPtr> vpindexSecondary;

        // Reorganize is costly in terms of db load, as it works in a single db transaction.
        // Try to limit how much needs to be done inside
        while (pindexIntermediate->pprev &&
               pindexIntermediate->pprev->nChainTrust > boost::atomic_load(&pindexBest)->nChainTrust) {
            vpindexSecondary.push_back(pindexIntermediate);
            pindexIntermediate = pindexIntermediate->pprev;
        }

        if (!vpindexSecondary.empty())
            printf("Postponing %" PRIszu " reconnects\n", vpindexSecondary.size());

        // Switch to new best branch
        if (!Reorganize(txdb, pindexIntermediate, createDbTransaction)) {
            if (createDbTransaction) {
                txdb.TxnAbort();
            }
            InvalidChainFound(pindexNew, txdb);
            return error("SetBestChain() : Reorganize failed");
        }

        // Connect further blocks
        BOOST_REVERSE_FOREACH(CBlockIndexSmartPtr pindex, vpindexSecondary)
        {
            CBlock block;
            if (!block.ReadFromDisk(pindex.get(), txdb)) {
                printf("SetBestChain() : ReadFromDisk failed\n");
                break;
            }
            if (createDbTransaction && !txdb.TxnBegin()) {
                printf("SetBestChain() : TxnBegin 2 failed\n");
                break;
            }
            // errors now are not fatal, we still did a reorganisation to a new chain in a valid way
            if (!block.SetBestChainInner(txdb, pindex, createDbTransaction))
                break;
        }
    }

    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload) {
        const CBlockLocator locator(pindexNew.get());
        ::SetBestChain(locator);
    }

    // New best block
    hashBestChain = hash;
    boost::atomic_store(&pindexBest, pindexNew);
    CBlockIndexSmartPtr pindexBestPtr = boost::atomic_load(&pindexBest);
    pblockindexFBBHLast               = nullptr;
    nBestHeight                       = pindexBestPtr->nHeight;
    nBestChainTrust                   = pindexNew->nChainTrust;
    nTimeBestReceived                 = GetTime();
    nTransactionsUpdated++;

    uint256 nBestBlockTrust = pindexBestPtr->nHeight != 0
                                  ? (pindexBestPtr->nChainTrust - pindexBestPtr->pprev->nChainTrust)
                                  : pindexBestPtr->nChainTrust;

    printf("SetBestChain: new best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
           hashBestChain.ToString().c_str(), nBestHeight.load(),
           CBigNum(nBestChainTrust).ToString().c_str(), nBestBlockTrust.Get64(),
           DateTimeStrFormat("%x %H:%M:%S", pindexBestPtr->GetBlockTime()).c_str());

    // Check the version of the last 100 blocks to see if we need to upgrade:
    if (!fIsInitialDownload) {
        int                      nUpgraded = 0;
        ConstCBlockIndexSmartPtr pindex    = boost::atomic_load(&pindexBest);
        for (int i = 0; i < 100 && pindex != NULL; i++) {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            printf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded,
                   CBlock::CURRENT_VERSION);
        if (nUpgraded > 100 / 2)
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the
            // user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
    }

    std::string strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty()) {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool CTransaction::GetCoinAge(CTxDB& txdb, uint64_t& nCoinAge) const
{
    CBigNum      bnCentSecond = 0; // coin age in the unit of cent-seconds
    unsigned int nSMA         = StakeMinAge();
    nCoinAge                  = 0;

    if (IsCoinBase())
        return true;

    for (const CTxIn& txin : vin) {
        // First try finding the previous transaction in database
        CTransaction txPrev;
        CTxIndex     txindex;
        if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
            continue; // previous transaction not in main chain
        if (nTime < txPrev.nTime)
            return false; // Transaction timestamp violation

        // Read block header
        CBlock block;
        if (!block.ReadFromDisk(txindex.pos.nBlockPos, false))
            return false; // unable to read block of previous transaction
        if (block.GetBlockTime() + nSMA > nTime)
            continue; // only count coins meeting min age requirement

        int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += CBigNum(nValueIn) * (nTime - txPrev.nTime) / CENT;

        if (fDebug)
            printf("coin age nValueIn=%" PRId64 " nTimeDiff=%d bnCentSecond=%s\n", nValueIn,
                   nTime - txPrev.nTime, bnCentSecond.ToString().c_str());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug)
        printf("coin age bnCoinDay=%s\n", bnCoinDay.ToString().c_str());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}

// ppcoin: total coin age spent in block, in the unit of coin-days.
bool CBlock::GetCoinAge(uint64_t& nCoinAge) const
{
    nCoinAge = 0;

    CTxDB txdb("r");
    for (const CTransaction& tx : vtx) {
        uint64_t nTxCoinAge;
        if (tx.GetCoinAge(txdb, nTxCoinAge))
            nCoinAge += nTxCoinAge;
        else
            return false;
    }

    if (nCoinAge == 0) // block coin age minimum 1 coin-day
        nCoinAge = 1;
    if (fDebug)
        printf("block coin age total nCoinDays=%" PRId64 "\n", nCoinAge);
    return true;
}

bool CBlock::AddToBlockIndex(uint256 nBlockPos, const uint256& hashProof, CTxDB& txdb,
                             CBlockIndexSmartPtr* newBlockIdxPtr, const bool createDbTransaction)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().c_str());

    // Construct new block index object
    CBlockIndexSmartPtr pindexNew = boost::make_shared<CBlockIndex>(nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    pindexNew->phashBlock              = &hash;
    BlockIndexMapType::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end()) {
        pindexNew->pprev   = boost::atomic_load(&miPrev->second);
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // ppcoin: compute chain trust score
    pindexNew->nChainTrust =
        (pindexNew->pprev ? pindexNew->pprev->nChainTrust : 0) + pindexNew->GetBlockTrust();

    // ppcoin: compute stake entropy bit for stake modifier
    if (!pindexNew->SetStakeEntropyBit(GetStakeEntropyBit()))
        return error("AddToBlockIndex() : SetStakeEntropyBit() failed");

    // Record proof hash value
    pindexNew->hashProof = hashProof;

    // ppcoin: compute stake modifier
    uint64_t nStakeModifier          = 0;
    bool     fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(pindexNew->pprev.get(), nStakeModifier, fGeneratedStakeModifier))
        return error("AddToBlockIndex() : ComputeNextStakeModifier() failed");
    pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew.get());
    if (!CheckStakeModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
        return error("AddToBlockIndex() : Rejected by stake modifier checkpoint height=%d, "
                     "modifier=0x%016" PRIx64,
                     pindexNew->nHeight, nStakeModifier);
    // return error("AddToBlockIndex() : Rejected by stake modifier checkpoint height=%d,
    // checksum=0x%016" PRIx64, pindexNew->nHeight, pindexNew->nStakeModifierChecksum);

    // Add to mapBlockIndex
    BlockIndexMapType::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    pindexNew->phashBlock = &((*mi).first);

    // Write to disk block index
    if (createDbTransaction && !txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew.get()));
    if (createDbTransaction && !txdb.TxnCommit())
        return false;

    LOCK(cs_main);

    // New best
    if (pindexNew->nChainTrust > nBestChainTrust)
        if (!SetBestChain(txdb, pindexNew, createDbTransaction))
            return false;

    if (pindexNew == pindexBest) {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }

    uiInterface.NotifyBlocksChanged();

    if (newBlockIdxPtr != nullptr) {
        *newBlockIdxPtr = pindexNew;
    }
    // TODO: Sam: this seems to need an "else" case. It doesn't make sense to ignore it

    return true;
}

bool CBlock::CheckBlock(bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig) const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    unsigned int nSizeLimit = MaxBlockSize();
    if (vtx.empty() || vtx.size() > nSizeLimit ||
        ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > nSizeLimit)
        return DoS(100, error("CheckBlock() : size limits failed"));

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(GetPoWHash(), nBits))
        return DoS(50, error("CheckBlock() : proof of work failed"));

    // Check timestamp
    if (GetBlockTime() > FutureDrift(GetAdjustedTime()))
        return error("CheckBlock() : block timestamp too far in the future");

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return DoS(100, error("CheckBlock() : first tx is not coinbase"));
    for (unsigned int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return DoS(100, error("CheckBlock() : more than one coinbase"));

    // Check coinbase timestamp
    if (GetBlockTime() > FutureDrift((int64_t)vtx[0].nTime))
        return DoS(50, error("CheckBlock() : coinbase timestamp is too early"));

    if (IsProofOfStake()) {
        // Coinbase output should be empty if proof-of-stake block
        if (vtx[0].vout.size() != 1 || !vtx[0].vout[0].IsEmpty())
            return DoS(100, error("CheckBlock() : coinbase output not empty for proof-of-stake block"));

        // Second transaction must be coinstake, the rest must not be
        if (vtx.empty() || !vtx[1].IsCoinStake())
            return DoS(100, error("CheckBlock() : second tx is not coinstake"));
        for (unsigned int i = 2; i < vtx.size(); i++)
            if (vtx[i].IsCoinStake())
                return DoS(100, error("CheckBlock() : more than one coinstake"));

        // Check coinstake timestamp
        if (!CheckCoinStakeTimestamp(GetBlockTime(), (int64_t)vtx[1].nTime))
            return DoS(50, error("CheckBlock() : coinstake timestamp violation nTimeBlock=%" PRId64
                                 " nTimeTx=%u",
                                 GetBlockTime(), vtx[1].nTime));

        // NovaCoin: check proof-of-stake block signature
        if (fCheckSig && !CheckBlockSignature())
            return DoS(100, error("CheckBlock() : bad proof-of-stake block signature"));
    }

    // Check transactions
    for (const CTransaction& tx : vtx) {
        if (!tx.CheckTransaction())
            return DoS(tx.nDoS, error("CheckBlock() : CheckTransaction failed"));

        // ppcoin: check transaction timestamp
        if (GetBlockTime() < (int64_t)tx.nTime)
            return DoS(50, error("CheckBlock() : block timestamp earlier than transaction timestamp"));
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    set<uint256> uniqueTx;
    for (const CTransaction& tx : vtx) {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock() : duplicate transaction"));

    unsigned int nSigOps = 0;
    for (const CTransaction& tx : vtx) {
        nSigOps += tx.GetLegacySigOpCount();
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"));

    return true;
}

CTransaction FetchTxFromDisk(const uint256& txid)
{
    CTxDB txdb;
    return FetchTxFromDisk(txid, txdb);
}

CTransaction FetchTxFromDisk(const uint256& txid, CTxDB& txdb)
{
    CTransaction result;
    CTxIndex     txPos;
    if (!txdb.ReadTxIndex(txid, txPos)) {
        printf("Unable to read standard transaction from db: %s\n", txid.ToString().c_str());
        throw std::runtime_error("Unable to read standard transaction from db: " + txid.ToString());
    }
    if (!result.ReadFromDisk(txPos.pos, txdb)) {
        printf("Unable to read standard transaction from disk with the "
               "index given by db: %s\n",
               txid.ToString().c_str());
        throw std::runtime_error("Unable to read standard transaction from db: " + txid.ToString());
    }
    return result;
}

bool RecoverNTP1TxInDatabase(const CTransaction& tx, CTxDB& txdb, bool recoveryProtection,
                             unsigned recurseDepth)
{
    printf("Recovering NTP1 transaction in database: %s\n", tx.GetHash().ToString().c_str());

    // prevent recursively attempting to recover the same transactions again and again

    if (recoveryProtection && UnrecoverableNTP1Txs.find(tx.GetHash()) != UnrecoverableNTP1Txs.end()) {
        printf("Will not recover transaction %s; it was marked for non-recovery. Restart to attempt to "
               "recover again.\n",
               tx.GetHash().ToString().c_str());
        return false;
    }

    std::vector<std::pair<CTransaction, NTP1Transaction>> ntp1inputs;
    try {
        ntp1inputs = GetAllNTP1InputsOfTx(tx, txdb, recoveryProtection);
    } catch (std::exception& ex) {
        printf("Error: Attempting to recursively recover the inputs. Failed to recover NTP1 "
               "transaction: %s; with error: %s\n",
               tx.GetHash().ToString().c_str(), ex.what());
        ntp1inputs.clear();
        for (const auto& in : tx.vin) {
            CTransaction inputTx;
            try {
                inputTx = FetchTxFromDisk(in.prevout.hash, txdb);
                bool anyInputBeforeWrongBlockHeights =
                    !PassedFirstValidNTP1Tx(GetTxBlockHeight(inputTx.GetHash()), fTestNet);
                bool isNTP1 = IsTxNTP1(&inputTx);
                if (anyInputBeforeWrongBlockHeights && isNTP1) {
                    printf("Error: cannot recover transaction with hash %s; the NTP1 input of this "
                           "transaction %s happened before the allowed limit.\n",
                           tx.GetHash().ToString().c_str(), inputTx.GetHash().ToString().c_str());
                    if (recoveryProtection) {
                        UnrecoverableNTP1Txs.insert(tx.GetHash());
                    }
                    return false;
                }
            } catch (std::exception& exIn) {
                printf("Error: Failed to retrieve standard neblio tranasction %s; this happened in the "
                       "context of recovering the NTP1 transaction: %s\n, making recovery not "
                       "possible. Error given: %s\n",
                       tx.GetHash().ToString().c_str(), in.prevout.hash.ToString().c_str(), exIn.what());
                if (recoveryProtection) {
                    UnrecoverableNTP1Txs.insert(tx.GetHash());
                }
                return false;
            }
            std::pair<CTransaction, NTP1Transaction> inputTxPair =
                std::make_pair(inputTx, NTP1Transaction());
            FetchNTP1TxFromDisk(inputTxPair, txdb, recurseDepth);
            ntp1inputs.push_back(inputTxPair);
        }
    }
    try {
        for (const auto in : ntp1inputs) {
            bool anyInputBeforeWrongBlockHeights =
                !PassedFirstValidNTP1Tx(GetTxBlockHeight(in.first.GetHash()), fTestNet);
            bool isNTP1 = IsTxNTP1(&in.first);
            if (anyInputBeforeWrongBlockHeights && isNTP1) {
                printf("One of the inputs of the NTP1 transaction %s, which is %s, is bofore the "
                       "allowed block height. "
                       "This cannot be recovered.\n",
                       tx.GetHash().ToString().c_str(), in.first.GetHash().ToString().c_str());
                if (recoveryProtection) {
                    UnrecoverableNTP1Txs.insert(tx.GetHash());
                }
                return false;
            }
        }
        NTP1Transaction ntp1tx;
        ntp1tx.readNTP1DataFromTx(tx, ntp1inputs);
        WriteNTP1TxToDbAndDisk(ntp1tx, txdb);
        printf("Recovering transation: %s is done successfully.\n", tx.GetHash().ToString().c_str());
    } catch (std::exception& ex) {
        printf("Error: Failed to retrieve read NTP1 transaction while attempting to recover NTP1 "
               "transaction %s; Error: %s\n",
               tx.GetHash().ToString().c_str(), ex.what());
        if (recoveryProtection) {
            UnrecoverableNTP1Txs.insert(tx.GetHash());
        }
        return false;
    }
    return true;
}

void FetchNTP1TxFromDisk(std::pair<CTransaction, NTP1Transaction>& txPair, CTxDB& txdb,
                         bool /*recoverProtection*/, unsigned /*recurseDepth*/)
{
    if (!IsTxNTP1(&txPair.first)) {
        return;
    }
    if (!txdb.ReadNTP1Tx(txPair.first.GetHash(), txPair.second)) {
        //        printf("Unable to read NTP1 transaction from db: %s\n",
        //               txPair.first.GetHash().ToString().c_str());
        //        if (recurseDepth < 32) {
        //            if (RecoverNTP1TxInDatabase(txPair.first, txdb, recoverProtection, recurseDepth +
        //            1)) {
        //                FetchNTP1TxFromDisk(txPair, txdb, recurseDepth + 1);
        //            } else {
        //                printf("Error: Failed to retrieve (and recover) NTP1 transaction %s.\n",
        //                       txPair.first.GetHash().ToString().c_str());
        //            }
        //        } else {
        //            printf("Error: max recursion depth, %u, reached while fetching transaction %s.
        //            Stopping!\n",
        //                   recurseDepth, txPair.first.GetHash().ToString().c_str());
        //        }
        printf("Failed to fetch NTP1 transaction %s", txPair.first.GetHash().ToString().c_str());
        return;
    }
    txPair.second.updateDebugStrHash();
}

void WriteNTP1TxToDbAndDisk(const NTP1Transaction& ntp1tx, CTxDB& txdb)
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
        if (!IsNTP1TokenBlacklisted(tokenId)) {
            if (!txdb.WriteNTP1TxWithTokenSymbol(ntp1tx.getTokenSymbolIfIssuance(), ntp1tx)) {
                throw std::runtime_error("Unable to write NTP1 transaction to database: " +
                                         ntp1tx.getTxHash().ToString());
            }
        }
    }
}

std::vector<std::pair<CTransaction, NTP1Transaction>>
StdFetchedInputTxsToNTP1(const CTransaction& tx, const MapPrevTx& mapInputs, CTxDB& txdb,
                         bool recoverProtection, const map<uint256, CTxIndex>& queuedAcceptedTxs,
                         int recursionCount)
{
    // It's not possible to use default parameter here because NTP1Transaction is an incomplete type in
    // main.h, and including NTP1Transaction header file is not possible due to circular dependency
    return StdFetchedInputTxsToNTP1(
        tx, mapInputs, txdb, recoverProtection,
        map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>(), queuedAcceptedTxs,
        recursionCount);
}

std::vector<std::pair<CTransaction, NTP1Transaction>> StdFetchedInputTxsToNTP1(
    const CTransaction& tx, const MapPrevTx& mapInputs, CTxDB& txdb, bool recoverProtection,
    const map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>& mapQueuedNTP1Inputs,
    const map<uint256, CTxIndex>& queuedAcceptedTxs, int recursionCount)
{
    if (recursionCount >= 32) {
        throw std::runtime_error("Reached maximum recursion in StdFetchedInputTxsToNTP1");
    }

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputsWithNTP1;
    // put the input transactions in a vector with their corresponding NTP1 transactions
    {
        inputsWithNTP1.clear();
        std::transform(tx.vin.begin(), tx.vin.end(), std::back_inserter(inputsWithNTP1),
                       [&mapInputs, &tx](const CTxIn& in) {
                           if (mapInputs.count(in.prevout.hash) == 0) {
                               throw std::runtime_error("Could not find input after having fetched it "
                                                        "(for NTP1 database storage); for tx: " +
                                                        tx.GetHash().ToString());
                           }
                           auto result = std::make_pair(mapInputs.find(in.prevout.hash)->second.second,
                                                        NTP1Transaction());
                           result.second.readNTP1DataFromTx_minimal(result.first);
                           return result;
                       });
    }

    // NTP1 transaction data is either in the test pool OR in the database; no third option here
    auto it = mapQueuedNTP1Inputs.find(tx.GetHash());
    if (it == mapQueuedNTP1Inputs.end()) {
        for (auto&& inTx : inputsWithNTP1) {
            if (IsTxNTP1(&inTx.first)) {
                if (txdb.ContainsNTP1Tx(inTx.first.GetHash())) {
                    // if the transaction is in the database, get it
                    FetchNTP1TxFromDisk(inTx, txdb, recoverProtection);
                } else if (queuedAcceptedTxs.find(inTx.first.GetHash()) != queuedAcceptedTxs.end()) {
                    // otherwise, if the transaction is already in the test pool, use it to read it

                    std::vector<std::pair<CTransaction, NTP1Transaction>> inputsOfInput =
                        GetAllNTP1InputsOfTx(inTx.first, txdb, recoverProtection, mapQueuedNTP1Inputs,
                                             queuedAcceptedTxs, recursionCount + 1);

                    NTP1Transaction ntp1tx;
                    inTx.second.readNTP1DataFromTx(inTx.first, inputsOfInput);
                } else {
                    // read NTP1 transaction inputs. If they fail, that's OK, because they will
                    // fail later if they're necessary
                    FetchNTP1TxFromDisk(inTx, txdb, recoverProtection);
                }
            }
        }
    } else {
        inputsWithNTP1 = it->second;
    }
    return inputsWithNTP1;
}

std::vector<std::pair<CTransaction, NTP1Transaction>>
GetAllNTP1InputsOfTx(CTransaction tx, bool recoverProtection, int recursionCount)
{
    CTxDB txdb;
    return GetAllNTP1InputsOfTx(tx, txdb, recoverProtection,
                                map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>(),
                                map<uint256, CTxIndex>(), recursionCount);
}

std::vector<std::pair<CTransaction, NTP1Transaction>>
GetAllNTP1InputsOfTx(CTransaction tx, CTxDB& txdb, bool recoverProtection, int recursionCount)
{
    return GetAllNTP1InputsOfTx(tx, txdb, recoverProtection,
                                map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>(),
                                map<uint256, CTxIndex>(), recursionCount);
}

std::vector<std::pair<CTransaction, NTP1Transaction>> GetAllNTP1InputsOfTx(
    CTransaction tx, CTxDB& txdb, bool recoverProtection,
    const map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>& mapQueuedNTP1Inputs,
    const map<uint256, CTxIndex>& queuedAcceptedTxs, int recursionCount)
{
    // rertrieve standard transaction inputs (NOT NTP1)
    MapPrevTx mapInputs;
    bool      fInvalid = false;
    if (!tx.FetchInputs(txdb, queuedAcceptedTxs, true, false, mapInputs, fInvalid)) {
        if (fInvalid) {
            printf("Error: For GetAllNTP1InputsOfTx, FetchInputs found invalid tx %s\n",
                   tx.GetHash().ToString().c_str());
            throw std::runtime_error("Error: For NTP1, FetchInputs found invalid tx " +
                                     tx.GetHash().ToString());
        }
    }

    std::vector<std::pair<CTransaction, NTP1Transaction>> result = StdFetchedInputTxsToNTP1(
        tx, mapInputs, txdb, recoverProtection, mapQueuedNTP1Inputs, queuedAcceptedTxs, recursionCount);
    return result;
}

void WriteNTP1TxToDiskFromRawTx(const CTransaction& tx, CTxDB& txdb)
{
    if (PassedFirstValidNTP1Tx(nBestHeight, fTestNet)) {
        // read previous transactions (inputs) which are necessary to validate an NTP1
        // transaction
        std::string opReturnArg;
        if (!IsTxNTP1(&tx, &opReturnArg)) {
            return;
        }

        std::vector<std::pair<CTransaction, NTP1Transaction>> inputsWithNTP1 =
            GetAllNTP1InputsOfTx(tx, txdb, true);

        // write NTP1 transactions' data
        NTP1Transaction ntp1tx;
        ntp1tx.readNTP1DataFromTx(tx, inputsWithNTP1);

        WriteNTP1TxToDbAndDisk(ntp1tx, txdb);
    }
}

void AssertIssuanceUniquenessInBlock(
    std::unordered_map<std::string, uint256>& issuedTokensSymbolsInThisBlock, CTxDB& txdb,
    const CTransaction&                                                        tx,
    const map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>& mapQueuedNTP1Inputs,
    const map<uint256, CTxIndex>&                                              queuedAcceptedTxs)
{
    std::string opRet;
    if (IsTxNTP1(&tx, &opRet)) {
        auto script = NTP1Script::ParseScript(opRet);
        if (script->getTxType() == NTP1Script::TxType_Issuance) {
            std::vector<std::pair<CTransaction, NTP1Transaction>> inputsTxs =
                GetAllNTP1InputsOfTx(tx, txdb, false, mapQueuedNTP1Inputs, queuedAcceptedTxs);

            NTP1Transaction ntp1tx;
            ntp1tx.readNTP1DataFromTx(tx, inputsTxs);
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

CTransaction PopLeafTransaction(std::vector<CTransaction>& vtx)
{
    if (vtx.empty()) {
        return CTransaction();
    }
    // pop one element
    CTransaction result = vtx.back();
    vtx.pop_back();

    // if any element in the array is an input to this transaction, swap them, and restart the loop
    for (int i = 0; i < static_cast<int>(vtx.size()); i++) {
        for (const CTxIn& input : result.vin) {
            assert(i >= 0);
            if (input.prevout.hash == vtx[i].GetHash()) {
                std::swap(result, vtx[i]);
                i = -1; // reset the loop
                break;
            }
        }
    }
    return result;
}

void WriteNTP1BlockTransactionsToDisk(const std::vector<CTransaction>& vtx, CTxDB& txdb)
{
    if (PassedFirstValidNTP1Tx(nBestHeight, fTestNet)) {
        std::vector<CTransaction> transactions(vtx.begin(), vtx.end());

        // add current transactions to possible inputs to cover the case if a transaction spends an
        // output in the same block
        while (!transactions.empty()) {
            CTransaction&& tx = PopLeafTransaction(transactions);
            WriteNTP1TxToDiskFromRawTx(tx, txdb);
        }
    }
}

bool CBlock::AcceptBlock()
{
    AssertLockHeld(cs_main);

    if (nVersion > CURRENT_VERSION)
        return DoS(100, error("AcceptBlock() : reject unknown block version %d", nVersion));

    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // protect against a possible attack where an attacker sends predecessors of very early blocks in the
    // blockchain, forcing a non-necessary scan of the whole blockchain
    int64_t maxCheckpointBlockHeight = Checkpoints::GetLastCheckpointBlockHeight();
    if (nBestHeight > maxCheckpointBlockHeight + 1) {
        const uint256 prevBlockHash = this->hashPrevBlock;
        auto          it            = mapBlockIndex.find(prevBlockHash);
        if (it != mapBlockIndex.cend()) {
            int64_t newBlockPrevBlockHeight = it->second->nHeight;
            if (newBlockPrevBlockHeight + 1 < maxCheckpointBlockHeight) {
                return DoS(
                    25,
                    error("Prevblock of block %s, which is %s, is behind the latest checkpoint block "
                          "height: %" PRId64 "\n",
                          this->GetHash().ToString().c_str(), prevBlockHash.ToString().c_str(),
                          maxCheckpointBlockHeight));
            }
        } else {
            return error("The prevblock of %s, which is %s, is not in the blockindex. This should never "
                         "happen in AcceptBlock(), where this error occurred\n",
                         this->GetHash().ToString().c_str(), prevBlockHash.ToString().c_str());
        }
    }

    try {
        CTxDB txdb;
        if (!VerifyInputsUnspent(txdb)) {
            return error("VerifyInputsUnspent() failed for block %s\n",
                         this->GetHash().ToString().c_str());
        }
    } catch (std::exception& ex) {
        return error("VerifyInputsUnspent() threw an exception for block %s; with error: %s\n",
                     this->GetHash().ToString().c_str(), ex.what());
    }

    // Get prev block index
    BlockIndexMapType::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found\n"));
    CBlockIndexSmartPtr pindexPrev = boost::atomic_load(&mi->second);
    int                 nHeight    = pindexPrev->nHeight + 1;

    if (IsProofOfWork() && nHeight > LAST_POW_BLOCK)
        return DoS(100, error("AcceptBlock() : reject proof-of-work at height %d", nHeight));

    // Check proof-of-work or proof-of-stake
    if (nBits != GetNextTargetRequired(pindexPrev.get(), IsProofOfStake()))
        return DoS(100, error("AcceptBlock() : incorrect %s",
                              IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));

    // Check timestamp against prev
    if (GetBlockTime() <= pindexPrev->GetPastTimeLimit() ||
        FutureDrift(GetBlockTime()) < pindexPrev->GetBlockTime())
        return error("AcceptBlock() : block's timestamp is too early");

    // Check that all transactions are finalized
    for (const CTransaction& tx : vtx)
        if (!IsFinalTx(tx, nHeight, GetBlockTime()))
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

    uint256 hashProof;
    // Verify hash target and signature of coinstake tx
    if (IsProofOfStake()) {
        uint256 targetProofOfStake;
        if (!CheckProofOfStake(vtx[1], nBits, hashProof, targetProofOfStake)) {
            printf("WARNING: AcceptBlock(): check proof-of-stake failed for block %s\n",
                   hash.ToString().c_str());
            return false; // do not error here as we expect this during initial block download
        }
    }
    // PoW is checked in CheckBlock()
    if (IsProofOfWork()) {
        hashProof = GetPoWHash();
    }

    bool cpSatisfies = Checkpoints::CheckSync(hash, pindexPrev.get());

    // Check that the block satisfies synchronized checkpoint
    if (CheckpointsMode == Checkpoints::STRICT && !cpSatisfies)
        return error("AcceptBlock() : rejected by synchronized checkpoint");

    if (CheckpointsMode == Checkpoints::ADVISORY && !cpSatisfies)
        strMiscWarning = _("WARNING: syncronized checkpoint violation detected, but skipped!");

    // Enforce rule that the coinbase starts with serialized block height
    CScript expect = CScript() << nHeight;
    if (vtx[0].vin[0].scriptSig.size() < expect.size() ||
        !std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
        return DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    uint256 nBlockPos = hash;
    if (!WriteToDisk(nBlockPos, hashProof))
        return error("AcceptBlock() : WriteToDisk failed");

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (hashBestChain == hash) {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (nBestHeight >
                (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    // ppcoin: check pending sync-checkpoint
    Checkpoints::AcceptPendingSyncCheckpoint();

    return true;
}

uint256 CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    if (bnTarget <= 0)
        return 0;

    return ((CBigNum(1) << 256) / (bnTarget + 1)).getuint256();
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired,
                                  unsigned int nToCheck)
{
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++) {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = boost::atomic_load(&pstart->pprev).get();
    }
    return (nFound >= nRequired);
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock)
{
    AssertLockHeld(cs_main);

    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight,
                     hash.ToString().c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.ToString().c_str());

    // ppcoin: check proof-of-stake
    // Limited duplicity on stake: prevents block flood attack
    // Duplicate stake allowed only when there is orphan child block
    if (pblock->IsProofOfStake() && setStakeSeen.count(pblock->GetProofOfStake()) &&
        !mapOrphanBlocksByPrev.count(hash) && !Checkpoints::WantedByPendingSyncCheckpoint(hash))
        return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for block %s",
                     pblock->GetProofOfStake().first.ToString().c_str(),
                     pblock->GetProofOfStake().second, hash.ToString().c_str());

    // Preliminary checks
    if (!pblock->CheckBlock())
        return error("ProcessBlock() : CheckBlock FAILED");

    CBlockIndex* pcheckpoint = Checkpoints::GetLastSyncCheckpoint();
    if (pcheckpoint && pblock->hashPrevBlock != hashBestChain &&
        !Checkpoints::WantedByPendingSyncCheckpoint(hash)) {
        // Extra checks to prevent "fill up memory by spamming with bogus blocks"
        int64_t deltaTime = pblock->GetBlockTime() - pcheckpoint->nTime;
        CBigNum bnNewBlock;
        bnNewBlock.SetCompact(pblock->nBits);
        CBigNum bnRequired;

        if (pblock->IsProofOfStake())
            bnRequired.SetCompact(
                ComputeMinStake(GetLastBlockIndex(pcheckpoint, true)->nBits, deltaTime, pblock->nTime));
        else
            bnRequired.SetCompact(
                ComputeMinWork(GetLastBlockIndex(pcheckpoint, false)->nBits, deltaTime));

        if (bnNewBlock > bnRequired) {
            if (pfrom)
                pfrom->Misbehaving(100);
            return error("ProcessBlock() : block with too little %s",
                         pblock->IsProofOfStake() ? "proof-of-stake" : "proof-of-work");
        }
    }

    // ppcoin: ask for pending sync-checkpoint if any
    if (!IsInitialBlockDownload())
        Checkpoints::AskForPendingSyncCheckpoint(pfrom);

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock)) {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->hashPrevBlock.ToString().c_str());
        // ppcoin: check proof-of-stake
        if (pblock->IsProofOfStake()) {
            // Limited duplicity on stake: prevents block flood attack
            // Duplicate stake allowed only when there is orphan child block
            if (setStakeSeenOrphan.count(pblock->GetProofOfStake()) &&
                !mapOrphanBlocksByPrev.count(hash) && !Checkpoints::WantedByPendingSyncCheckpoint(hash))
                return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for orphan block %s",
                             pblock->GetProofOfStake().first.ToString().c_str(),
                             pblock->GetProofOfStake().second, hash.ToString().c_str());
            else
                setStakeSeenOrphan.insert(pblock->GetProofOfStake());
        }
        CBlock* pblock2 = new CBlock(*pblock);
        mapOrphanBlocks.insert(make_pair(hash, pblock2));
        mapOrphanBlocksByPrev.insert(make_pair(pblock2->hashPrevBlock, pblock2));

        // Ask this guy to fill in what we're missing
        if (pfrom) {
            pfrom->PushGetBlocks(boost::atomic_load(&pindexBest).get(), GetOrphanRoot(pblock2));
            // ppcoin: getblocks may not obtain the ancestor block rejected
            // earlier by duplicate-stake check so we ask for it again directly
            if (!IsInitialBlockDownload())
                pfrom->AskFor(CInv(MSG_BLOCK, WantedByOrphan(pblock2)));
        }
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock())
        return error("ProcessBlock() : AcceptBlock FAILED");

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++) {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev); ++mi) {
            CBlock* pblockOrphan = (*mi).second;
            if (pblockOrphan->AcceptBlock())
                vWorkQueue.push_back(pblockOrphan->GetHash());
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            setStakeSeenOrphan.erase(pblockOrphan->GetProofOfStake());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    printf("ProcessBlock: ACCEPTED\n");

    // ppcoin: if responsible for sync-checkpoint send it
    if (pfrom && !CSyncCheckpoint::strMasterPrivKey.empty())
        Checkpoints::SendSyncCheckpoint(Checkpoints::AutoSelectSyncCheckpoint());

    return true;
}

// novacoin: attempt to generate suitable proof-of-stake
bool CBlock::SignBlock(CWallet& wallet, int64_t nFees)
{
    // if we are trying to sign
    //    something except proof-of-stake block template
    if (!vtx[0].vout[0].IsEmpty())
        return false;

    // if we are trying to sign
    //    a complete proof-of-stake block
    if (IsProofOfStake())
        return true;

    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime(); // startup timestamp

    CKey         key;
    CTransaction txCoinStake;
    int64_t      nSearchTime = txCoinStake.nTime; // search to current time

    CBlockIndexSmartPtr pindexBestPtr = boost::atomic_load(&pindexBest);
    if (nSearchTime > nLastCoinStakeSearchTime) {
        if (wallet.CreateCoinStake(wallet, nBits, nSearchTime - nLastCoinStakeSearchTime, nFees,
                                   txCoinStake, key)) {
            if (txCoinStake.nTime >=
                max(pindexBestPtr->GetPastTimeLimit() + 1, PastDrift(pindexBestPtr->GetBlockTime()))) {
                // make sure coinstake would meet timestamp protocol
                //    as it would be the same as the block timestamp
                vtx[0].nTime = nTime = txCoinStake.nTime;
                nTime = max(pindexBestPtr->GetPastTimeLimit() + 1, GetMaxTransactionTime());
                nTime = max(GetBlockTime(), PastDrift(pindexBestPtr->GetBlockTime()));

                // we have to make sure that we have no future timestamps in
                //    our transactions set
                for (vector<CTransaction>::iterator it = vtx.begin(); it != vtx.end();)
                    if (it->nTime > nTime) {
                        it = vtx.erase(it);
                    } else {
                        ++it;
                    }

                vtx.insert(vtx.begin() + 1, txCoinStake);
                hashMerkleRoot = BuildMerkleTree();

                // append a signature to our block
                return key.Sign(GetHash(), vchBlockSig);
            }
        }
        nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
        nLastCoinStakeSearchTime     = nSearchTime;
    }

    return false;
}

bool CBlock::CheckBlockSignature() const
{
    if (IsProofOfWork())
        return vchBlockSig.empty();

    vector<valtype> vSolutions;
    txnouttype      whichType;

    const CTxOut& txout = vtx[1].vout[1];

    if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY) {
        valtype& vchPubKey = vSolutions[0];
        CKey     key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        if (vchBlockSig.empty())
            return false;
        return key.Verify(GetHash(), vchBlockSig);
    }

    return false;
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
        if (filter.IsRelevantAndUpdate(block.vtx[i], hash)) {
            vMatch.push_back(true);
            vMatchedTxn.push_back(make_pair(i, hash));
        } else
            vMatch.push_back(false);
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
    unsigned int nSizeLimit = MaxBlockSize();
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

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes) {
        fShutdown         = true;
        string strMessage = _("Warning: Disk space is low!");
        strMiscWarning    = strMessage;
        printf("*** %s\n", strMessage.c_str());
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

    CBigNum bnTrustedModulus;

    if (fTestNet) {
        pchMessageStart[0] = 0x1b;
        pchMessageStart[1] = 0xba;
        pchMessageStart[2] = 0x63;
        pchMessageStart[3] = 0xc5;

        bnTrustedModulus.SetHex(
            "bee2a4e394e8d268702b94138c5659130ac83b6d93fe6940cb0738384b18366ce1f3ca05624c3dbd89f8eac83d5"
            "f95706a26faeff38efc560f0bf22d31a9828d454a79a35b5abf892635f37637fba3c0358df3fe204066e42075ae"
            "079f45296c520b942dfbb030c17c95da6ac60870a614df5def2324f710a61df35d83993f3cc38b7252a79732282"
            "b7ae12fe5edfcdb87f0e980d1b1dc0d1881708f2ff95f416c339b1ff513bf70555df587b98dfd7a122c9bb1e7ac"
            "81b665101f23f172a1c2159d630f429934abcb41c7659ff86a862b39086f4bf8263ae52d6e3c21ff92fd5d39841"
            "97b5683fb41c3bdbc8aa07e5db0041dce17b2bd8b929d09c0d3af58bc6920cfa55b187cc6486d805ed8c2442506"
            "37eab0f7e8143f0af6b2f6a9e7a7253e8fd805ae5eab3b4540b0ec6768ec883ee38ae57e8e4e023f35bd640d914"
            "82d2e6345b6c598e1d78a7a34b8235288fae59f928f820e69badaf98fa15ff1ae53a7a9d158f5c323a3bdef2227"
            "f0138c1e2fe701d2d152905f48301c3b83e130dd");
    } else {
        bnTrustedModulus.SetHex(
            "db36c560d3c006b250bd1f966ada42bca5f648c1865a76d0f20996b0ea9cc243a43d46305929cc77c4381e3eb11"
            "dea3627d32322e2f04df35a094c06dcba7b6b19af5be903be76c156661ca9d83c69a8db9281296713fe6e20393e"
            "a527a5a2cd5743d8a4e004344ad5fba4095634d084af0a077e205679f89520af9345a0167be935a77add6682e55"
            "ab506f64c72a7559ef77bb7b72d6b9ac5646c2c5efad66f2bd6150c15251260812cd2bcfe96d32dfc17c7106042"
            "40912beb96cfeece138003e1788891ad7d166afd21c753fdbe5e5f2ca963c510b8fd050ac6ddcfd0eafb5f13c66"
            "7d75d274b27958931dfed5ab36d9b7b33f4ec8c239d3632574bd2036c0cf3b58d6ad0b9d139d068a2d39b290076"
            "8f7fe45f1c871ed92dd49d4ce36046d5294914e7a1755bcc391fb09f3b858a60da95a75064ac0d0cbd1dea45259"
            "9d0f64843bf86c56ea6f2c6c6ba8a703dbefaf7c3b720fc18b04031c8827e4545dfc5e5c82b0ea81c60355d3036"
            "62d0b7d242cb527a022c63c2949f4c360cbbeb45");
    }

#if 0
    // Set up the Zerocoin Params object
    ZCParams = new libzerocoin::Params(bnTrustedModulus);
#endif

    //
    // Load block index
    //
    CTxDB txdb("cr+");
    if (!txdb.LoadBlockIndex())
        return false;

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty()) {
        if (!fAllowNew)
            return false;

        // Genesis block

        // block.GetHash() == 7286972be4dbc1463d256049b7471c252e6557e222cab9be73181d359cd28bcc
        // block.hashMerkleRoot == 203fd13214321a12b01c0d8b32c780977cf52e56ae35b7383cd389c73291aee7
        // block.nTime = 1500674579
        // block.nNonce = 8485

        const char*  pszTimestamp = "21jul2017 - Neblio First Net Launches";
        CTransaction txNew;
        txNew.nTime = 1500674579;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 0 << CBigNum(42)
                                           << vector<unsigned char>((const unsigned char*)pszTimestamp,
                                                                    (const unsigned char*)pszTimestamp +
                                                                        strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock  = 0;
        block.hashMerkleRoot = block.BuildMerkleTree();
        block.nVersion       = 1;
        block.nTime          = 1500674579;
        block.nBits          = bnProofOfWorkLimit.GetCompact();
        block.nNonce         = !fTestNet ? 8485 : 8485;

        if (true && (block.GetHash() != hashGenesisBlock)) {

            // This will figure out a valid hash and Nonce if you're
            // creating a different genesis block:
            uint256 hashTarget = CBigNum().SetCompact(block.nBits).getuint256();
            while (block.GetHash() > hashTarget) {
                ++block.nNonce;
                if (block.nNonce == 0) {
                    printf("NONCE WRAPPED, incrementing time");
                    ++block.nTime;
                }
            }
        }

        //// debug print
        block.print();

        printf("block.GetHash() == %s\n", block.GetHash().ToString().c_str());
        printf("block.hashMerkleRoot == %s\n", block.hashMerkleRoot.ToString().c_str());
        printf("block.nTime = %u \n", block.nTime);
        printf("block.nNonce = %u \n", block.nNonce);

        assert(block.hashMerkleRoot ==
               uint256("0x203fd13214321a12b01c0d8b32c780977cf52e56ae35b7383cd389c73291aee7"));
        assert(block.GetHash() == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
        assert(block.CheckBlock());

        // Start new block file
        if (!block.WriteToDisk(hashGenesisBlock, hashGenesisBlock))
            return error("LoadBlockIndex() : writing genesis block to disk failed");

        // ppcoin: initialize synchronized checkpoint
        if (!Checkpoints::WriteSyncCheckpoint((!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet)))
            return error("LoadBlockIndex() : failed to init sync checkpoint");
    }

    string strPubKey = "";

    // if checkpoint master key changed must reset sync-checkpoint
    if (!txdb.ReadCheckpointPubKey(strPubKey) || strPubKey != CSyncCheckpoint::strMasterPubKey) {
        // write checkpoint master key to db
        txdb.TxnBegin();
        if (!txdb.WriteCheckpointPubKey(CSyncCheckpoint::strMasterPubKey))
            return error("LoadBlockIndex() : failed to write new checkpoint master key to db");
        if (!txdb.TxnCommit())
            return error("LoadBlockIndex() : failed to commit new checkpoint master key to db");
        if ((!fTestNet) && !Checkpoints::ResetSyncCheckpoint())
            return error("LoadBlockIndex() : failed to reset sync-checkpoint");
    }

    return true;
}

void PrintBlockTree()
{
    AssertLockHeld(cs_main);
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndexSmartPtr>> mapNext;
    for (BlockIndexMapType::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi) {
        CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
        mapNext[boost::atomic_load(&pindex->pprev).get()].push_back(pindex);
        // test
        // while (rand() % 3 == 0)
        //    mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndexSmartPtr>> vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty()) {
        int                 nCol   = vStack.back().first;
        CBlockIndexSmartPtr pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        if (nCol > nPrevCol) {
            for (int i = 0; i < nCol - 1; i++)
                printf("| ");
            printf("|\\\n");
        } else if (nCol < nPrevCol) {
            for (int i = 0; i < nCol; i++)
                printf("| ");
            printf("|\n");
        }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            printf("| ");

        // print item
        CBlock block;
        block.ReadFromDisk(pindex.get());
        printf("%d (%s) %s  %08x  %s  mint %7s  tx %" PRIszu "", pindex->nHeight,
               pindex->blockKeyInDB.ToString().c_str(), block.GetHash().ToString().c_str(), block.nBits,
               DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str(),
               FormatMoney(pindex->nMint).c_str(), block.vtx.size());

        PrintWallets(block);

        // put the main time-chain first
        vector<CBlockIndexSmartPtr>& vNext = mapNext[pindex.get()];
        for (unsigned int i = 0; i < vNext.size(); i++) {
            if (vNext[i]->pnext) {
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
    int64_t      nStart     = GetTimeMillis();
    unsigned int nSizeLimit = MaxBlockSize();

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
                    void* nFind =
                        memchr(pchData, pchMessageStart[0], nRead + 1 - sizeof(pchMessageStart));
                    if (nFind) {
                        if (memcmp(nFind, pchMessageStart, sizeof(pchMessageStart)) == 0) {
                            nPos += ((unsigned char*)nFind - pchData) + sizeof(pchMessageStart);
                            break;
                        }
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                    } else
                        nPos += sizeof(pchData) - sizeof(pchMessageStart) + 1;
                } while (!fRequestShutdown && !fShutdown);
                if (nPos == (unsigned int)-1)
                    break;
                fseek(blkdat, nPos, SEEK_SET);
                unsigned int nSize;
                blkdat >> nSize;
                if (nSize > 0 && nSize <= nSizeLimit) {
                    CBlock block;
                    blkdat >> block;
                    LOCK(cs_main);
                    if (ProcessBlock(NULL, &block)) {
                        nLoaded++;
                        nPos += 4 + nSize;
                    }
                }
            }
        } catch (std::exception& e) {
            printf("%s() : Deserialize or I/O error caught during load\n", __PRETTY_FUNCTION__);
        }
    }
    printf("Loaded %i blocks from external file in %" PRId64 "ms\n", nLoaded, GetTimeMillis() - nStart);
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

void ThreadImport(void* data)
{
    std::vector<boost::filesystem::path>* vFiles =
        reinterpret_cast<std::vector<boost::filesystem::path>*>(data);

    RenameThread("bitcoin-loadblk");

    CImportingNow imp;
    vnThreadsRunning[THREAD_IMPORT]++;

    // -loadblock=
    // uiInterface.InitMessage(_("Starting block import..."));
    for (boost::filesystem::path& path : *vFiles) {
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

    delete vFiles;

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

    // if detected invalid checkpoint enter safe mode
    if (Checkpoints::hashInvalidCheckpoint != 0) {
        nPriority    = 3000;
        strStatusBar = strRPC = _("WARNING: Invalid checkpoint found! Displayed transactions may not be "
                                  "correct! You may need to upgrade, or notify developers.");
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

bool static AlreadyHave(CTxDB& txdb, const CInv& inv)
{
    switch (inv.type) {
    case MSG_TX: {
        bool txInMap = false;
        txInMap      = mempool.exists(inv.hash);
        return txInMap || mapOrphanTransactions.count(inv.hash) || txdb.ContainsTx(inv.hash);
    }

    case MSG_BLOCK:
        return mapBlockIndex.count(inv.hash) || mapOrphanBlocks.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}

// The message start string is designed to be unlikely to occur in normal data.
// The characters are rarely used upper ASCII, not valid as UTF-8, and produce
// a large 4-byte int at any alignment.
unsigned char pchMessageStart[4] = {0x32, 0x5e, 0x6f, 0x86};

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    static map<CService, CPubKey> mapReuseKey;
    RandAddSeedPerfmon();
    if (fDebug)
        printf("received: %s (%" PRIszu " bytes)\n", strCommand.c_str(), vRecv.size());
    std::string dropMessageTestVal;
    bool        dropMessageTestExists = mapArgs.get("-dropmessagestest", dropMessageTestVal);
    if (dropMessageTestExists && GetRand(atoi(dropMessageTestVal)) == 0) {
        printf("dropmessagestest DROPPING RECV MESSAGE\n");
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
        int minPeerVer = MinPeerVersion();
        if (pfrom->nVersion < minPeerVer) {
            // disconnect from peers older than this proto version
            printf("partner %s using obsolete version %i; disconnecting\n",
                   pfrom->addr.ToString().c_str(), pfrom->nVersion);
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
            printf("connected to self at %s, disconnecting\n", pfrom->addr.ToString().c_str());
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
            if (!fNoListen && !IsInitialBlockDownload()) {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable())
                    pfrom->PushAddress(addr);
            }

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.get().size() < 1000) {
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
        static int nAskedForBlocks = 0;
        if (!pfrom->fClient && !pfrom->fOneShot && !fImporting &&
            (pfrom->nStartingHeight > (nBestHeight - 144)) &&
            (pfrom->nVersion < NOBLKS_VERSION_START || pfrom->nVersion >= NOBLKS_VERSION_END) &&
            (nAskedForBlocks < 1 || vNodes.size() <= 1)) {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(pindexBest.get(), uint256(0));
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            for (PAIRTYPE(const uint256, CAlert) & item : mapAlerts)
                item.second.RelayTo(pfrom);
        }

        // Relay sync-checkpoint
        {
            LOCK(Checkpoints::cs_hashSyncCheckpoint);
            if (!Checkpoints::checkpointMessage.IsNull())
                Checkpoints::checkpointMessage.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        printf("receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s\n",
               pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString().c_str(),
               addrFrom.ToString().c_str(), pfrom->addr.ToString().c_str());

        cPeerBlockCounts.input(pfrom->nStartingHeight);

        // ppcoin: ask for pending sync-checkpoint if any
        if (!IsInitialBlockDownload())
            Checkpoints::AskForPendingSyncCheckpoint(pfrom);
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
            return error("message addr size() = %" PRIszu "", vAddr.size());
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
            return error("message inv size() = %" PRIszu "", vInv.size());
        }

        // find last block in inv vector
        unsigned int nLastBlock = (unsigned int)(-1);
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK) {
                nLastBlock = vInv.size() - 1 - nInv;
                break;
            }
        }
        CTxDB txdb("r");
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            const CInv& inv = vInv[nInv];

            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(txdb, inv);
            if (fDebug)
                printf("  got inventory: %s  %s\n", inv.ToString().c_str(),
                       fAlreadyHave ? "have" : "new");

            if (!fAlreadyHave) {
                if (!fImporting)
                    pfrom->AskFor(inv);
            } else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                pfrom->PushGetBlocks(pindexBest.get(), GetOrphanRoot(mapOrphanBlocks[inv.hash]));
            } else if (nInv == nLastBlock) {
                // In case we are on a very long side-chain, it is possible that we already have
                // the last block in an inv bundle sent in response to getblocks. Try to detect
                // this situation and push another getblocks to continue.
                pfrom->PushGetBlocks(boost::atomic_load(&mapBlockIndex[inv.hash]).get(), uint256(0));
                if (fDebug)
                    printf("force request: %s\n", inv.ToString().c_str());
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }

    else if (strCommand == "getdata") {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ) {
            pfrom->Misbehaving(20);
            return error("message getdata size() = %" PRIszu "", vInv.size());
        }

        if (fDebugNet || (vInv.size() != 1))
            printf("received getdata (%" PRIszu " invsz)\n", vInv.size());

        for (const CInv& inv : vInv) {
            if (fShutdown)
                return true;
            if (fDebugNet || (vInv.size() == 1))
                printf("received getdata for: %s\n", inv.ToString().c_str());

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK) {
                // Send block from disk
                BlockIndexMapType::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end()) {
                    CBlock block;
                    block.ReadFromDisk(boost::atomic_load(&mi->second).get());
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
                        vector<CInv> vInv;
                        vInv.push_back(
                            CInv(MSG_BLOCK, GetLastBlockIndex(pindexBest.get(), false)->GetBlockHash()));
                        pfrom->PushMessage("inv", vInv);
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

        // Find the last block the caller has in the main chain
        CBlockIndexSmartPtr pindex = locator.GetBlockIndex();

        // Send the rest of the chain
        if (pindex)
            pindex = boost::atomic_load(&pindex->pnext);
        int nLimit = 500;
        printf("getblocks %d to %s limit %d\n", (pindex ? pindex->nHeight : -1),
               hashStop.ToString().c_str(), nLimit);
        for (; pindex; pindex = pindex->pnext) {
            if (pindex->GetBlockHash() == hashStop) {
                printf("  getblocks stopping at %d %s\n", pindex->nHeight,
                       pindex->GetBlockHash().ToString().c_str());
                unsigned int nSMA = StakeMinAge();
                // ppcoin: tell downloading node about the latest block if it's
                // without risk being rejected due to stake connection check
                if (hashStop != hashBestChain &&
                    pindex->GetBlockTime() + nSMA > boost::atomic_load(&pindexBest)->GetBlockTime())
                    pfrom->PushInventory(CInv(MSG_BLOCK, hashBestChain));
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0) {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                printf("  getblocks stopping at limit %d %s\n", pindex->nHeight,
                       pindex->GetBlockHash().ToString().c_str());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    } else if (strCommand == "checkpoint") {
        CSyncCheckpoint checkpoint;
        vRecv >> checkpoint;

        if (checkpoint.ProcessSyncCheckpoint(pfrom)) {
            // Relay
            pfrom->hashCheckpointKnown = checkpoint.hashCheckpoint;
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
                checkpoint.RelayTo(pnode);
        }
    }

    else if (strCommand == "getheaders") {
        CBlockLocator locator;
        uint256       hashStop;
        vRecv >> locator >> hashStop;

        CBlockIndexSmartPtr pindex = NULL;
        if (locator.IsNull()) {
            // If locator is null, return the hashStop block
            BlockIndexMapType::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
        } else {
            // Find the last block the caller has in the main chain
            pindex = locator.GetBlockIndex();
            if (pindex)
                pindex = boost::atomic_load(&pindex->pnext);
        }

        vector<CBlock> vHeaders;
        int            nLimit = 2000;
        printf("getheaders %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().c_str());
        for (; pindex; pindex = pindex->pnext) {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
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

        bool fMissingInputs = false;
        if (AcceptToMemoryPool(mempool, tx, &fMissingInputs)) {
            SyncWithWallets(tx, NULL, true);
            RelayTransaction(tx, inv.hash);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (unsigned int i = 0; i < vWorkQueue.size(); i++) {
                uint256 hashPrev = vWorkQueue[i];
                for (set<uint256>::iterator mi = mapOrphanTransactionsByPrev[hashPrev].begin();
                     mi != mapOrphanTransactionsByPrev[hashPrev].end(); ++mi) {
                    const uint256& orphanTxHash    = *mi;
                    CTransaction&  orphanTx        = mapOrphanTransactions[orphanTxHash];
                    bool           fMissingInputs2 = false;

                    if (AcceptToMemoryPool(mempool, orphanTx, &fMissingInputs2)) {
                        printf("   accepted orphan tx %s\n", orphanTxHash.ToString().c_str());
                        SyncWithWallets(tx, NULL, true);
                        RelayTransaction(orphanTx, orphanTxHash);
                        mapAlreadyAskedFor.erase(CInv(MSG_TX, orphanTxHash));
                        vWorkQueue.push_back(orphanTxHash);
                        vEraseQueue.push_back(orphanTxHash);
                    } else if (!fMissingInputs2) {
                        // invalid orphan
                        vEraseQueue.push_back(orphanTxHash);
                        printf("   removed invalid orphan tx %s\n", orphanTxHash.ToString().c_str());
                    }
                }
            }

            for (uint256 hash : vEraseQueue)
                EraseOrphanTx(hash);
        } else if (fMissingInputs) {
            AddOrphanTx(tx);

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nEvicted = LimitOrphanTxSize(MAX_ORPHAN_TRANSACTIONS);
            if (nEvicted > 0)
                printf("mapOrphan overflow, removed %u tx\n", nEvicted);
        }
        if (tx.nDoS)
            pfrom->Misbehaving(tx.nDoS);
    }

    else if (strCommand == "block") {
        CBlock block;
        vRecv >> block;
        uint256 hashBlock = block.GetHash();

        printf("received block %s\n", hashBlock.ToString().c_str());

        CInv inv(MSG_BLOCK, hashBlock);
        pfrom->AddInventoryKnown(inv);

        if (ProcessBlock(pfrom, &block))
            mapAlreadyAskedFor.erase(inv);
        if (block.nDoS)
            pfrom->Misbehaving(block.nDoS);
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
            CInv inv(MSG_TX, hash);
            if ((pfrom->pfilter && pfrom->pfilter->IsRelevantAndUpdate(mempool.lookup(hash), hash)) ||
                (!pfrom->pfilter))
                vInv.push_back(inv);
            if (vInv.size() == MAX_INV_SZ)
                break;
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }

    else if (strCommand == "checkorder") {
        uint256 hashReply;
        vRecv >> hashReply;

        if (!GetBoolArg("-allowreceivebyip")) {
            pfrom->PushMessage("reply", hashReply, (int)2, string(""));
            return true;
        }

        CWalletTx order;
        vRecv >> order;

        /// we have a chance to check the order here

        // Keep giving the same key to the same ip until they use it
        if (!mapReuseKey.count(pfrom->addr))
            pwalletMain->GetKeyFromPool(mapReuseKey[pfrom->addr], true);

        // Send back approval of order and pubkey to use
        CScript scriptPubKey;
        scriptPubKey << mapReuseKey[pfrom->addr] << OP_CHECKSIG;
        pfrom->PushMessage("reply", hashReply, (int)0, scriptPubKey);
    }

    else if (strCommand == "reply") {
        uint256 hashReply;
        vRecv >> hashReply;

        CRequestTracker tracker;
        {
            LOCK(pfrom->cs_mapRequests);
            map<uint256, CRequestTracker>::iterator mi = pfrom->mapRequests.find(hashReply);
            if (mi != pfrom->mapRequests.end()) {
                tracker = (*mi).second;
                pfrom->mapRequests.erase(mi);
            }
        }
        if (!tracker.IsNull())
            tracker.fn(tracker.param1, vRecv);
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
        pfrom->pfilter    = NULL;
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
    // if (fDebug)
    //    printf("ProcessMessages(%zu messages)\n", pfrom->vRecvMsg.size());

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

        // if (fDebug)
        //    printf("ProcessMessages(message %u msgsz, %zu bytes, complete:%s)\n",
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, pchMessageStart, sizeof(pchMessageStart)) != 0) {
            printf("\n\nPROCESSMESSAGE: INVALID MESSAGESTART\n\n");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid()) {
            printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
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
            printf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
                   strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
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
            if (strstr(e.what(), "end of data")) {
                // Allow exceptions from under-length message on vRecv
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a "
                       "message being shorter than its stated length\n",
                       strCommand.c_str(), nMessageSize, e.what());
            } else if (strstr(e.what(), "size too large")) {
                // Allow exceptions from over-long size
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught\n", strCommand.c_str(),
                       nMessageSize, e.what());
            } else {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        } catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            printf("ProcessMessage(%s, %u bytes) FAILED\n", strCommand.c_str(), nMessageSize);
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
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60)) {
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
        CTxDB        txdb("r");
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow) {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(txdb, inv)) {
                if (fDebugNet)
                    printf("sending getdata: %s\n", inv.ToString().c_str());
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

bool EnableEnforceUniqueTokenSymbols()
{
    //    if (PassedFirstValidNTP1Tx(nBestHeight, isTestnet)) {
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
        return true;
    } else {
        return false;
    }
}

bool PassedFirstValidNTP1Tx(const int bestHeight, const bool isTestnet)
{
    if (isTestnet) {
        // testnet past network upgrade block
        return (bestHeight >= 10313);
    } else {
        // mainnet past first valid NTP1 txn
        return (bestHeight >= 157528);
    }
}

/** Maximum size of a block */
unsigned int MaxBlockSize()
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
        return MAX_BLOCK_SIZE;
    } else {
        return OLD_MAX_BLOCK_SIZE;
    }
}

/** Spacing between blocks */
unsigned int TargetSpacing()
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
        return nTargetSpacing;
    } else {
        return nOldTargetSpacing;
    }
}

/** Coinbase Maturity */
int CoinbaseMaturity()
{
    if (fTestNet) {
        if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
            return nCoinbaseMaturity;
        } else {
            return 10;
        }
    } else {
        if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
            return nCoinbaseMaturity;
        } else if (GetNetForks().isForkActivated(NetworkFork::NETFORK__2_CONFS_CHANGE)) {
            // return nOldCoinbaseMaturity;
            return 10; // testnet maturity is 10, mainnet will be 30
        } else {
            return nOldCoinbaseMaturity;
        }
    }
}

/** Max OP_RETURN Size */
unsigned int DataSize()
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
        return MAX_DATA_SIZE;
    } else {
        return OLD_MAX_DATA_SIZE;
    }
}

/** Minimum Peer Version */
int MinPeerVersion()
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__4_RETARGET_CORRECTION)) {
        return MIN_PEER_PROTO_VERSION;
    } else {
        return OLD_MIN_PEER_PROTO_VERSION;
    }
}

/** Minimum Staking Age */
unsigned int StakeMinAge()
{
    if (fTestNet) {
        if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
            return nStakeMinAge;
        } else {
            return nOldTestnetStakeMinAge;
        }
    } else {
        return nStakeMinAge;
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

bool TxContainsOpReturn(const CTransaction* tx, std::string* opReturnArg)
{
    if (!tx) {
        return false;
    }

    boost::smatch opReturnArgMatch;

    for (unsigned long j = 0; j < tx->vout.size(); j++) {
        // if the string OP_RET_STR is found in scriptPubKey
        std::string scriptPubKeyStr = tx->vout[j].scriptPubKey.ToString();
        if (boost::regex_match(scriptPubKeyStr, opReturnArgMatch, OpReturnRegex)) {
            if (opReturnArg != nullptr && opReturnArgMatch[1].matched) {
                *opReturnArg = std::string(opReturnArgMatch[1]);
                return true;
            }
            return true; // could not retrieve OP_RETURN argument
        }
    }
    return false;
}

bool IsTxNTP1(const CTransaction* tx, std::string* opReturnArg)
{
    if (!tx) {
        return false;
    }

    if (IsNTP1TxExcluded(tx->GetHash())) {
        return false;
    }

    boost::smatch opReturnArgMatch;

    for (unsigned long j = 0; j < tx->vout.size(); j++) {
        std::string scriptPubKeyStr = tx->vout[j].scriptPubKey.ToString();
        if (boost::regex_match(scriptPubKeyStr, opReturnArgMatch, NTP1OpReturnRegex)) {
            if (opReturnArg != nullptr && opReturnArgMatch[1].matched) {
                *opReturnArg = std::string(opReturnArgMatch[1]);
                return true;
            }
            return true; // could not retrieve OP_RETURN argument
        }
    }
    return false;
}

bool IsTxOutputNTP1OpRet(const CTransaction* tx, unsigned int index, std::string* opReturnArg)
{
    if (!tx) {
        return false;
    }

    if (IsNTP1TxExcluded(tx->GetHash())) {
        return false;
    }

    boost::smatch opReturnArgMatch;

    // out of range index
    if (index + 1 >= tx->vout.size()) {
        return false;
    }

    std::string scriptPubKeyStr = tx->vout[index].scriptPubKey.ToString();
    if (boost::regex_match(scriptPubKeyStr, opReturnArgMatch, NTP1OpReturnRegex)) {
        if (opReturnArg != nullptr && opReturnArgMatch[1].matched) {
            *opReturnArg = std::string(opReturnArgMatch[1]);
            return true;
        }
        return true; // could not retrieve OP_RETURN argument
    }
    return false;
}

bool IsTxOutputOpRet(const CTransaction* tx, unsigned int index, std::string* opReturnArg)
{
    if (!tx) {
        return false;
    }

    boost::smatch opReturnArgMatch;

    // out of range index
    if (index + 1 >= tx->vout.size()) {
        return false;
    }

    std::string scriptPubKeyStr = tx->vout[index].scriptPubKey.ToString();
    if (boost::regex_match(scriptPubKeyStr, opReturnArgMatch, OpReturnRegex)) {
        if (opReturnArg != nullptr && opReturnArgMatch[1].matched) {
            *opReturnArg = std::string(opReturnArgMatch[1]);
            return true;
        }
        return true; // could not retrieve OP_RETURN argument
    }
    return false;
}

bool IsTxOutputOpRet(const CTxOut* output, string* opReturnArg)
{
    if (!output) {
        return false;
    }

    boost::smatch opReturnArgMatch;

    std::string scriptPubKeyStr = output->scriptPubKey.ToString();
    if (boost::regex_match(scriptPubKeyStr, opReturnArgMatch, OpReturnRegex)) {
        if (opReturnArg != nullptr && opReturnArgMatch[1].matched) {
            *opReturnArg = std::string(opReturnArgMatch[1]);
            return true;
        }
        return true; // could not retrieve OP_RETURN argument
    }
    return false;
}

bool CBlock::WriteToDisk(const uint256& nBlockPos, const uint256& hashProof)
{
    /**
     * @brief txdb
     * This function writes a whole block in an ACID transaction
     */

    CTxDB       txdb;
    std::size_t req_size = 500 * ::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION);
    if (!txdb.TxnBegin(req_size)) {
        printf("Failed to start transaction for writing a new block.");
        return false;
    }

    bool success = false;

    // this is an RAII hack to guarantee that the function will commit/abort the transaction on exit
    auto txReverseFunctor = [&txdb, &success](bool*) {
        if (success) {
            txdb.TxnCommit();
        } else {
            txdb.TxnAbort();
        }
    };
    std::unique_ptr<bool, decltype(txReverseFunctor)> txEnder(&success, txReverseFunctor);

    if (!txdb.WriteBlock(this->GetHash(), *this)) {
        return false;
    }

    CBlockIndexSmartPtr pindexNew = nullptr;

    if (!AddToBlockIndex(nBlockPos, hashProof, txdb, &pindexNew, false)) {
        return error("AcceptBlock() : AddToBlockIndex failed");
    }

    if (!pindexNew) {
        return error("Major Error: A nullptr CBlockIndex() was found in CBlock::WriteToDisk(), although "
                     "AddToBlockIndex() succeeded. This should never happen!");
    }

    success = true;
    txEnder.reset();
    return true;
}

bool CBlock::ReadFromDisk(const uint256& hash, bool fReadTransactions)
{
    SetNull();
    return CTxDB().ReadBlock(hash, *this, fReadTransactions);
}

bool CBlock::ReadFromDisk(const uint256& hash, CTxDB& txdb, bool fReadTransactions)
{
    SetNull();
    return txdb.ReadBlock(hash, *this, fReadTransactions);
}

bool IsTxInMainChain(const uint256& txHash)
{
    CTransaction tx;
    uint256      hashBlock;
    if (GetTransaction(txHash, tx, hashBlock)) {
        BlockIndexMapType::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
            return pindex->IsInMainChain();
        } else {
            throw std::runtime_error("Unable to find the block that has the transaction " +
                                     txHash.ToString());
        }
    }
    throw std::runtime_error("Unable to retrieve the transaction " + txHash.ToString());
}

int64_t GetTxBlockHeight(const uint256& txHash)
{
    CTransaction tx;
    uint256      hashBlock;
    if (GetTransaction(txHash, tx, hashBlock)) {
        BlockIndexMapType::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndexSmartPtr pindex = boost::atomic_load(&mi->second);
            if (pindex->IsInMainChain()) {
                return static_cast<int64_t>(pindex->nHeight);
            } else {
                return static_cast<int64_t>(-1);
            }
        } else {
            throw std::runtime_error("Unable to find the block that has the transaction " +
                                     txHash.ToString());
        }
    }
    throw std::runtime_error("Unable to retrieve the transaction " + txHash.ToString());
}

void ExportBootstrapBlockchain(const string& filename, std::atomic<bool>& stopped,
                               std::atomic<double>& progress, boost::promise<void>& result)
{
    RenameThread("Export-blockchain");
    try {
        progress.store(0, std::memory_order_relaxed);

        std::vector<CBlockIndex*> chainBlocksIndices;

        {
            CBlockIndex* pblockindex = boost::atomic_load(&mapBlockIndex[hashBestChain]).get();
            chainBlocksIndices.push_back(pblockindex);
            while (pblockindex->nHeight > 0 && !stopped.load() && !fShutdown) {
                pblockindex = boost::atomic_load(&pblockindex->pprev).get();
                chainBlocksIndices.push_back(pblockindex);
            }
        }

        if (stopped.load() || fShutdown) {
            throw std::runtime_error("Operation was stopped.");
        }

        std::ofstream outFile(filename.c_str(), ios::binary);
        if (!outFile.good()) {
            throw std::runtime_error("Failed to open file for writing. Make sure you have sufficient "
                                     "permissions and diskspace.");
        }

        size_t threadsholdSize = 1 << 24; // 4 MB

        CDataStream  serializedBlocks(SER_DISK, CLIENT_VERSION);
        size_t       written = 0;
        const size_t total   = chainBlocksIndices.size();
        for (CBlockIndex* blockIndex : boost::adaptors::reverse(chainBlocksIndices)) {
            progress.store(static_cast<double>(written) / static_cast<double>(total),
                           std::memory_order_relaxed);
            if (stopped.load() || fShutdown) {
                throw std::runtime_error("Operation was stopped.");
            }
            CBlock block;
            block.ReadFromDisk(blockIndex, true);

            // every block starts with pchMessageStart
            unsigned int nSize = block.GetSerializeSize(SER_DISK, CLIENT_VERSION);
            serializedBlocks << FLATDATA(pchMessageStart) << nSize;
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

std::pair<BlockIndexGraphType, VerticesDescriptorsMapType>
GetBlockIndexAsGraph(const BlockIndexMapType& BlockIndex = mapBlockIndex)
{
    BlockIndexGraphType graph;

    // copy block index to avoid conflicts
    const BlockIndexMapType tempBlockIndex = BlockIndex;

    VerticesDescriptorsMapType verticesDescriptors;

    // add all vertices, which are block hashes
    for (const auto& bi : tempBlockIndex) {
        verticesDescriptors[bi.first] = boost::add_vertex(bi.first, graph);
    }

    // add edges, which are previous blocks connected to subsequent blocks
    for (const auto& bi : tempBlockIndex) {
        if (bi.first != hashGenesisBlock && bi.first != hashGenesisBlockTestNet) {
            boost::add_edge(verticesDescriptors.at(*bi.second->pprev->phashBlock),
                            verticesDescriptors.at(bi.first), graph);
        }
    }
    return std::make_pair(graph, verticesDescriptors);
}

std::deque<uint256> TraverseBlockIndexGraph(const BlockIndexGraphType&        graph,
                                            const VerticesDescriptorsMapType& descriptors,
                                            GraphTraverseType                 traverseType)
{
    uint256 startBlockHash = (fTestNet ? hashGenesisBlockTestNet : hashGenesisBlock);

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

void ExportBootstrapBlockchainWithOrphans(const string& filename, std::atomic<bool>& stopped,
                                          std::atomic<double>& progress, boost::promise<void>& result,
                                          GraphTraverseType traverseType)
{
    RenameThread("Export-blockchain");
    try {
        progress.store(0, std::memory_order_relaxed);

        BlockIndexGraphType        graph;
        VerticesDescriptorsMapType verticesDescriptors;
        std::tie(graph, verticesDescriptors) = GetBlockIndexAsGraph(mapBlockIndex);

        std::deque<uint256> blocksHashes =
            TraverseBlockIndexGraph(graph, verticesDescriptors, traverseType);

        if (stopped.load() || fShutdown) {
            throw std::runtime_error("Operation was stopped.");
        }

        std::ofstream outFile(filename.c_str(), ios::binary);
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
            block.ReadFromDisk(h, true);

            // every block starts with pchMessageStart
            unsigned int nSize = block.GetSerializeSize(SER_DISK, CLIENT_VERSION);
            serializedBlocks << FLATDATA(pchMessageStart) << nSize;
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

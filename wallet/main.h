// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "bignum.h"
#include "block.h"
#include "blockindex.h"
#include "blockindexcatalog.h"
#include "globals.h"
#include "net.h"
#include "outpoint.h"
#include "script.h"
#include "scrypt.h"
#include "sync.h"
#include "transaction.h"
#include "zerocoin/Zerocoin.h"

#include <atomic>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <unordered_map>
#include <unordered_set>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/depth_first_search.hpp>

using BlockIndexVertexType = uint256;
using BlockIndexGraphType =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, BlockIndexVertexType>;
using DescriptorType             = boost::graph_traits<BlockIndexGraphType>::vertex_descriptor;
using VerticesDescriptorsMapType = std::map<uint256, DescriptorType>;

enum GraphTraverseType
{
    BreadthFirst,
    DepthFirst
};

class CWallet;
class CBlock;
class CBlockIndex;
class CKeyItem;
class CReserveKey;
class COutPoint;
class CBlockLocator;

class CAddress;
class CInv;
class CRequestTracker;
class CNode;

class CTxMemPool;

// this prevents attempting to recover NTP1 transactions again and again recursively
// once a tx is on this list, it won't be recovered
extern std::set<uint256> UnrecoverableNTP1Txs;

inline int64_t PastDrift(int64_t nTime) { return nTime - 10 * 60; }   // up to 10 minutes from the past
inline int64_t FutureDrift(int64_t nTime) { return nTime + 10 * 60; } // up to 10 minutes in the future

extern libzerocoin::Params*                         ZCParams;
extern CScript                                      COINBASE_FLAGS;
extern std::set<std::pair<COutPoint, unsigned int>> setStakeSeen;
static constexpr const int64_t                      TARGET_AVERAGE_BLOCK_COUNT = 100;
extern unsigned int                                 nTargetSpacing;
extern unsigned int                                 nStakeMinAge;
extern unsigned int                                 nOldTestnetStakeMinAge;
extern unsigned int                                 nStakeMaxAge;
extern unsigned int                                 nNodeLifespan;
extern int                                          nCoinbaseMaturity;
extern uint256                                      nBestChainTrust;
extern uint256                                      nBestInvalidTrust;
extern uint256                                      hashBestChain;
extern uint64_t                                     nLastBlockTx;
extern uint64_t                                     nLastBlockSize;
extern boost::atomic<int64_t>                       nLastCoinStakeSearchInterval;
extern const std::string                            strMessageMagic;
extern int64_t                                      nTimeBestReceived;
extern CCriticalSection                             cs_setpwalletRegistered;
extern std::set<std::shared_ptr<CWallet>>           setpwalletRegistered;
extern unsigned char                                pchMessageStart[4];
extern std::unordered_map<uint256, CBlock*>         mapOrphanBlocks;
extern boost::atomic<bool>                          fImporting;

// Settings
extern int64_t      nTransactionFee;
extern int64_t      nReserveBalance;
extern int64_t      nMinimumInputValue;
extern unsigned int nDerivationMethodIndex;

extern bool fEnforceCanonical;

class NTP1Transaction;

// Minimum disk space required - used in CheckDiskSpace()
static const uint64_t nMinDiskSpace = 52428800;

class CReserveKey;
class CTxDB;
class CTxIndex;

void         RegisterWallet(std::shared_ptr<CWallet> pwalletIn);
void         UnregisterWallet(std::shared_ptr<CWallet> pwalletIn);
void         SyncWithWallets(const CTransaction& tx, const CBlock* pblock = NULL, bool fUpdate = false,
                             bool fConnect = true);
bool         ProcessBlock(CNode* pfrom, CBlock* pblock);
bool         CheckDiskSpace(uint64_t nAdditionalBytes = 0);
bool         LoadBlockIndex(bool fAllowNew = true);
void         PrintBlockTree();
bool         ProcessMessages(CNode* pfrom);
bool         SendMessages(CNode* pto, bool fSendTrickle);
void         ThreadImport(void* parg);
bool         CheckProofOfWork(uint256 hash, unsigned int nBits);
unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake);
int64_t      GetProofOfWorkReward(int64_t nFees);
int64_t      GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees);
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime);
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int nBlockTime);
int          GetNumBlocksOfPeers();
bool         IsInitialBlockDownload();
bool         IsInitialBlockDownload_tolerant();
bool         __IsInitialBlockDownload_internal();
std::string  GetWarnings(std::string strFor);
bool         GetTransaction(const uint256& hash, CTransaction& tx, uint256& hashBlock);
uint256      WantedByOrphan(const CBlock* pblockOrphan);
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake);
void               StakeMiner(CWallet* pwallet);
void               ResendWalletTransactions(bool fForce = false);
CTransaction       FetchTxFromDisk(const uint256& txid);
CTransaction       FetchTxFromDisk(const uint256& txid, CTxDB& txdb);

void SetBestChain(const CBlockLocator& loc);

/** given a neblio tx, get the corresponding NTP1 tx */
void FetchNTP1TxFromDisk(std::pair<CTransaction, NTP1Transaction>& txPair, CTxDB& txdb,
                         bool recoverProtection, unsigned recurseDepth = 0);
void WriteNTP1TxToDbAndDisk(const NTP1Transaction& ntp1tx, CTxDB& txdb);

void WriteNTP1TxToDiskFromRawTx(const CTransaction& tx, CTxDB& txdb);

void AssertIssuanceUniquenessInBlock(
    std::unordered_map<std::string, uint256>& issuedTokensSymbolsInThisBlock, CTxDB& txdb,
    const CTransaction&                                                             tx,
    const std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>& mapQueuedNTP1Inputs,
    const std::map<uint256, CTxIndex>&                                              queuedAcceptedTxs);

void WriteNTP1BlockTransactionsToDisk(const std::vector<CTransaction>& vtx, CTxDB& txdb);

/// create a fake tx position that helps in marking an output as spent
CDiskTxPos CreateFakeSpentTxPos(const uint256& blockhash);

/** blacklisted tokens are tokens that are to be ignored and not used for historical reasons */
bool IsIssuedTokenBlacklisted(std::pair<CTransaction, NTP1Transaction>& txPair);

void AssertNTP1TokenNameIsNotAlreadyInMainChain(std::string sym, const uint256& txHash, CTxDB& txdb);
void AssertNTP1TokenNameIsNotAlreadyInMainChain(const NTP1Transaction& ntp1tx, CTxDB& txdb);

/** this function solves the problem of blocks having inputs from the same block. To process transactions
 * in such a situation (or always, to be safe), first we pop the transactions from the leaves (the
 * inputs), and then process their parents. This function pops one transaction from the leaf of a least
 * of transactions from a block */
CTransaction PopLeafTransaction(std::vector<CTransaction>& vtx);

/** True if the transaction is in the main chain (can throw) */
bool IsTxInMainChain(const uint256& txHash);

/** The number of the block where the transaction is located */
int64_t GetTxBlockHeight(const uint256& txHash);

/** (try to) add transaction to memory pool **/
bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction& tx, bool* pfMissingInputs);

bool EnableEnforceUniqueTokenSymbols();

/** the condition for the first valid NTP1 transaction; transactions before this point are invalid in the
 * network*/
bool PassedFirstValidNTP1Tx(const int bestHeight, const bool isTestnet);

/** Maximum size of a block */
unsigned int MaxBlockSize();

/** Target time between blocks */
unsigned int TargetSpacing();

/** Coinbase Maturity */
int CoinbaseMaturity();

/** max OP_RETURN size */
unsigned int DataSize();

/** Minimum Peer Protocol Version */
int MinPeerVersion();

/** Minimum Staking Age */
unsigned int StakeMinAge();

bool GetWalletFile(CWallet* pwallet, std::string& strWalletFileOut);

/** Check for standard transaction types
    @return True if all outputs (scriptPubKeys) use only standard transaction forms
*/
bool IsStandardTx(const CTransaction& tx, std::string& reason);

/** wrapper for CTxOut that provides a more compact serialization */
class CTxOutCompressor
{
private:
    CTxOut& txout;

public:
    static uint64_t CompressAmount(uint64_t nAmount);
    static uint64_t DecompressAmount(uint64_t nAmount);

    CTxOutCompressor(CTxOut& txoutIn) : txout(txoutIn) {}

    IMPLEMENT_SERIALIZE(({
                            if (!fRead) {
                                uint64_t nVal = CompressAmount(txout.nValue);
                                READWRITE(VARINT(nVal));
                            } else {
                                uint64_t nVal = 0;
                                READWRITE(VARINT(nVal));
                                txout.nValue = DecompressAmount(nVal);
                            }
                            CScriptCompressor cscript(REF(txout.scriptPubKey));
                            READWRITE(cscript);
                        });)
};

bool IsFinalTx(const CTransaction& tx, int nBlockHeight = 0, int64_t nBlockTime = 0);

/** Undo information for a CTxIn
 *
 *  Contains the prevout's CTxOut being spent, and if this was the
 *  last output of the affected transaction, its metadata as well
 *  (coinbase or not, height, transaction version)
 */
class CTxInUndo
{
public:
    CTxOut       txout;     // the txout data before being spent
    bool         fCoinBase; // if the outpoint was the last unspent: whether it belonged to a coinbase
    unsigned int nHeight;   // if the outpoint was the last unspent: its height
    int          nVersion;  // if the outpoint was the last unspent: its version

    CTxInUndo() : txout(), fCoinBase(false), nHeight(0), nVersion(0) {}
    CTxInUndo(const CTxOut& txoutIn, bool fCoinBaseIn = false, unsigned int nHeightIn = 0,
              int nVersionIn = 0)
        : txout(txoutIn), fCoinBase(fCoinBaseIn), nHeight(nHeightIn), nVersion(nVersionIn)
    {
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return ::GetSerializeSize(VARINT(nHeight * 2 + (fCoinBase ? 1 : 0)), nType, nVersion) +
               (nHeight > 0 ? ::GetSerializeSize(VARINT(this->nVersion), nType, nVersion) : 0) +
               ::GetSerializeSize(CTxOutCompressor(REF(txout)), nType, nVersion);
    }

    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        ::Serialize(s, VARINT(nHeight * 2 + (fCoinBase ? 1 : 0)), nType, nVersion);
        if (nHeight > 0)
            ::Serialize(s, VARINT(this->nVersion), nType, nVersion);
        ::Serialize(s, CTxOutCompressor(REF(txout)), nType, nVersion);
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nCode), nType, nVersion);
        nHeight   = nCode / 2;
        fCoinBase = nCode & 1;
        if (nHeight > 0)
            ::Unserialize(s, VARINT(this->nVersion), nType, nVersion);
        ::Unserialize(s, REF(CTxOutCompressor(REF(txout))), nType, nVersion);
    }
};

/** Undo information for a CTransaction */
class CTxUndo
{
public:
    std::vector<CTxInUndo> vprevout;

    IMPLEMENT_SERIALIZE(READWRITE(vprevout);)
};

/** pruned version of CTransaction: only retains metadata and unspent transaction outputs
 *
 * Serialized format:
 * - VARINT(nVersion)
 * - VARINT(nCode)
 * - unspentness bitvector, for vout[2] and further; least significant byte first
 * - the non-spent CTxOuts (via CTxOutCompressor)
 * - VARINT(nHeight)
 *
 * The nCode value consists of:
 * - bit 1: IsCoinBase()
 * - bit 2: vout[0] is not spent
 * - bit 4: vout[1] is not spent
 * - The higher bits encode N, the number of non-zero bytes in the following bitvector.
 *   - In case both bit 2 and bit 4 are unset, they encode N-1, as there must be at
 *     least one non-spent output).
 *
 * Example: 0104835800816115944e077fe7c803cfa57f29b36bf87c1d358bb85e
 *          <><><--------------------------------------------><---->
 *          |  \                  |                             /
 *    version   code             vout[1]                  height
 *
 *    - version = 1
 *    - code = 4 (vout[1] is not spent, and 0 non-zero bytes of bitvector follow)
 *    - unspentness bitvector: as 0 non-zero bytes follow, it has length 0
 *    - vout[1]: 835800816115944e077fe7c803cfa57f29b36bf87c1d35
 *               * 8358: compact amount representation for 60000000000 (600 BTC)
 *               * 00: special txout type pay-to-pubkey-hash
 *               * 816115944e077fe7c803cfa57f29b36bf87c1d35: address uint160
 *    - height = 203998
 *
 *
 * Example:
 * 0109044086ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4eebbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa486af3b
 *          <><><--><--------------------------------------------------><----------------------------------------------><---->
 *         /  \   \                     |                                                           | /
 *  version  code  unspentness       vout[4]                                                     vout[16]
 * height
 *
 *  - version = 1
 *  - code = 9 (coinbase, neither vout[0] or vout[1] are unspent,
 *                2 (1, +1 because both bit 2 and bit 4 are unset) non-zero bitvector bytes follow)
 *  - unspentness bitvector: bits 2 (0x04) and 14 (0x4000) are set, so vout[2+2] and vout[14+2] are
 * unspent
 *  - vout[4]: 86ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4ee
 *             * 86ef97d579: compact amount representation for 234925952 (2.35 BTC)
 *             * 00: special txout type pay-to-pubkey-hash
 *             * 61b01caab50f1b8e9c50a5057eb43c2d9563a4ee: address uint160
 *  - vout[16]: bbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa4
 *              * bbd123: compact amount representation for 110397 (0.001 BTC)
 *              * 00: special txout type pay-to-pubkey-hash
 *              * 8c988f1a4a4de2161e0f50aac7f17e7f9555caa4: address uint160
 *  - height = 120891
 */
class CCoins
{
public:
    // whether transaction is a coinbase
    bool fCoinBase;

    // unspent transaction outputs; spent outputs are .IsNull(); spent outputs at the end of the array
    // are dropped
    std::vector<CTxOut> vout;

    // at which height this transaction was included in the active blockchain
    int nHeight;

    // version of the CTransaction; accesses to this value should probably check for nHeight as well,
    // as new tx version will probably only be introduced at certain heights
    int nVersion;

    // construct a CCoins from a CTransaction, at a given height
    CCoins(const CTransaction& tx, int nHeightIn)
        : fCoinBase(tx.IsCoinBase()), vout(tx.vout), nHeight(nHeightIn), nVersion(tx.nVersion)
    {
    }

    // empty constructor
    CCoins() : fCoinBase(false), vout(0), nHeight(0), nVersion(0) {}

    // remove spent outputs at the end of vout
    void Cleanup()
    {
        while (vout.size() > 0 && vout.back().IsNull())
            vout.pop_back();
    }

    // equality test
    friend bool operator==(const CCoins& a, const CCoins& b)
    {
        return a.fCoinBase == b.fCoinBase && a.nHeight == b.nHeight && a.nVersion == b.nVersion &&
               a.vout == b.vout;
    }
    friend bool operator!=(const CCoins& a, const CCoins& b) { return !(a == b); }

    // calculate number of bytes for the bitmask, and its number of non-zero bytes
    // each bit in the bitmask represents the availability of one output, but the
    // availabilities of the first two outputs are encoded separately
    void CalcMaskSize(unsigned int& nBytes, unsigned int& nNonzeroBytes) const
    {
        unsigned int nLastUsedByte = 0;
        for (unsigned int b = 0; 2 + b * 8 < vout.size(); b++) {
            bool fZero = true;
            for (unsigned int i = 0; i < 8 && 2 + b * 8 + i < vout.size(); i++) {
                if (!vout[2 + b * 8 + i].IsNull()) {
                    fZero = false;
                    continue;
                }
            }
            if (!fZero) {
                nLastUsedByte = b + 1;
                nNonzeroBytes++;
            }
        }
        nBytes += nLastUsedByte;
    }

    bool IsCoinBase() const { return fCoinBase; }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        unsigned int nSize     = 0;
        unsigned int nMaskSize = 0, nMaskCode = 0;
        CalcMaskSize(nMaskSize, nMaskCode);
        bool fFirst  = vout.size() > 0 && !vout[0].IsNull();
        bool fSecond = vout.size() > 1 && !vout[1].IsNull();
        assert(fFirst || fSecond || nMaskCode);
        unsigned int nCode = 8 * (nMaskCode - (fFirst || fSecond ? 0 : 1)) + (fCoinBase ? 1 : 0) +
                             (fFirst ? 2 : 0) + (fSecond ? 4 : 0);
        // version
        nSize += ::GetSerializeSize(VARINT(this->nVersion), nType, nVersion);
        // size of header code
        nSize += ::GetSerializeSize(VARINT(nCode), nType, nVersion);
        // spentness bitmask
        nSize += nMaskSize;
        // txouts themself
        for (unsigned int i = 0; i < vout.size(); i++)
            if (!vout[i].IsNull())
                nSize += ::GetSerializeSize(CTxOutCompressor(REF(vout[i])), nType, nVersion);
        // height
        nSize += ::GetSerializeSize(VARINT(nHeight), nType, nVersion);
        return nSize;
    }

    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        unsigned int nMaskSize = 0, nMaskCode = 0;
        CalcMaskSize(nMaskSize, nMaskCode);
        bool fFirst  = vout.size() > 0 && !vout[0].IsNull();
        bool fSecond = vout.size() > 1 && !vout[1].IsNull();
        assert(fFirst || fSecond || nMaskCode);
        unsigned int nCode = 8 * (nMaskCode - (fFirst || fSecond ? 0 : 1)) + (fCoinBase ? 1 : 0) +
                             (fFirst ? 2 : 0) + (fSecond ? 4 : 0);
        // version
        ::Serialize(s, VARINT(this->nVersion), nType, nVersion);
        // header code
        ::Serialize(s, VARINT(nCode), nType, nVersion);
        // spentness bitmask
        for (unsigned int b = 0; b < nMaskSize; b++) {
            unsigned char chAvail = 0;
            for (unsigned int i = 0; i < 8 && 2 + b * 8 + i < vout.size(); i++)
                if (!vout[2 + b * 8 + i].IsNull())
                    chAvail |= (1 << i);
            ::Serialize(s, chAvail, nType, nVersion);
        }
        // txouts themself
        for (unsigned int i = 0; i < vout.size(); i++) {
            if (!vout[i].IsNull())
                ::Serialize(s, CTxOutCompressor(REF(vout[i])), nType, nVersion);
        }
        // coinbase height
        ::Serialize(s, VARINT(nHeight), nType, nVersion);
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        unsigned int nCode = 0;
        // version
        ::Unserialize(s, VARINT(this->nVersion), nType, nVersion);
        // header code
        ::Unserialize(s, VARINT(nCode), nType, nVersion);
        fCoinBase = nCode & 1;
        std::vector<bool> vAvail(2, false);
        vAvail[0]              = nCode & 2;
        vAvail[1]              = nCode & 4;
        unsigned int nMaskCode = (nCode / 8) + ((nCode & 6) != 0 ? 0 : 1);
        // spentness bitmask
        while (nMaskCode > 0) {
            unsigned char chAvail = 0;
            ::Unserialize(s, chAvail, nType, nVersion);
            for (unsigned int p = 0; p < 8; p++) {
                bool f = (chAvail & (1 << p)) != 0;
                vAvail.push_back(f);
            }
            if (chAvail != 0)
                nMaskCode--;
        }
        // txouts themself
        vout.assign(vAvail.size(), CTxOut());
        for (unsigned int i = 0; i < vAvail.size(); i++) {
            if (vAvail[i])
                ::Unserialize(s, REF(CTxOutCompressor(vout[i])), nType, nVersion);
        }
        // coinbase height
        ::Unserialize(s, VARINT(nHeight), nType, nVersion);
        Cleanup();
    }

    // mark an outpoint spent, and construct undo information
    bool Spend(const COutPoint& out, CTxInUndo& undo)
    {
        if (out.n >= vout.size())
            return false;
        if (vout[out.n].IsNull())
            return false;
        undo = CTxInUndo(vout[out.n]);
        vout[out.n].SetNull();
        Cleanup();
        if (vout.size() == 0) {
            undo.nHeight   = nHeight;
            undo.fCoinBase = fCoinBase;
            undo.nVersion  = this->nVersion;
        }
        return true;
    }

    // mark a vout spent
    bool Spend(int nPos)
    {
        CTxInUndo undo;
        COutPoint out(0, nPos);
        return Spend(out, undo);
    }

    // check whether a particular output is still available
    bool IsAvailable(unsigned int nPos) const { return (nPos < vout.size() && !vout[nPos].IsNull()); }

    // check whether the entire CCoins is spent
    // note that only !IsPruned() CCoins can be serialized
    bool IsPruned() const
    {
        BOOST_FOREACH (const CTxOut& out, vout)
            if (!out.IsNull())
                return false;
        return true;
    }
};

/** Data structure that represents a partial merkle tree.
 *
 * It respresents a subset of the txid's of a known block, in a way that
 * allows recovery of the list of txid's and the merkle root, in an
 * authenticated way.
 *
 * The encoding works as follows: we traverse the tree in depth-first order,
 * storing a bit for each traversed node, signifying whether the node is the
 * parent of at least one matched leaf txid (or a matched txid itself). In
 * case we are at the leaf level, or this bit is 0, its merkle node hash is
 * stored, and its children are not explorer further. Otherwise, no hash is
 * stored, but we recurse into both (or the only) child branch. During
 * decoding, the same depth-first traversal is performed, consuming bits and
 * hashes as they written during encoding.
 *
 * The serialization is fixed and provides a hard guarantee about the
 * encoded size:
 *
 *   SIZE <= 10 + ceil(32.25*N)
 *
 * Where N represents the number of leaf nodes of the partial tree. N itself
 * is bounded by:
 *
 *   N <= total_transactions
 *   N <= 1 + matched_transactions*tree_height
 *
 * The serialization format:
 *  - uint32     total_transactions (4 bytes)
 *  - varint     number of hashes   (1-3 bytes)
 *  - uint256[]  hashes in depth-first order (<= 32*N bytes)
 *  - varint     number of bytes of flag bits (1-3 bytes)
 *  - byte[]     flag bits, packed per 8 in a byte, least significant bit first (<= 2*N-1 bits)
 * The size constraints follow from this.
 */
class CPartialMerkleTree
{
protected:
    // the total number of transactions in the block
    unsigned int nTransactions;

    // node-is-parent-of-matched-txid bits
    std::vector<bool> vBits;

    // txids and internal hashes
    std::vector<uint256> vHash;

    // flag set when encountering invalid data
    bool fBad;

    // helper function to efficiently calculate the number of nodes at given height in the merkle tree
    unsigned int CalcTreeWidth(int height) { return (nTransactions + (1 << height) - 1) >> height; }

    // calculate the hash of a node in the merkle tree (at leaf level: the txid's themself)
    uint256 CalcHash(int height, unsigned int pos, const std::vector<uint256>& vTxid);

    // recursive function that traverses tree nodes, storing the data as bits and hashes
    void TraverseAndBuild(int height, unsigned int pos, const std::vector<uint256>& vTxid,
                          const std::vector<bool>& vMatch);

    // recursive function that traverses tree nodes, consuming the bits and hashes produced by
    // TraverseAndBuild. it returns the hash of the respective node.
    uint256 TraverseAndExtract(int height, unsigned int pos, unsigned int& nBitsUsed,
                               unsigned int& nHashUsed, std::vector<uint256>& vMatch);

public:
    // serialization implementation
    IMPLEMENT_SERIALIZE(
        READWRITE(nTransactions); READWRITE(vHash); std::vector<unsigned char> vBytes; if (fRead) {
            READWRITE(vBytes);
            CPartialMerkleTree& us = *(const_cast<CPartialMerkleTree*>(this));
            us.vBits.resize(vBytes.size() * 8);
            for (unsigned int p = 0; p < us.vBits.size(); p++)
                us.vBits[p] = (vBytes[p / 8] & (1 << (p % 8))) != 0;
            us.fBad = false;
        } else {
            vBytes.resize((vBits.size() + 7) / 8);
            for (unsigned int p = 0; p < vBits.size(); p++)
                vBytes[p / 8] |= vBits[p] << (p % 8);
            READWRITE(vBytes);
        })

    // Construct a partial merkle tree from a list of transaction id's, and a mask that selects a subset
    // of them
    CPartialMerkleTree(const std::vector<uint256>& vTxid, const std::vector<bool>& vMatch);

    CPartialMerkleTree();

    // extract the matching txid's represented by this partial merkle tree.
    // returns the merkle root, or 0 in case of failure
    uint256 ExtractMatches(std::vector<uint256>& vMatch);
};

struct CCoinsStats
{
    int      nHeight;
    uint64_t nTransactions;
    uint64_t nTransactionOutputs;
    uint64_t nSerializedSize;

    CCoinsStats() : nHeight(0), nTransactions(0), nTransactionOutputs(0), nSerializedSize(0) {}
};

/** Abstract view on the open txout dataset. */
class CCoinsView
{
public:
    // Retrieve the CCoins (unspent transaction outputs) for a given txid
    virtual bool GetCoins(uint256 txid, CCoins& coins);

    // Modify the CCoins for a given txid
    virtual bool SetCoins(uint256 txid, const CCoins& coins);

    // Just check whether we have data for a given txid.
    // This may (but cannot always) return true for fully spent transactions
    virtual bool HaveCoins(uint256 txid);

    // Retrieve the block index whose state this CCoinsView currently represents
    virtual CBlockIndex* GetBestBlock();

    // Modify the currently active block index
    virtual bool SetBestBlock(CBlockIndex* pindex);

    // Do a bulk modification (multiple SetCoins + one SetBestBlock)
    virtual bool BatchWrite(const std::map<uint256, CCoins>& mapCoins, CBlockIndex* pindex);

    // Calculate statistics about the unspent transaction output set
    virtual bool GetStats(CCoinsStats& stats);

    // As we use CCoinsViews polymorphically, have a virtual destructor
    virtual ~CCoinsView() {}
};

/** CCoinsView backed by another CCoinsView */
class CCoinsViewBacked : public CCoinsView
{
protected:
    CCoinsView* base;

public:
    CCoinsViewBacked(CCoinsView& viewIn);
    bool         GetCoins(uint256 txid, CCoins& coins);
    bool         SetCoins(uint256 txid, const CCoins& coins);
    bool         HaveCoins(uint256 txid);
    CBlockIndex* GetBestBlock();
    bool         SetBestBlock(CBlockIndex* pindex);
    void         SetBackend(CCoinsView& viewIn);
    bool         BatchWrite(const std::map<uint256, CCoins>& mapCoins, CBlockIndex* pindex);
    bool         GetStats(CCoinsStats& stats);
};

/** CCoinsView that adds a memory cache for transactions to another CCoinsView */
class CCoinsViewCache : public CCoinsViewBacked
{
protected:
    CBlockIndex*              pindexTip;
    std::map<uint256, CCoins> cacheCoins;

public:
    CCoinsViewCache(CCoinsView& baseIn, bool fDummy = false);

    // Standard CCoinsView methods
    bool         GetCoins(uint256 txid, CCoins& coins);
    bool         SetCoins(uint256 txid, const CCoins& coins);
    bool         HaveCoins(uint256 txid);
    CBlockIndex* GetBestBlock();
    bool         SetBestBlock(CBlockIndex* pindex);
    bool         BatchWrite(const std::map<uint256, CCoins>& mapCoins, CBlockIndex* pindex);

    // Return a modifiable reference to a CCoins. Check HaveCoins first.
    // Many methods explicitly require a CCoinsViewCache because of this method, to reduce
    // copying.
    CCoins& GetCoins(uint256 txid);

    // Push the modifications applied to this cache to its base.
    // Failure to call this method before destruction will cause the changes to be forgotten.
    bool Flush();

    // Calculate the size of the cache (in number of transactions)
    unsigned int GetCacheSize();

private:
    std::map<uint256, CCoins>::iterator FetchCoins(uint256 txid);
};

/** CCoinsView that brings transactions from a memorypool into view.
    It does not check for spendings by memory pool transactions. */
class CCoinsViewMemPool : public CCoinsViewBacked
{
protected:
    CTxMemPool& mempool;

public:
    CCoinsViewMemPool(CCoinsView& baseIn, CTxMemPool& mempoolIn);
    bool GetCoins(uint256 txid, CCoins& coins);
    bool HaveCoins(uint256 txid);
};

/** Global variable that points to the active CCoinsView (protected by cs_main) */
extern CCoinsViewCache* pcoinsTip;

/** Global variable that points to the active block tree (protected by cs_main) */
extern CTxDB* pblocktree;

struct CBlockTemplate
{
    CBlock               block;
    std::vector<int64_t> vTxFees;
    std::vector<int64_t> vTxSigOps;
};

/** Used to relay blocks as header + vector<merkle branch>
 * to filtered nodes.
 */
class CMerkleBlock
{
public:
    // Public only for unit testing
    CBlock             header;
    CPartialMerkleTree txn;

public:
    // Public only for unit testing and relay testing
    // (not relayed)
    std::vector<std::pair<unsigned int, uint256>> vMatchedTxn;

    // Create from a CBlock, filtering transactions according to filter
    // Note that this will call IsRelevantAndUpdate on the filter for each transaction,
    // thus the filter will likely be modified.
    CMerkleBlock(const CBlock& block, CBloomFilter& filter);

    IMPLEMENT_SERIALIZE(READWRITE(header); READWRITE(txn);)
};

void ExportBootstrapBlockchain(const std::string& filename, std::atomic<bool>& stopped,
                               std::atomic<double>& progress, boost::promise<void>& result);
void ExportBootstrapBlockchainWithOrphans(const std::string& filename, std::atomic<bool>& stopped,
                                          std::atomic<double>& progress, boost::promise<void>& result,
                                          GraphTraverseType traverseType);

#endif

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include "block.h"
#include "kernel.h"
#include "main.h"
#include "txdb.h"
#include "txmempool.h"
#include "work.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

extern unsigned int nMinerSleep;

int static FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata  = (unsigned char*)pbuffer;
    unsigned int   blocks = 1 + ((len + 8) / 64);
    unsigned char* pend   = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len]        = 0x80;
    unsigned int bits = len * 8;
    pend[-1]          = (bits >> 0) & 0xff;
    pend[-2]          = (bits >> 8) & 0xff;
    pend[-3]          = (bits >> 16) & 0xff;
    pend[-4]          = (bits >> 24) & 0xff;
    return blocks;
}

static const unsigned int pSHA256InitState[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                                 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

void SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    SHA256_CTX    ctx;
    unsigned char data[64];

    SHA256_Init(&ctx);

    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = ByteReverse(((uint32_t*)pinput)[i]);

    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];

    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}

// Some explaining would be appreciated
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256>        setDependsOn;
    double              dPriority;
    double              dFeePerKb;

    COrphan(const CTransaction* ptxIn)
    {
        ptx       = ptxIn;
        dPriority = dFeePerKb = 0;
    }

    void print() const
    {
        NLog.write(b_sev::info, "COrphan(hash={}, dPriority={:.1f}, dFeePerKb={:.1f})",
                   ptx->GetHash().ToString().substr(0, 10), dPriority, dFeePerKb);
        for (uint256 hash : setDependsOn)
            NLog.write(b_sev::info, "   setDependsOn {}", hash.ToString().substr(0, 10));
    }
};

uint64_t   nLastBlockTx   = 0;
uint64_t   nLastBlockSize = 0;
StakeMaker stakeMaker;

// We want to sort transactions by priority and fee, so:
typedef boost::tuple<double, double, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) {}
    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee) {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        } else {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

// CreateNewBlock: create new block (without proof-of-work/proof-of-stake)
std::unique_ptr<CBlock> CreateNewBlock(CWallet* pwallet, bool fProofOfStake, int64_t* pFees,
                                       const boost::optional<CBitcoinAddress>& PoWDestination)
{
    // Create new block
    std::unique_ptr<CBlock> pblock(new CBlock());
    if (!pblock)
        return nullptr;

    const CTxDB txdb;

    boost::optional<CBlockIndex> pindexPrev = *txdb.GetBestBlockIndex();

    // Create coinbase tx
    CTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);

    if (!fProofOfStake) {
        if (!pwallet) {
            return nullptr;
        }
        if (PoWDestination) {
            coinbaseTx.vout[0].scriptPubKey.SetDestination(PoWDestination->Get());
        } else {
            CReserveKey reservekey(pwallet);
            CPubKey     pubkey;
            if (!reservekey.GetReservedKey(pubkey))
                return nullptr;
            coinbaseTx.vout[0].scriptPubKey.SetDestination(pubkey.GetID());
        }
    } else {
        // Height first in coinbase required for block.version=2
        coinbaseTx.vin[0].scriptSig = (CScript() << pindexPrev->nHeight + 1) + COINBASE_FLAGS;
        assert(coinbaseTx.vin[0].scriptSig.size() <= 100);

        coinbaseTx.vout[0].SetEmpty();
    }

    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Add our coinbase tx as first transaction
    pblock->vtx.push_back(coinbaseTx);

    unsigned int nSizeLimit = MaxBlockSize(txdb);

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", nSizeLimit);
    // Limit to betweeen 1K and nSizeLimit-1K for sanity:
    nBlockMaxSize =
        std::max(1000u, std::min(static_cast<unsigned int>(nSizeLimit - 1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", 27000);
    nBlockPrioritySize              = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", 0);
    nBlockMinSize              = std::min(nBlockMaxSize, nBlockMinSize);

    // Fee-per-kilobyte amount considered the same as "free"
    // Be careful setting this: if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    int64_t                            nMinTxFee = MIN_TX_FEE;
    const boost::optional<std::string> minTxFee  = mapArgs.get("-mintxfee");
    if (minTxFee) {
        ParseMoney(*minTxFee, nMinTxFee);
    }

    pblock->nBits = GetNextTargetRequired(txdb, &*pindexPrev, fProofOfStake);

    // map of issued token names in this block vs token hashes
    // this is used to prevent duplicate token names
    std::unordered_map<std::string, uint256> issuedTokensSymbolsInThisBlock;

    // Collect memory pool transactions into the block
    int64_t nFees = 0;
    {
        const CTxMemPool& mempool_ = ::mempool;
        LOCK2(cs_main, mempool_.cs);

        // Priority order to process transactions
        list<COrphan>                  vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*>> mapDependers;

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool_.mapTx.size());
        for (map<uint256, CTransaction>::const_iterator mi = mempool_.mapTx.cbegin();
             mi != mempool_.mapTx.cend(); ++mi) {
            const CTransaction& tx = (*mi).second;
            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, txdb, pindexPrev->nHeight + 1))
                continue;

            COrphan* porphan        = nullptr;
            double   dPriority      = 0;
            int64_t  nTotalIn       = 0;
            bool     fMissingInputs = false;
            for (const CTxIn& txin : tx.vin) {
                // Read prev transaction
                CTransaction txPrev;
                CTxIndex     txindex;
                if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex)) {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    const auto txIt = mempool_.mapTx.find(txin.prevout.hash);
                    if (txIt == mempool_.mapTx.cend()) {
                        NLog.write(b_sev::err, "ERROR: mempool transaction missing input");
                        if (fDebug)
                            assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan) {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += txIt->second.vout[txin.prevout.n].nValue;
                    continue;
                }
                int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = txindex.GetDepthInMainChain(txdb);
                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs)
                continue;

            // Priority is sum(valuein * age) / txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority /= nTxSize;

            // This is a more accurate fee-per-kilobyte than is used by the client code, because the
            // client code rounds up the size to the nearest 1K. That's good, because it gives an
            // incentive to create smaller transactions.
            double dFeePerKb = double(nTotalIn - tx.GetValueOut()) / (double(nTxSize) / 1000.0);

            if (porphan) {
                porphan->dPriority = dPriority;
                porphan->dFeePerKb = dFeePerKb;
            } else
                vecPriority.push_back(TxPriority(dPriority, dFeePerKb, &(*mi).second));
        }

        // Collect transactions into block
        map<uint256, CTxIndex> mapTestPool;
        uint64_t               nBlockSize   = 1000;
        uint64_t               nBlockTx     = 0;
        int                    nBlockSigOps = 100;
        bool                   fSortedByFee = (nBlockPrioritySize <= 0);

        map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>> mapQueuedNTP1Inputs;

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty()) {
            // Take highest priority transaction off the priority queue:
            double              dPriority = vecPriority.front().get<0>();
            double              dFeePerKb = vecPriority.front().get<1>();
            const CTransaction& tx        = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = tx.GetLegacySigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Timestamp limit
            if (tx.nTime > GetAdjustedTime() || (fProofOfStake && tx.nTime > pblock->vtx[0].nTime))
                continue;

            // Transaction fee
            int64_t nMinFee = tx.GetMinFee(txdb, nBlockSize, GMF_BLOCK);

            // Skip free transactions if we're past the minimum block size:
            if (fSortedByFee && (dFeePerKb < nMinTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritize by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || (dPriority < COIN * 144 / 250))) {
                fSortedByFee = true;
                comparer     = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            // Connecting shouldn't fail due to dependency on other memory pool transactions
            // because we're already processing them in order of dependency
            map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);

            std::vector<std::pair<CTransaction, NTP1Transaction>>               inputsTxs;
            map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>> mapQueuedNTP1InputsTmp(
                mapQueuedNTP1Inputs);

            MapPrevTx mapInputs;
            bool      fInvalid;
            if (!tx.FetchInputs(txdb, mapTestPoolTmp, false, true, mapInputs, fInvalid))
                continue;

            int64_t nTxFees = tx.GetValueIn(mapInputs) - tx.GetValueOut();
            if (nTxFees < nMinFee)
                continue;

            nTxSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            try {
                std::string opRet;
                if (NTP1Transaction::IsTxNTP1(&tx, &opRet)) {
                    auto script = NTP1Script::ParseScript(opRet);
                    if (script->getTxType() == NTP1Script::TxType_Issuance) {

                        inputsTxs = NTP1Transaction::StdFetchedInputTxsToNTP1(
                            tx, mapInputs, txdb, false, mapQueuedNTP1InputsTmp, mapTestPoolTmp);

                        NTP1Transaction ntp1tx;
                        ntp1tx.readNTP1DataFromTx(txdb, tx, inputsTxs);
                        AssertNTP1TokenNameIsNotAlreadyInMainChain(ntp1tx, txdb);
                        if (ntp1tx.getTxType() == NTP1TxType_ISSUANCE) {
                            std::string currSymbol = ntp1tx.getTokenSymbolIfIssuance();
                            // make sure that case doesn't matter by converting to upper case
                            std::transform(currSymbol.begin(), currSymbol.end(), currSymbol.begin(),
                                           ::toupper);
                            if (issuedTokensSymbolsInThisBlock.find(currSymbol) !=
                                issuedTokensSymbolsInThisBlock.end()) {
                                throw std::runtime_error("The token name " + currSymbol +
                                                         " already exists in this block (while mining). "
                                                         "Skipping this transaction.");
                            }
                            issuedTokensSymbolsInThisBlock.insert(
                                std::make_pair(currSymbol, ntp1tx.getTxHash()));
                        }
                    }
                }
            } catch (std::exception& ex) {
                NLog.write(b_sev::err,
                           "Error while mining and verifying the uniqueness of issued token symbol in "
                           "CreateNewBlock(): {}",
                           ex.what());
                continue;
            } catch (...) {
                NLog.write(b_sev::err,
                           "Error while mining and verifying the uniqueness of issued token symbol in "
                           "CreateNewBlock(). Unknown exception thrown");
                continue;
            }

            if (tx.ConnectInputs(txdb, mapInputs, mapTestPoolTmp, CDiskTxPos(1, 1), pindexPrev, false,
                                 true)
                    .isErr())
                continue;

            mapTestPoolTmp[tx.GetHash()] = CTxIndex(CDiskTxPos(1, 1), tx.vout.size());
            swap(mapTestPool, mapTestPoolTmp);
            mapQueuedNTP1InputsTmp[tx.GetHash()] = inputsTxs;
            swap(mapQueuedNTP1Inputs, mapQueuedNTP1InputsTmp);

            // Added
            pblock->vtx.push_back(tx);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fDebug) {
                NLog.write(b_sev::info, "priority {:.1f} feeperkb {:.1f} txid {}", dPriority, dFeePerKb,
                           tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            uint256 hash = tx.GetHash();
            if (mapDependers.count(hash)) {
                for (COrphan* porphan : mapDependers[hash]) {
                    if (!porphan->setDependsOn.empty()) {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty()) {
                            vecPriority.push_back(
                                TxPriority(porphan->dPriority, porphan->dFeePerKb, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx   = nBlockTx;
        nLastBlockSize = nBlockSize;

        if (fDebug)
            NLog.write(b_sev::debug, "CreateNewBlock(): total size {}", nBlockSize);

        if (!fProofOfStake)
            pblock->vtx[0].vout[0].nValue = GetProofOfWorkReward(txdb, nFees);

        if (pFees)
            *pFees = nFees;

        // Fill in header
        pblock->hashPrevBlock = pindexPrev->GetBlockHash();
        pblock->nTime = max(pindexPrev->GetPastTimeLimit(txdb) + 1, pblock->GetMaxTransactionTime());
        pblock->nTime = max(pblock->GetBlockTime(), PastDrift(pindexPrev->GetBlockTime()));
        if (!fProofOfStake)
            pblock->UpdateTime(&*pindexPrev);
        pblock->nNonce = 0;
    }

    return pblock;
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce   = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;

    unsigned int nHeight =
        pindexPrev->nHeight + 1; // Height first in coinbase required for block.version=2
    pblock->vtx[0].vin[0].scriptSig = (CScript() << nHeight << CBigNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(pblock->vtx[0].vin[0].scriptSig.size() <= 100);

    pblock->hashMerkleRoot = pblock->GetMerkleRoot();
}

void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Pre-build hash buffers
    //
    struct
    {
        struct unnamed2
        {
            int          nVersion;
            uint256      hashPrevBlock;
            uint256      hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        } block;
        unsigned char pchPadding0[64];
        uint256       hash1;
        unsigned char pchPadding1[64];
    } tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion       = pblock->nVersion;
    tmp.block.hashPrevBlock  = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime          = pblock->nTime;
    tmp.block.nBits          = pblock->nBits;
    tmp.block.nNonce         = pblock->nNonce;

    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    // Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp) / 4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    // Precalc the first half of the first hash, which stays constant
    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);

    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}

bool CheckWork(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    uint256 hashBlock  = pblock->GetHash();
    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

    if (!pblock->IsProofOfWork())
        return NLog.error("CheckWork() : {} is not a proof-of-work block", hashBlock.GetHex());

    if (hashBlock > hashTarget)
        return NLog.error("CheckWork() : proof-of-work not meeting target");

    //// debug print
    NLog.write(b_sev::debug, "CheckWork() : new proof-of-work block found hash: {} target: {}",
               hashBlock.GetHex(), hashTarget.GetHex());
    pblock->print();
    NLog.write(b_sev::debug, "generated {}", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != CTxDB().GetBestBlockHash())
            return NLog.error("CheckWork() : generated block is stale");

        // Remove key from key pool
        reservekey.KeepKey();

        // Track how many getdata requests this block gets
        {
            LOCK(wallet.cs_wallet);
            wallet.mapRequestCount[hashBlock] = 0;
        }

        // Process this block the same as if we had received it from another node
        if (!ProcessBlock(nullptr, pblock))
            return NLog.error("CheckWork() : ProcessBlock, block not accepted");
    }

    return true;
}

bool CheckStake(const ITxDB& txdb, CBlock* pblock, CWallet& wallet)
{
    uint256 proofHash = 0, hashTarget = 0;
    uint256 hashBlock = pblock->GetHash();

    if (!pblock->IsProofOfStake())
        return NLog.error("CheckStake() : {} is not a proof-of-stake block", hashBlock.GetHex());

    // verify hash target and signature of coinstake tx
    if (!CheckProofOfStake(txdb, pblock->vtx[1], pblock->nBits, proofHash, hashTarget))
        return NLog.error("CheckStake() : proof-of-stake checking failed");

    //// debug print
    NLog.write(b_sev::info,
               "CheckStake() : new proof-of-stake block found  hash: {} proofhash: {} target: {}",
               hashBlock.GetHex(), proofHash.GetHex(), hashTarget.GetHex());
    pblock->print();
    NLog.write(b_sev::info, "out {}", FormatMoney(pblock->vtx[1].GetValueOut()));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != CTxDB().GetBestBlockHash())
            return NLog.error("CheckStake() : generated block is stale");

        // Track how many getdata requests this block gets
        {
            LOCK(wallet.cs_wallet);
            wallet.mapRequestCount[hashBlock] = 0;
        }

        // Process this block the same as if we had received it from another node
        if (!ProcessBlock(nullptr, pblock))
            return NLog.error("CheckStake() : ProcessBlock, block not accepted");
    }

    return true;
}

bool is_vNodesEmpty_safe()
{
    LOCK(cs_vNodes);
    return vNodes.empty();
}

void StakeMiner(CWallet* pwallet)
{
    SetThreadPriority(THREAD_PRIORITY_LOWEST);

    // Make this thread recognisable as the mining thread
    RenameThread("neblio-miner");

    bool fTryToSync = Params().MiningRequiresPeers();

    // we don't stake in regtest mode
    if (Params().MineBlocksOnDemand())
        return;

    // synchronize memory once
    fShutdown.load(boost::memory_order_seq_cst);
    while (!fShutdown.load(boost::memory_order_relaxed)) {
        const CTxDB txdb;

        while (pwallet->IsLocked()) {
            stakeMaker.resetLastCoinStakeSearchInterval();
            MilliSleep(1000);
            if (fShutdown)
                return;
        }

        if (Params().MiningRequiresPeers()) {
            while (is_vNodesEmpty_safe() || IsInitialBlockDownload(txdb)) {
                stakeMaker.resetLastCoinStakeSearchInterval();
                fTryToSync = true;
                MilliSleep(1000);
                if (fShutdown)
                    return;
            }
        }

        if (fTryToSync) {
            fTryToSync             = false;
            std::size_t vNodesSize = 0;
            {
                LOCK(cs_vNodes);
                vNodesSize = vNodes.size();
            }
            if (vNodesSize < 3 || txdb.GetBestChainHeight().value_or(0) < GetNumBlocksOfPeers()) {
                MilliSleep(60000);
                continue;
            }
        }

        //
        // Create new block
        //
        CAmount                 nFees;
        std::unique_ptr<CBlock> pblock = CreateNewBlock(pwallet, true, &nFees);
        if (!pblock)
            return;

        // Trying to sign a block
        if (pblock->SignBlock(txdb, *pwallet, nFees)) {
            SetThreadPriority(THREAD_PRIORITY_NORMAL);
            CheckStake(txdb, pblock.get(), *pwallet);
            SetThreadPriority(THREAD_PRIORITY_LOWEST);
            MilliSleep(500);
        } else {
            MilliSleep(nMinerSleep);
        }
    }
}

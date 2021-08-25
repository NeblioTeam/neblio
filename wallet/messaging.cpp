#include "messaging.h"

#include "alert.h"
#include "block.h"
#include "blockindex.h"
#include "checkpoints.h"
#include "consensus.h"
#include "globals.h"
#include "key.h"
#include "mempoolmisc.h"
#include "merkleblock.h"
#include "txmempool.h"
#include "wallet.h"
#include "wallet_interface.h"
#include "work.h"
#include <cstdint>
#include <map>
#include <net.h>
#include <protocol.h>
#include <set>
#include <txdb.h>
#include <utility>
#include <vector>

CMedianFilter<int> cPeerBlockCounts(5, 0);

std::unordered_map<uint256, CBlock*> mapOrphanBlocks;
std::multimap<uint256, CBlock*>      mapOrphanBlocksByPrev;

std::map<uint256, CTransaction>      mapOrphanTransactions;
std::map<uint256, std::set<uint256>> mapOrphanTransactionsByPrev;

std::set<std::pair<COutPoint, unsigned int>> setStakeSeenOrphan;

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
        uint256                                   randomhash = GetRandHash();
        std::map<uint256, CTransaction>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}

#define ACT_ON_MSG_PROCESS_RETURN(resVar)                                                               \
    do {                                                                                                \
        switch (resVar) {                                                                               \
        case MessageProcessResult::ReturnTrue:                                                          \
            return true;                                                                                \
        case MessageProcessResult::ReturnFalse:                                                         \
            return false;                                                                               \
        case MessageProcessResult::DoNothing:                                                           \
            break;                                                                                      \
        }                                                                                               \
    } while (0)

/** Minimum Peer Version */
int MinPeerVersion(const ITxDB& txdb)
{
    if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__5_COLD_STAKING, txdb)) {
        return MIN_PEER_PROTO_VERSION;
    } else {
        return OLD_MIN_PEER_PROTO_VERSION;
    }
}

bool AlreadyHave(const ITxDB& txdb, const CInv& inv)
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

MessageProcessResult handleMsg_version(CNode* pfrom, CDataStream& vRecv)
{
    // Each connection can only send one version message
    if (pfrom->nVersion != 0) {
        pfrom->Misbehaving(1);
        return MessageProcessResult::ReturnFalse;
    }

    const CTxDB txdb;

    int64_t  nTime;
    CAddress addrMe;
    CAddress addrFrom;
    uint64_t nNonce = 1;
    vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
    int minPeerVer = MinPeerVersion(txdb);
    if (pfrom->nVersion < minPeerVer) {
        // disconnect from peers older than this proto version
        NLog.write(b_sev::info, "partner {} using obsolete version {}; disconnecting",
                   pfrom->addr.ToString(), pfrom->nVersion);
        pfrom->fDisconnect = true;
        return MessageProcessResult::ReturnFalse;
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
        return MessageProcessResult::ReturnTrue;
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
    pfrom->ssSend.SetVersion(std::min(pfrom->nVersion, PROTOCOL_VERSION));

    if (!pfrom->fInbound) {
        // Advertise our address
        if (!fNoListen && !IsInitialBlockDownload(txdb)) {
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
    // For regtest, we need to sync immediately after connection; this is important for tests that
    // split and reconnect the network
    static int nAskedForBlocks           = 0;
    const bool neverAskedForBlocksBefore = (nAskedForBlocks < 1 || vNodes.size() <= 1);
    const bool withinAllowedVersionRange =
        (pfrom->nVersion < NOBLKS_VERSION_START || pfrom->nVersion >= NOBLKS_VERSION_END);
    const bool peerHasNewBlocks = (pfrom->nStartingHeight > txdb.GetBestChainHeight().value_or(0) - 144);

    if (!fImporting && !pfrom->fClient && !pfrom->fOneShot &&
        ((peerHasNewBlocks && withinAllowedVersionRange && neverAskedForBlocksBefore) ||
         Params().NetType() == NetworkType::Regtest)) {
        nAskedForBlocks++;
        const boost::optional<CBlockIndex> best = txdb.GetBestBlockIndex();
        pfrom->PushGetBlocks(&*best, uint256(0));
    }

    // Relay alerts
    {
        LOCK(cs_mapAlerts);
        for (const auto& item : mapAlerts)
            item.second.RelayTo(pfrom);
    }

    pfrom->fSuccessfullyConnected = true;

    NLog.write(b_sev::info, "receive version message: version {}, blocks={}, us={}, them={}, peer={}",
               pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString(), addrFrom.ToString(),
               pfrom->addr.ToString());

    cPeerBlockCounts.input(pfrom->nStartingHeight);

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_verack(CNode* pfrom, CDataStream& /*vRecv*/)
{
    pfrom->SetRecvVersion(std::min(pfrom->nVersion, PROTOCOL_VERSION));
    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_addr(CNode* pfrom, CDataStream& vRecv)
{
    std::vector<CAddress> vAddr;
    vRecv >> vAddr;

    // Don't want addr from older versions unless seeding
    if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.get().size() > 1000)
        return MessageProcessResult::ReturnTrue;
    if (vAddr.size() > 1000) {
        pfrom->Misbehaving(20);
        NLog.write(b_sev::err, "message addr size() = {}", vAddr.size());
        return MessageProcessResult::ReturnFalse;
    }

    // Store the new addresses
    std::vector<CAddress> vAddrOk;
    int64_t               nNow   = GetAdjustedTime();
    int64_t               nSince = nNow - 10 * 60;
    for (CAddress& addr : vAddr) {
        if (fShutdown)
            return MessageProcessResult::ReturnTrue;
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
                std::multimap<uint256, CNode*> mapMix;
                for (CNode* pnode : vNodes) {
                    if (pnode->nVersion < CADDR_TIME_VERSION)
                        continue;
                    unsigned int nPointer;
                    memcpy(&nPointer, &pnode, sizeof(nPointer));
                    uint256 hashKey = hashRand ^ nPointer;
                    hashKey         = Hash(BEGIN(hashKey), END(hashKey));
                    mapMix.insert(std::make_pair(hashKey, pnode));
                }
                int nRelayNodes =
                    fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                for (std::multimap<uint256, CNode*>::iterator mi = mapMix.begin();
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_inv(CNode* pfrom, CDataStream& vRecv)
{
    std::vector<CInv> vInv;
    vRecv >> vInv;
    if (vInv.size() > MAX_INV_SZ) {
        pfrom->Misbehaving(20);
        NLog.write(b_sev::err, "message inv size() = {}", vInv.size());
        return MessageProcessResult::ReturnFalse;
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
            return MessageProcessResult::ReturnTrue;
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_getdata(CNode* pfrom, CDataStream& vRecv)
{
    const CTxDB txdb;

    std::vector<CInv> vInv;
    vRecv >> vInv;
    if (vInv.size() > MAX_INV_SZ) {
        pfrom->Misbehaving(20);
        NLog.write(b_sev::err, "message getdata size() = {}", vInv.size());
        return MessageProcessResult::ReturnFalse;
    }

    if (fDebugNet || (vInv.size() != 1))
        NLog.write(b_sev::debug, "received getdata ({} invsz)", vInv.size());

    for (const CInv& inv : vInv) {
        if (fShutdown)
            return MessageProcessResult::ReturnTrue;
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
                    std::vector<CInv> vInvP;
                    vInvP.push_back(
                        CInv(MSG_BLOCK,
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
                std::map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_getblocks(CNode* pfrom, CDataStream& vRecv)
{
    CBlockLocator locator;
    uint256       hashStop;
    vRecv >> locator >> hashStop;

    if (locator.size() > MAX_LOCATOR_SZ) {
        NLog.write(b_sev::err, "locator size {} > {}, disconnect peer={} with addr={}", locator.size(),
                   MAX_LOCATOR_SZ, pfrom->nodeid, pfrom->addr.ToString());
        pfrom->fDisconnect = true;
        return MessageProcessResult::ReturnTrue;
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_getheaders(CNode* pfrom, CDataStream& vRecv)
{
    CBlockLocator locator;
    uint256       hashStop;
    vRecv >> locator >> hashStop;

    if (locator.size() > MAX_LOCATOR_SZ) {
        NLog.write(b_sev::err, "locator size {} > {}, disconnect peer={} with addr={}", locator.size(),
                   MAX_LOCATOR_SZ, pfrom->nodeid, pfrom->addr.ToString());
        pfrom->fDisconnect = true;
        return MessageProcessResult::ReturnTrue;
    }

    const CTxDB txdb;

    boost::optional<CBlockIndex> pindex = boost::none;
    if (locator.IsNull()) {
        // If locator is null, return the hashStop block
        const auto mi = txdb.ReadBlockIndex(hashStop);
        if (!mi)
            return MessageProcessResult::ReturnTrue;
        pindex = mi;
    } else {
        // Find the last block the caller has in the main chain
        pindex = locator.GetBlockIndex(txdb);
        if (pindex) {
            pindex = pindex->getNext(txdb);
        }
    }

    std::vector<CBlock> vHeaders;
    int                 nLimit = 2000;
    NLog.write(b_sev::info, "getheaders {} to {}", (pindex ? pindex->nHeight : -1), hashStop.ToString());
    while (pindex) {
        vHeaders.push_back(pindex->GetBlockHeader());
        if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
            break;
        pindex = pindex->getNext(txdb);
    }
    pfrom->PushMessage("headers", vHeaders);

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_tx(CNode* pfrom, CDataStream& vRecv)
{
    std::vector<uint256> vWorkQueue;
    std::vector<uint256> vEraseQueue;
    CTransaction         tx;
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
            for (std::set<uint256>::iterator mi = mapOrphanTransactionsByPrev[hashPrev].begin();
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
                    NLog.write(b_sev::info, "   removed invalid orphan tx {}", orphanTxHash.ToString());
                }
            }
        }

        for (uint256 hash : vEraseQueue)
            EraseOrphanTx(hash);
    } else if (mempoolRes.unwrapErr(RESULT_PRE).GetResult() == TxValidationResult::TX_MISSING_INPUTS) {
        AddOrphanTx(tx);

        // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
        unsigned int nMaxOrphanTx =
            (unsigned int)std::max(INT64_C(0), GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_block(CNode* pfrom, CDataStream& vRecv)
{
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_getaddr(CNode* pfrom, CDataStream& /*vRecv*/)
{
    // Don't return addresses older than nCutOff timestamp
    int64_t nCutOff = GetTime() - (nNodeLifespan * 24 * 60 * 60);
    pfrom->vAddrToSend.clear();
    std::vector<CAddress> vAddr = addrman.get().GetAddr();
    for (const CAddress& addr : vAddr)
        if (addr.nTime > nCutOff)
            pfrom->PushAddress(addr);

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_mempool(CNode* pfrom, CDataStream& /*vRecv*/)
{
    std::vector<uint256> vtxid;
    LOCK2(mempool.cs, pfrom->cs_filter);
    mempool.queryHashes(vtxid);
    std::vector<CInv> vInv;
    for (uint256& hash : vtxid) {
        CInv                inv(MSG_TX, hash);
        const CTransaction* txFromMempool = mempool.lookup_unsafe(hash);
        // this tx should exist because we locked then used mempool.queryHashes()
        assert(txFromMempool);
        if ((pfrom->pfilter && pfrom->pfilter->IsRelevantAndUpdate(*txFromMempool)) || (!pfrom->pfilter))
            vInv.push_back(inv);
        if (vInv.size() == MAX_INV_SZ)
            break;
    }
    if (vInv.size() > 0)
        pfrom->PushMessage("inv", vInv);

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_ping(CNode* pfrom, CDataStream& vRecv)
{
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_alert(CNode* pfrom, CDataStream& vRecv)
{
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_filterload(CNode* pfrom, CDataStream& vRecv)
{
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_filteradd(CNode* pfrom, CDataStream& vRecv)
{
    std::vector<unsigned char> vData;
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

    return MessageProcessResult::DoNothing;
}

MessageProcessResult handleMsg_filterclear(CNode* pfrom, CDataStream& /*vRecv*/)
{
    LOCK(pfrom->cs_filter);
    delete pfrom->pfilter;
    pfrom->pfilter    = nullptr;
    pfrom->fRelayTxes = true;

    return MessageProcessResult::DoNothing;
}

bool ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    static std::map<CService, CPubKey> mapReuseKey;
    RandAddSeedPerfmon();
    if (fDebug)
        NLog.write(b_sev::debug, "received: {} ({} bytes)", strCommand, vRecv.size());
    const boost::optional<std::string> dropMessageTest = mapArgs.get("-dropmessagestest");
    if (dropMessageTest && GetRand(atoi(*dropMessageTest)) == 0) {
        NLog.write(b_sev::info, "dropmessagestest DROPPING RECV MESSAGE");
        return true;
    }

    if (strCommand == "version") {
        const auto res = handleMsg_version(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (pfrom->nVersion == 0) {
        // Must have a version message before anything else
        pfrom->Misbehaving(1);
        return false;
    }

    else if (strCommand == "verack") {
        const auto res = handleMsg_verack(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "addr") {
        const auto res = handleMsg_addr(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "inv") {
        const auto res = handleMsg_inv(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "getdata") {
        const auto res = handleMsg_getdata(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "getblocks") {
        const auto res = handleMsg_getblocks(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "getheaders") {
        const auto res = handleMsg_getheaders(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "tx") {
        const auto res = handleMsg_tx(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "block") {
        const auto res = handleMsg_block(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "getaddr") {
        const auto res = handleMsg_getaddr(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "mempool") {
        const auto res = handleMsg_mempool(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "ping") {
        const auto res = handleMsg_ping(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "alert") {
        const auto res = handleMsg_alert(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "filterload") {
        const auto res = handleMsg_filterload(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "filteradd") {
        const auto res = handleMsg_filteradd(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
    }

    else if (strCommand == "filterclear") {
        const auto res = handleMsg_filterclear(pfrom, vRecv);
        ACT_ON_MSG_PROCESS_RETURN(res);
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

void PruneOrphanBlocks()
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

uint256 GetOrphanRoot(const CBlock* pblock)
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
            mapOrphanBlocks.insert(std::make_pair(hash, pblock2));
            mapOrphanBlocksByPrev.insert(std::make_pair(pblock2->hashPrevBlock, pblock2));

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
    std::vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++) {
        uint256 hashPrev = vWorkQueue[i];
        for (std::multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
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

// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/foreach.hpp>

#include "checkpoints.h"

#include "ThreadSafeMap.h"

#include "main.h"
#include "txdb.h"
#include "uint256.h"

static const int nCheckpointSpan = 10;

namespace Checkpoints {
typedef ThreadSafeMap<int, uint256> MapCheckpoints;

// clang-format off

    //
    // What makes a good checkpoint block?
    // + Is surrounded by blocks with reasonable timestamps
    //   (no blocks before with a timestamp after, none after with
    //    timestamp before)
    // + Contains no strange transactions
    //
    static MapCheckpoints mapCheckpoints(MapCheckpoints::MapType{
        {     0,      hashGenesisBlock },
        {   500,      uint256{"0x00000342c5dc5f7fd4a8ef041d4df4e569bd40756405a8c336c5f42c77e097a2"}},
        {  1000,      uint256{"0x00000c60e3a8d27dedb15fc33d91caec5cf714fae60f24ea22a649cded8e0cca"}},
        {  5000,      uint256{"0x074873095a26296d4f0033f697f46bddb7c1359ffcb3461f620e346bc516a1d2"}},
        { 25000,      uint256{"0x9c28e51c9c21092909fe0a6ad98ae335f253fa9c8076bb3cca154b6ba5ee03ab"}},
        {100000,      uint256{"0xbb13aedc5846fe5d384601ef4648492262718fc7dfe35b886ef297ea74cab8cc"}},
        {150000,      uint256{"0x9a755758cc9a8d40fc36e6cc312077c8dd5b32b2c771241286099fd54fd22db0"}},
        {200000,      uint256{"0xacea764bbb689e940040b229a89213e17b50b98db0514e1428acedede9c1a4c0"}},
        {250000,      uint256{"0x297eda3c18c160bdb2b1465164b11ba2ee7908b209a26d3b76eac3876aa55072"}},
        {260000,      uint256{"0x4d407875afd318897266c14153d856774868949c65176de9214778d5626707a0"}},
        {270000,      uint256{"0x7f8ead004a853b411de63a3f30ee5a0e4c144a11dbbc00c96942eb58ff3b9a48"}},
        {280000,      uint256{"0x954544adaa689ad91627822b9da976ad6f272ced95a272b41b108aabff30a3e5"}},
        {285000,      uint256{"0x7c37fbdb5129db54860e57fd565f0a17b40fb8b9d070bda7368d196f63034ae5"}},
        {287500,      uint256{"0x3da2de78a53afaf9dafc8cec20a7ace84c52cff994307aef4072d3d0392fe041"}},
        {290000,      uint256{"0x5685d1cc15100fa0c7423b7427b9f0f22653ccd137854f3ecc6230b0d1af9ebc"}},
        {295000,      uint256{"0x581aef5415de9ce8b2817bf803cf29150bd589a242c4cb97a6fd931d6f165190"}},
        {300000,      uint256{"0xb2d6ef8b3ec931c48c2d42fa574a382a534014388b17eb8e0eca1a0db379e369"}},
        {305000,      uint256{"0x9332baa2c500cb938024d2ec35b265bfa2928b63ae5d2d9d81ffd8cbfd75ef1d"}},
        {310000,      uint256{"0x53c993efaf747fadd0ecae8b3a15292549e77223853a8dc90c18aa4664f85b6e"}},
        {315000,      uint256{"0xb46b2d2681294d04a366f34eb2b9183621961432c841a155fe723deabcbf9e38"}},
        {320000,      uint256{"0x82ecc41d44fefc6667119b0142ba956670bda4e15c035eefe66bfaa4362d2823"}},
        {350000,      uint256{"0x7787a1240f1bff02cd3e37cfc8f4635725e26c6db7ff44e8fbee7bf31dc6d929"}},
        {360000,      uint256{"0xb4b001753a4d7ec18012a5ff1cbf3f614130adbf6c3f2515d36dfc3300655c2a"}},
        {387026,      uint256{"0x37ec421ce623892935d939930d61c066499b8c7eb55606be67219a576d925b67"}},
        {387027,      uint256{"0x1a7a41f757451fa32acb0aa31e262398d660e90994b8e17f164dd201718c8f5d"}},
        {387028,      uint256{"0xac7d44244ff394255f4c1f99664b26cd015d3d10bddbb8a86727ff848faa6acf"}},
        {387029,      uint256{"0x7e4655517659f78cd2e870305e42353ea5bcf9ac1aaa79c1254f9222993c12d5"}},
        {387030,      uint256{"0xae375a05ca92fe78e2768352eebb358b12fc0c2c65263d7ac29e4fe723636f81"}},
        {390000,      uint256{"0xcd035c9899d22c414f79a345c1b96fd9342d1beb5f80f1dbad6a6244b5d3d5b8"}},
        {400000,      uint256{"0x7ae908b0c5351fae59fcff7ab4fe0e23f4e7630ed895822676f3ee551262d82d"}},
        {500000,      uint256{"0x92b5c16c99769dcad4c2d4548426037b35894ef57ff1bf2516575440e1f87d4f"}},
        {600000,      uint256{"0x69c4acf177368eeb40155e7b03d07b7a6579620320d5de2554db99d0f4908b97"}},
        {685000,      uint256{"0xa276d5697372e71f597dca34c40391747186ce3fda96ee1875376b4b0f625881"}},
        {700000,      uint256{"0x8b5806c169fb7d3345e9f02ee0a38538cc4ab5884177002c1e9528058c5eab40"}},
        {800000,      uint256{"0x71e29af1056d1e8e217382f433d017406db7f0e03eb1995429a9edb741120643"}},
        {900000,      uint256{"0x8757e0670d5db26a9b540c616ae1c208bda9f4c3b3270754a36c867aa238206b"}},
        {1000000,     uint256{"0x0ef9d1ce85a1e8209f735f1574bbe0ed0aaca34f0c6052a65443aada25be94a8"}},
        {1003123,     uint256{"0xf2ec975040b2a5b1a1bf0c722b685596755e6021680661589aa7f8585d283700"}},
        {1003124,     uint256{"0xd9d451b69134e2d7682014fb5366bb662b3e753b23722cb34326c09aa1c22762"}},
        {1003125,     uint256{"0x0faaf5119ab9eb3a22e0984d6cba6cebc8d7bae25342401c782ab4fa413c326e"}},
        {1003126,     uint256{"0x8f21fc3e383c5ec61dec1f171a0b49eea25dccbb28755214a0d45e73dccb7c56"}},
        {1003127,     uint256{"0x5aaf45ff165d066f84d55399fda3c4458234f94cf32b0cfdcc7f9bbcc814585d"}},
        {1100000,     uint256{"0xb726814d624b9a1b77e4edfb43ec4c8c47d5cfe4a2c7644812074fb5ac01f252"}},
        {1120000,     uint256{"0x8c33837e3657a73aa3a89fa9f31cc565b6d075ddcb246de1cf5d9db90574e344"}}
    });

    // TestNet
    static MapCheckpoints mapCheckpointsTestnet(MapCheckpoints::MapType{
        {     0,      hashGenesisBlockTestNet },
        {     1,      uint256{"0x0e2eecad99db0eab96abbd7e2de769d92483a090eefcefc014b802d31131a0ce"}},
        {   500,      uint256{"0x0000006939777fded9640797f3008d9fca5d6e177e440655ba10f8a900cabe61"}},
        {  1000,      uint256{"0x000004715d8818cea9c2e5e9a727eb2f950964eb0d1060e1d5effd44c2ca45df"}},
        {100000,      uint256{"0x1fdbb9642e997fa13df3b0c11c95e959a2606ef9bc6c431e942cf3fc74ed344d"}},
        {200000,      uint256{"0xf4072b1e5b7ede5b33c82045b13f225b41ff3d8262e03ea5ed9521290e2d5e42"}},
        {300000,      uint256{"0x448d74d70dea376576217ef72518f18f289ab4680f6714cdac8a3903f7a2cacf"}},
        {400000,      uint256{"0x09c3bd420fa43ab4e591b0629ed8fe0e86fc264939483d6b7cb0a59f05020953"}},
        {500000,      uint256{"0xae87c4f158e07623b88aa089f2de3e3437352873293febcfa1585b07e823d955"}},
        {600000,      uint256{"0x3c7dbe265d43da7834c3f291e031dda89ef6c74f2950f0af15acf33768831f91"}},
        {700000,      uint256{"0xa5bcfb2d5d52e8c0bdce1ae11019a7819d4d626e6836f1980fe6b5ce13c10039"}},
        {800000,      uint256{"0x13a2c603fbdb4ced718d6f7bba60b335651ddb832fbe8e11962e454c6625e20f"}},
        {900000,      uint256{"0xe5c4d6f1fbd90b6a2af9a02f1e947422a4c5a8756c34d7f0e45f57b341e47156"}},
        {1000000,     uint256{"0x806506a6eafe00e213c666a8c8fd14dac0c6d6a52e0f05a4d175633361e5e377"}},
        {1100000,     uint256{"0x397b5e6e0e95d74d7c01064feae627d11a2a99d08ebf91200dbb9d94b1d4ee26"}},
        {1200000,     uint256{"0x54e813b81516c1a6169ff81abaec2715e13b2ec0796db4fcc510be1e0805d21e"}},
        {1300000,     uint256{"0x75da223a32b31b3bbb1f32ab33ad5079b70698902ebed5594bebc02ffecb74a8"}}
    });

// clang-format on

bool CheckHardened(int nHeight, const uint256& hash)
{
    MapCheckpoints& checkpoints = (fTestNet ? mapCheckpointsTestnet : mapCheckpoints);

    uint256 foundHash(0);
    bool    found = checkpoints.get(nHeight, foundHash);
    if (!found)
        return true;
    return hash == foundHash;
}

int GetTotalBlocksEstimate()
{
    MapCheckpoints& checkpoints = (fTestNet ? mapCheckpointsTestnet : mapCheckpoints);

    MapCheckpoints::value_type r;
    if (checkpoints.back(r)) {
        return r.first;
    } else {
        return 0;
    }
}

CBlockIndex* GetLastCheckpoint(const std::map<uint256, CBlockIndex*>& mapBlockIndex)
{
    MapCheckpoints::MapType checkpoints =
        (fTestNet ? mapCheckpointsTestnet.getInternalMap() : mapCheckpoints.getInternalMap());

    BOOST_REVERSE_FOREACH(const MapCheckpoints::MapType::value_type& i, checkpoints)
    {
        const uint256&                                  hash = i.second;
        std::map<uint256, CBlockIndex*>::const_iterator t    = mapBlockIndex.find(hash);
        if (t != mapBlockIndex.end())
            return t->second;
    }
    return NULL;
}

// ppcoin: synchronized checkpoint (centrally broadcasted)
uint256          hashSyncCheckpoint    = 0;
uint256          hashPendingCheckpoint = 0;
CSyncCheckpoint  checkpointMessage;
CSyncCheckpoint  checkpointMessagePending;
uint256          hashInvalidCheckpoint = 0;
CCriticalSection cs_hashSyncCheckpoint;

// ppcoin: get last synchronized checkpoint
CBlockIndex* GetLastSyncCheckpoint()
{
    LOCK(cs_hashSyncCheckpoint);
    if (!mapBlockIndex.count(hashSyncCheckpoint))
        error("GetSyncCheckpoint: block index missing for current sync-checkpoint %s",
              hashSyncCheckpoint.ToString().c_str());
    else
        return boost::atomic_load(&mapBlockIndex[hashSyncCheckpoint]).get();
    return NULL;
}

// ppcoin: only descendant of current sync-checkpoint is allowed
bool ValidateSyncCheckpoint(uint256 hashCheckpoint)
{
    if (!mapBlockIndex.count(hashSyncCheckpoint))
        return error("ValidateSyncCheckpoint: block index missing for current sync-checkpoint %s",
                     hashSyncCheckpoint.ToString().c_str());
    if (!mapBlockIndex.count(hashCheckpoint))
        return error("ValidateSyncCheckpoint: block index missing for received sync-checkpoint %s",
                     hashCheckpoint.ToString().c_str());

    CBlockIndexSmartPtr pindexSyncCheckpoint = boost::atomic_load(&mapBlockIndex[hashSyncCheckpoint]);
    CBlockIndexSmartPtr pindexCheckpointRecv = boost::atomic_load(&mapBlockIndex[hashCheckpoint]);

    if (pindexCheckpointRecv->nHeight <= pindexSyncCheckpoint->nHeight) {
        // Received an older checkpoint, trace back from current checkpoint
        // to the same height of the received checkpoint to verify
        // that current checkpoint should be a descendant block
        CBlockIndexSmartPtr pindex = pindexSyncCheckpoint;
        while (pindex->nHeight > pindexCheckpointRecv->nHeight)
            if (!(pindex = pindex->pprev))
                return error("ValidateSyncCheckpoint: pprev null - block index structure failure");
        if (pindex->GetBlockHash() != hashCheckpoint) {
            hashInvalidCheckpoint = hashCheckpoint;
            return error("ValidateSyncCheckpoint: new sync-checkpoint %s is conflicting with current "
                         "sync-checkpoint %s",
                         hashCheckpoint.ToString().c_str(), hashSyncCheckpoint.ToString().c_str());
        }
        return false; // ignore older checkpoint
    }

    // Received checkpoint should be a descendant block of the current
    // checkpoint. Trace back to the same height of current checkpoint
    // to verify.
    CBlockIndexSmartPtr pindex = pindexCheckpointRecv;
    while (pindex->nHeight > pindexSyncCheckpoint->nHeight)
        if (!(pindex = pindex->pprev))
            return error("ValidateSyncCheckpoint: pprev2 null - block index structure failure");
    if (pindex->GetBlockHash() != hashSyncCheckpoint) {
        hashInvalidCheckpoint = hashCheckpoint;
        return error("ValidateSyncCheckpoint: new sync-checkpoint %s is not a descendant of current "
                     "sync-checkpoint %s",
                     hashCheckpoint.ToString().c_str(), hashSyncCheckpoint.ToString().c_str());
    }
    return true;
}

bool WriteSyncCheckpoint(const uint256& hashCheckpoint)
{
    CTxDB txdb;
    txdb.TxnBegin();
    if (!txdb.WriteSyncCheckpoint(hashCheckpoint)) {
        txdb.TxnAbort();
        return error("WriteSyncCheckpoint(): failed to write to db sync checkpoint %s",
                     hashCheckpoint.ToString().c_str());
    }
    if (!txdb.TxnCommit())
        return error("WriteSyncCheckpoint(): failed to commit to db sync checkpoint %s",
                     hashCheckpoint.ToString().c_str());

    Checkpoints::hashSyncCheckpoint = hashCheckpoint;
    return true;
}

bool AcceptPendingSyncCheckpoint()
{
    LOCK(cs_hashSyncCheckpoint);
    if (hashPendingCheckpoint != 0 && mapBlockIndex.count(hashPendingCheckpoint)) {
        if (!ValidateSyncCheckpoint(hashPendingCheckpoint)) {
            hashPendingCheckpoint = 0;
            checkpointMessagePending.SetNull();
            return false;
        }

        CTxDB               txdb;
        CBlockIndexSmartPtr pindexCheckpoint = boost::atomic_load(&mapBlockIndex[hashPendingCheckpoint]);
        if (!pindexCheckpoint->IsInMainChain()) {
            CBlock block;
            if (!block.ReadFromDisk(pindexCheckpoint.get()))
                return error("AcceptPendingSyncCheckpoint: ReadFromDisk failed for sync checkpoint %s",
                             hashPendingCheckpoint.ToString().c_str());
            if (!block.SetBestChain(txdb, pindexCheckpoint)) {
                hashInvalidCheckpoint = hashPendingCheckpoint;
                return error("AcceptPendingSyncCheckpoint: SetBestChain failed for sync checkpoint %s",
                             hashPendingCheckpoint.ToString().c_str());
            }
        }

        if (!WriteSyncCheckpoint(hashPendingCheckpoint))
            return error("AcceptPendingSyncCheckpoint(): failed to write sync checkpoint %s",
                         hashPendingCheckpoint.ToString().c_str());
        hashPendingCheckpoint = 0;
        checkpointMessage     = checkpointMessagePending;
        checkpointMessagePending.SetNull();
        printf("AcceptPendingSyncCheckpoint : sync-checkpoint at %s\n",
               hashSyncCheckpoint.ToString().c_str());
        // relay the checkpoint
        if (!checkpointMessage.IsNull()) {
            BOOST_FOREACH (CNode* pnode, vNodes)
                checkpointMessage.RelayTo(pnode);
        }
        return true;
    }
    return false;
}

// Automatically select a suitable sync-checkpoint
uint256 AutoSelectSyncCheckpoint()
{
    ConstCBlockIndexSmartPtr pindex = boost::atomic_load(&pindexBest);
    // Search backward for a block within max span and maturity window
    unsigned int nTS = TargetSpacing();
    while (pindex->pprev &&
           (pindex->GetBlockTime() + nCheckpointSpan * nTS >
                boost::atomic_load(&pindexBest)->GetBlockTime() ||
            pindex->nHeight + nCheckpointSpan > boost::atomic_load(&pindexBest)->nHeight)) {
        pindex = pindex->pprev;
    }
    return pindex->GetBlockHash();
}

// Check against synchronized checkpoint
bool CheckSync(const uint256& hashBlock, const CBlockIndex* pindexPrev)
{
    if (fTestNet)
        return true; // Testnet has no checkpoints
    int nHeight = pindexPrev->nHeight + 1;

    LOCK(cs_hashSyncCheckpoint);
    // sync-checkpoint should always be accepted block
    assert(mapBlockIndex.count(hashSyncCheckpoint));
    const CBlockIndex* pindexSync = boost::atomic_load(&mapBlockIndex[hashSyncCheckpoint]).get();

    if (nHeight > pindexSync->nHeight) {
        // trace back to same height as sync-checkpoint
        const CBlockIndex* pindex = pindexPrev;
        while (pindex->nHeight > pindexSync->nHeight)
            if (!(pindex = pindex->pprev.get()))
                return error("CheckSync: pprev null - block index structure failure");
        if (pindex->nHeight < pindexSync->nHeight || pindex->GetBlockHash() != hashSyncCheckpoint)
            return false; // only descendant of sync-checkpoint can pass check
    }
    if (nHeight == pindexSync->nHeight && hashBlock != hashSyncCheckpoint)
        return false; // same height with sync-checkpoint
    if (nHeight < pindexSync->nHeight && !mapBlockIndex.count(hashBlock))
        return false; // lower height than sync-checkpoint
    return true;
}

bool WantedByPendingSyncCheckpoint(uint256 hashBlock)
{
    LOCK(cs_hashSyncCheckpoint);
    if (hashPendingCheckpoint == 0)
        return false;
    if (hashBlock == hashPendingCheckpoint)
        return true;
    if (mapOrphanBlocks.count(hashPendingCheckpoint) &&
        hashBlock == WantedByOrphan(mapOrphanBlocks[hashPendingCheckpoint]))
        return true;
    return false;
}

// ppcoin: reset synchronized checkpoint to last hardened checkpoint
bool ResetSyncCheckpoint()
{
    LOCK(cs_hashSyncCheckpoint);
    MapCheckpoints::MapType mapCheckpointsCopy = mapCheckpoints.getInternalMap();
    const uint256&          hash               = mapCheckpointsCopy.rbegin()->second;
    if (mapBlockIndex.count(hash) && !mapBlockIndex[hash]->IsInMainChain()) {
        // checkpoint block accepted but not yet in main chain
        printf("ResetSyncCheckpoint: SetBestChain to hardened checkpoint %s\n", hash.ToString().c_str());
        CTxDB  txdb;
        CBlock block;
        if (!block.ReadFromDisk(boost::atomic_load(&mapBlockIndex[hash]).get()))
            return error("ResetSyncCheckpoint: ReadFromDisk failed for hardened checkpoint %s",
                         hash.ToString().c_str());
        if (!block.SetBestChain(txdb, boost::atomic_load(&mapBlockIndex[hash]))) {
            return error("ResetSyncCheckpoint: SetBestChain failed for hardened checkpoint %s",
                         hash.ToString().c_str());
        }
    } else if (!mapBlockIndex.count(hash)) {
        // checkpoint block not yet accepted
        hashPendingCheckpoint = hash;
        checkpointMessagePending.SetNull();
        printf("ResetSyncCheckpoint: pending for sync-checkpoint %s\n",
               hashPendingCheckpoint.ToString().c_str());
    }

    BOOST_REVERSE_FOREACH(const MapCheckpoints::MapType::value_type& i, mapCheckpointsCopy)
    {
        const uint256& hash = i.second;
        if (mapBlockIndex.count(hash) && mapBlockIndex[hash]->IsInMainChain()) {
            if (!WriteSyncCheckpoint(hash))
                return error("ResetSyncCheckpoint: failed to write sync checkpoint %s",
                             hash.ToString().c_str());
            printf("ResetSyncCheckpoint: sync-checkpoint reset to %s\n",
                   hashSyncCheckpoint.ToString().c_str());
            return true;
        }
    }

    return false;
}

void AskForPendingSyncCheckpoint(CNode* pfrom)
{
    LOCK(cs_hashSyncCheckpoint);
    if (pfrom && hashPendingCheckpoint != 0 && (!mapBlockIndex.count(hashPendingCheckpoint)) &&
        (!mapOrphanBlocks.count(hashPendingCheckpoint)))
        pfrom->AskFor(CInv(MSG_BLOCK, hashPendingCheckpoint));
}

bool SetCheckpointPrivKey(std::string strPrivKey)
{
    // Test signing a sync-checkpoint with genesis block
    CSyncCheckpoint checkpoint;
    checkpoint.hashCheckpoint = !fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet;
    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedSyncCheckpoint)checkpoint;
    checkpoint.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

    std::vector<unsigned char> vchPrivKey = ParseHex(strPrivKey);
    CKey                       key;
    key.SetPrivKey(
        CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(checkpoint.vchMsg.begin(), checkpoint.vchMsg.end()), checkpoint.vchSig))
        return false;

    // Test signing successful, proceed
    CSyncCheckpoint::strMasterPrivKey = strPrivKey;
    return true;
}

bool SendSyncCheckpoint(uint256 hashCheckpoint)
{
    CSyncCheckpoint checkpoint;
    checkpoint.hashCheckpoint = hashCheckpoint;
    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedSyncCheckpoint)checkpoint;
    checkpoint.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

    if (CSyncCheckpoint::strMasterPrivKey.empty())
        return error("SendSyncCheckpoint: Checkpoint master key unavailable.");
    std::vector<unsigned char> vchPrivKey = ParseHex(CSyncCheckpoint::strMasterPrivKey);
    CKey                       key;
    key.SetPrivKey(
        CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(checkpoint.vchMsg.begin(), checkpoint.vchMsg.end()), checkpoint.vchSig))
        return error("SendSyncCheckpoint: Unable to sign checkpoint, check private key?");

    if (!checkpoint.ProcessSyncCheckpoint(NULL)) {
        printf("WARNING: SendSyncCheckpoint: Failed to process checkpoint.\n");
        return false;
    }

    // Relay checkpoint
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
            checkpoint.RelayTo(pnode);
    }
    return true;
}

// Is the sync-checkpoint outside maturity window?
bool IsMatureSyncCheckpoint()
{
    LOCK(cs_hashSyncCheckpoint);
    // sync-checkpoint should always be accepted block
    assert(mapBlockIndex.count(hashSyncCheckpoint));
    int                nCbM       = CoinbaseMaturity();
    unsigned int       nSMA       = StakeMinAge();
    const CBlockIndex* pindexSync = boost::atomic_load(&mapBlockIndex[hashSyncCheckpoint]).get();
    return (nBestHeight >= pindexSync->nHeight + nCbM ||
            pindexSync->GetBlockTime() + nSMA < GetAdjustedTime());
}

int64_t GetLastCheckpointBlockHeight()
{
    MapCheckpoints::value_type lastValue;
    if (fTestNet) {
        if (mapCheckpointsTestnet.back(lastValue)) {
            return lastValue.first;
        } else {
            return 0;
        }
    } else {
        if (mapCheckpoints.back(lastValue)) {
            return lastValue.first;
        } else {
            return 0;
        }
    }
}

} // namespace Checkpoints

// ppcoin: sync-checkpoint master key
const std::string CSyncCheckpoint::strMasterPubKey = "04a18357665ed7a802dcf252ef528d3dc786da38653b51d1ab"
                                                     "8e9f4820b55aca807892a056781967315908ac205940ec9d6f"
                                                     "2fd0a85941966971eac7e475a27826";

std::string CSyncCheckpoint::strMasterPrivKey = "";

// ppcoin: verify signature of sync-checkpoint message
bool CSyncCheckpoint::CheckSignature()
{
    CKey key;
    if (!key.SetPubKey(ParseHex(CSyncCheckpoint::strMasterPubKey)))
        return error("CSyncCheckpoint::CheckSignature() : SetPubKey failed");
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return error("CSyncCheckpoint::CheckSignature() : verify signature failed");

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedSyncCheckpoint*)this;
    return true;
}

// ppcoin: process synchronized checkpoint
bool CSyncCheckpoint::ProcessSyncCheckpoint(CNode* pfrom)
{
    if (!CheckSignature())
        return false;

    LOCK(Checkpoints::cs_hashSyncCheckpoint);
    if (!mapBlockIndex.count(hashCheckpoint)) {
        // We haven't received the checkpoint chain, keep the checkpoint as pending
        Checkpoints::hashPendingCheckpoint    = hashCheckpoint;
        Checkpoints::checkpointMessagePending = *this;
        printf("ProcessSyncCheckpoint: pending for sync-checkpoint %s\n",
               hashCheckpoint.ToString().c_str());
        // Ask this guy to fill in what we're missing
        if (pfrom) {
            pfrom->PushGetBlocks(pindexBest.get(), hashCheckpoint);
            // ask directly as well in case rejected earlier by duplicate
            // proof-of-stake because getblocks may not get it this time
            pfrom->AskFor(CInv(MSG_BLOCK, mapOrphanBlocks.count(hashCheckpoint)
                                              ? WantedByOrphan(mapOrphanBlocks[hashCheckpoint])
                                              : hashCheckpoint));
        }
        return false;
    }

    if (!Checkpoints::ValidateSyncCheckpoint(hashCheckpoint))
        return false;

    CTxDB               txdb;
    CBlockIndexSmartPtr pindexCheckpoint = boost::atomic_load(&mapBlockIndex[hashCheckpoint]);
    if (!pindexCheckpoint->IsInMainChain()) {
        // checkpoint chain received but not yet main chain
        CBlock block;
        if (!block.ReadFromDisk(pindexCheckpoint.get()))
            return error("ProcessSyncCheckpoint: ReadFromDisk failed for sync checkpoint %s",
                         hashCheckpoint.ToString().c_str());
        if (!block.SetBestChain(txdb, pindexCheckpoint)) {
            Checkpoints::hashInvalidCheckpoint = hashCheckpoint;
            return error("ProcessSyncCheckpoint: SetBestChain failed for sync checkpoint %s",
                         hashCheckpoint.ToString().c_str());
        }
    }

    if (!Checkpoints::WriteSyncCheckpoint(hashCheckpoint))
        return error("ProcessSyncCheckpoint(): failed to write sync checkpoint %s",
                     hashCheckpoint.ToString().c_str());
    Checkpoints::checkpointMessage     = *this;
    Checkpoints::hashPendingCheckpoint = 0;
    Checkpoints::checkpointMessagePending.SetNull();
    printf("ProcessSyncCheckpoint: sync-checkpoint at %s\n", hashCheckpoint.ToString().c_str());
    return true;
}

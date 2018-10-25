// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/version.hpp>

#include "checkpoints.h"
#include "kernel.h"
#include "main.h"
#include "txdb.h"
#include "util.h"

std::unique_ptr<MDB_env, std::function<void(MDB_env*)>> dbEnv;
std::unique_ptr<MDB_dbi, std::function<void(MDB_dbi*)>> txdb; // global pointer for lmdb object instance

using namespace std;
using namespace boost;

boost::filesystem::path CTxDB::DB_DIR = "txlmdb";

std::atomic<uint64_t> mdb_txn_safe::num_active_txns{0};
std::atomic_flag      mdb_txn_safe::creation_gate = ATOMIC_FLAG_INIT;

// threshold_size is used for batch transactions
bool CTxDB::need_resize(uint64_t threshold_size)
{
    printf("CTxDB::%s\n", __func__);
#if defined(ENABLE_AUTO_RESIZE)
    MDB_envinfo mei;

    mdb_env_info(dbEnv.get(), &mei);

    MDB_stat mst;

    mdb_env_stat(dbEnv.get(), &mst);

    // size_used doesn't include data yet to be committed, which can be
    // significant size during batch transactions. For that, we estimate the size
    // needed at the beginning of the batch transaction and pass in the
    // additional size needed.
    uint64_t size_used = mst.ms_psize * mei.me_last_pgno;

    printf("DB map size:     %zu\n", mei.me_mapsize);
    printf("Space used:      %zu\n", size_used);
    printf("Space remaining: %zu\n", mei.me_mapsize - size_used);
    printf("Size threshold:  %zu\n", threshold_size);
    float resize_percent = DB_RESIZE_PERCENT;
    printf("Percent used: %.04f  Percent threshold: %.04f\n", ((double)size_used / mei.me_mapsize),
           resize_percent);

    if (threshold_size > 0) {
        if (mei.me_mapsize - size_used < threshold_size) {
            printf("Threshold met (size-based)\n");
            return true;
        } else
            return false;
    }

    if ((double)size_used / mei.me_mapsize > resize_percent) {
        printf("Threshold met (percent-based)\n");
        return true;
    }
    return false;
#else
    return false;
#endif
}

void lmdb_resized(MDB_env* env)
{
    // TODO: use RAII to restore allowing txns
    mdb_txn_safe::prevent_new_txns();

    printf("LMDB map resize detected.\n");

    MDB_envinfo mei;

    mdb_env_info(env, &mei);
    uint64_t old = mei.me_mapsize;

    mdb_txn_safe::wait_no_active_txns();

    int result = mdb_env_set_mapsize(env, 0);
    if (result)
        printf("Failed to set new mapsize: %d\n", result);

    mdb_env_info(env, &mei);
    uint64_t new_mapsize = mei.me_mapsize;

    std::stringstream ss;
    ss << "LMDB Mapsize increased."
       << "  Old: " << old / (1024 * 1024) << "MiB"
       << ", New: " << new_mapsize / (1024 * 1024) << "MiB";
    printf("%s\n", ss.str().c_str());

    mdb_txn_safe::allow_new_txns();
}

void CTxDB::do_resize(uint64_t increase_size)
{
    printf("CTxDB::%s\n", __func__);
    const uint64_t add_size = 1LL << 30;

    // check disk capacity
    try {
        boost::filesystem::path       path(GetDataDir() / DB_DIR);
        boost::filesystem::space_info si = boost::filesystem::space(path);
        if (si.available < add_size) {
            stringstream ss;
            ss << "!! WARNING: Insufficient free space to extend database !!: " << (si.available >> 20L)
               << " MB available, " << (add_size >> 20L) << " MB needed";
            throw std::runtime_error(ss.str());
        }
    } catch (...) {
        // print something but proceed.
        throw std::runtime_error("Unable to query free disk space.");
    }

    MDB_envinfo mei;

    mdb_env_info(dbEnv.get(), &mei);

    MDB_stat mst;

    mdb_env_stat(dbEnv.get(), &mst);

    // add 1Gb per resize, instead of doing a percentage increase
    uint64_t new_mapsize = (double)mei.me_mapsize + add_size;

    // If given, use increase_size instead of above way of resizing.
    // This is currently used for increasing by an estimated size at start of new
    // batch txn.
    if (increase_size > 0)
        new_mapsize = mei.me_mapsize + increase_size;

    new_mapsize += (new_mapsize % mst.ms_psize);

    mdb_txn_safe::prevent_new_txns();

    if (activeBatch) {
        throw std::runtime_error(
            "attempting resize with write transaction in progress, this should not happen!");
    }

    mdb_txn_safe::wait_no_active_txns();

    int result = mdb_env_set_mapsize(dbEnv.get(), new_mapsize);
    if (result)
        throw std::runtime_error("Failed to set new mapsize: " + std::to_string(result));

    std::stringstream ss;
    ss << "LMDB Mapsize increased."
       << "  Old: " << mei.me_mapsize / (1024 * 1024) << "MiB"
       << ", New: " << new_mapsize / (1024 * 1024) << "MiB";
    printf("%s", ss.str().c_str());

    mdb_txn_safe::allow_new_txns();
}

void CTxDB::init_blockindex(bool fRemoveOld)
{
    // First time init.
    filesystem::path directory = GetDataDir() / DB_DIR;

    if (fRemoveOld) {
        filesystem::remove_all(directory); // remove directory

        // delete block data files
        {
            unsigned int nFile = 1;

            while (true) {
                filesystem::path strBlockFile = GetDataDir() / strprintf("blk%04u.dat", nFile);

                // Break if no such file
                if (!filesystem::exists(strBlockFile))
                    break;

                filesystem::remove(strBlockFile);

                nFile++;
            }
        }

        // delete NTP1 transaction data files
        {
            unsigned int nFile = 1;

            while (true) {
                filesystem::path strBlockFile = GetDataDir() / strprintf("ntp1txs%04u.dat", nFile);

                // Break if no such file
                if (!filesystem::exists(strBlockFile))
                    break;

                filesystem::remove(strBlockFile);

                nFile++;
            }
        }
    }

    filesystem::create_directories(directory);
    printf("Opening lmdb in %s\n", directory.string().c_str());
    MDB_env* envPtr = nullptr;
    if (const int rc = mdb_env_create(&envPtr)) {
        std::cerr << "Error env create" << std::endl;
    }
    dbEnv = std::unique_ptr<MDB_env, std::function<void(MDB_env*)>>(envPtr, [](MDB_env* p) {
        if (p)
            mdb_env_close(p);
    });

    mdb_env_set_maxdbs(dbEnv.get(), 20);

    if (auto result = mdb_env_open(dbEnv.get(), directory.string().c_str(), /*MDB_NOTLS*/ 0, 0644)) {
        throw std::runtime_error("Failed to open lmdb environment: " + std::to_string(result) +
                                 "; message: " + std::string(mdb_strerror(result)));
    }

    MDB_envinfo mei;
    mdb_env_info(dbEnv.get(), &mei);
    std::size_t currMapSize = mei.me_mapsize;

    std::size_t mapSize = DB_DEFAULT_MAPSIZE;

    if (currMapSize < mapSize) {
        if (auto mapSizeErr = mdb_env_set_mapsize(dbEnv.get(), mapSize))
            throw std::runtime_error(
                "Error: set max memory map size failed: " + std::to_string(mapSizeErr) +
                "; message: " + std::string(mdb_strerror(mapSizeErr)));

        mdb_env_info(dbEnv.get(), &mei);
        currMapSize = (double)mei.me_mapsize;
        printf("LMDB memory map size: %zu\n", currMapSize);
    }

    if (CTxDB::need_resize()) {
        printf("LMDB memory map needs to be resized, doing that now.\n");
        CTxDB::do_resize();
    }

    mdb_txn_safe txn;
    if (auto mdb_res = mdb_txn_begin(dbEnv.get(), NULL, 0, txn)) {
        throw std::runtime_error(
            "Failed to create a transaction for the db: " + std::to_string(mdb_res) +
            "; message: " + std::string(mdb_strerror(mdb_res)));
    }

    txdb = std::unique_ptr<MDB_dbi, std::function<void(MDB_dbi*)>>(new MDB_dbi, [](MDB_dbi* p) {
        if (p) {
            mdb_close(dbEnv.get(), *p);
            delete p;
        }
    });

    CTxDB::lmdb_db_open(txn, LMDB_MAINDB.c_str(), MDB_CREATE, *txdb,
                        "Failed to open db handle for m_blocks");

    // commit the transaction
    txn.commit();

    if (!txdb) {
        throw std::runtime_error("LMDB nullptr after opening the database.");
    }
}

// CDB subclasses are created and destroyed VERY OFTEN. That's why
// we shouldn't treat this as a free operations.
CTxDB::CTxDB(const char* pszMode)
{
    assert(pszMode);
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    if (txdb) {
        db_main = txdb.get();
        return;
    }

    printf("Initializing lmdb with db size: %lu\n", DB_DEFAULT_MAPSIZE);
    bool fCreate = strchr(pszMode, 'c');

    //    options                   = GetOptions();
    //    options.create_if_missing = fCreate;
    //    options.filter_policy     = leveldb::NewBloomFilterPolicy(10);

    init_blockindex(); // Init directory
    db_main = txdb.get();

    if (Exists(string("version"), db_main)) {
        ReadVersion(nVersion);
        printf("Transaction index version is %d\n", nVersion);

        if (nVersion < DATABASE_VERSION) {
            printf("Required index version is %d, removing old database\n", DATABASE_VERSION);

            // lmdb instance destruction
            db_main = nullptr;
            txdb.reset();
            if (activeBatch) {
                activeBatch->abort();
                activeBatch.reset();
            }

            init_blockindex(true); // Remove directory and create new database
            db_main = txdb.get();

            bool fTmp = fReadOnly;
            fReadOnly = false;
            WriteVersion(DATABASE_VERSION); // Save transaction index version
            fReadOnly = fTmp;
        }
    } else if (fCreate) {
        bool fTmp = fReadOnly;
        fReadOnly = false;
        WriteVersion(DATABASE_VERSION);
        fReadOnly = fTmp;
    }

    printf("Opened LMDB successfully\n");
}

void CTxDB::Close()
{
    if (activeBatch) {
        activeBatch->abort();
        activeBatch.reset();
    }
    txdb.reset();
    db_main = nullptr;
}

void CTxDB::__deleteDb()
{
    try {
        boost::filesystem::remove_all(GetDataDir() / DB_DIR);
    } catch (...) {
    }
}

bool CTxDB::TxnBegin(size_t required_size)
{
    assert(activeBatch == nullptr);
    if (CTxDB::need_resize(required_size)) {
        printf("LMDB memory map needs to be resized, doing that now.\n");
        CTxDB::do_resize(required_size);
    }
    activeBatch = std::unique_ptr<mdb_txn_safe>(new mdb_txn_safe);
    if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, *activeBatch)) {
        printf("Failed to begin transaction at read with error code %i; with error: %s\n", res,
               mdb_strerror(res));
        activeBatch.reset();
    }
    return true;
}

bool CTxDB::TxnCommit()
{
    assert(activeBatch);
    if (activeBatch) {
        activeBatch->commit();
        activeBatch.reset();
    }
    return true;
}

bool CTxDB::TxnAbort()
{
    assert(activeBatch);
    if (activeBatch) {
        activeBatch->abort();
        activeBatch.reset();
    }
    return true;
}

bool CTxDB::WriteStrKeyVal(const string& key, const string& val) { return Write(key, val, db_main); }
bool CTxDB::ReadStrKeyVal(const string& key, string& val) { return Read(key, val, db_main); }
bool CTxDB::ExistsStrKeyVal(const string& key) { return Exists(key, db_main); }
bool CTxDB::EraseStrKeyVal(const string& key) { return Erase(key, db_main); }

bool CTxDB::ReadVersion(int& nVersion)
{
    nVersion = 0;
    return Read(std::string("version"), nVersion, db_main);
}

bool CTxDB::WriteVersion(int nVersion) { return Write(std::string("version"), nVersion, db_main); }

bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex, db_main);
}

bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    return Write(make_pair(string("tx"), hash), txindex, db_main);
}

bool CTxDB::ReadNTP1TxIndex(uint256 hash, DiskNTP1TxPos& txindex)
{
    txindex.SetNull();
    return Read(make_pair(string("ntp1tx"), hash), txindex, db_main);
}

bool CTxDB::WriteNTP1TxIndex(uint256 hash, const DiskNTP1TxPos& txindex)
{
    return Write(make_pair(string("ntp1tx"), hash), txindex, db_main);
}

bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int /*nHeight*/)
{
    // Add to tx index
    uint256  hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(make_pair(string("tx"), hash), txindex, db_main);
}

bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    uint256 hash = tx.GetHash();

    return Erase(make_pair(string("tx"), hash), db_main);
}

bool CTxDB::ContainsTx(uint256 hash) { return Exists(make_pair(string("tx"), hash), db_main); }

bool CTxDB::ContainsNTP1Tx(uint256 hash) { return Exists(make_pair(string("ntp1tx"), hash), db_main); }

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex, db_main);
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain, db_main);
}

bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain, db_main);
}

bool CTxDB::ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust)
{
    return Read(string("bnBestInvalidTrust"), bnBestInvalidTrust, db_main);
}

bool CTxDB::WriteBestInvalidTrust(CBigNum bnBestInvalidTrust)
{
    return Write(string("bnBestInvalidTrust"), bnBestInvalidTrust, db_main);
}

bool CTxDB::ReadSyncCheckpoint(uint256& hashCheckpoint)
{
    return Read(string("hashSyncCheckpoint"), hashCheckpoint, db_main);
}

bool CTxDB::WriteSyncCheckpoint(uint256 hashCheckpoint)
{
    return Write(string("hashSyncCheckpoint"), hashCheckpoint, db_main);
}

bool CTxDB::ReadCheckpointPubKey(string& strPubKey)
{
    return Read(string("strCheckpointPubKey"), strPubKey, db_main);
}

bool CTxDB::WriteCheckpointPubKey(const string& strPubKey)
{
    return Write(string("strCheckpointPubKey"), strPubKey, db_main);
}

static CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return nullptr;

    // Return existing
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi                    = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

std::string LmdbValToString(const MDB_val& val)
{
    return std::string((const char*)val.mv_data, val.mv_size);
}

bool CTxDB::LoadBlockIndex()
{
    if (mapBlockIndex.size() > 0) {
        // Already loaded once in this session. It can happen during migration
        // from BDB.
        return true;
    }

    // The block index is an in-memory structure that maps hashes to on-disk
    // locations where the contents of the block can be found. Here, we scan it
    // out of the DB and into mapBlockIndex.

    MDB_cursor*  cursorRawPtr = nullptr;
    mdb_txn_safe localTxn;
    if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
        return error("Failed to begin transaction at read with error code %i; and error: %s\n", res,
                     mdb_strerror(res));
    }
    if (auto rc = mdb_cursor_open(localTxn, *db_main, &cursorRawPtr)) {
        return error(
            "CTxDB::LoadBlockIndex() : Failed to open lmdb cursor with error code %d; and error: %s\n",
            rc, mdb_strerror(rc));
    }
    std::unique_ptr<MDB_cursor, std::function<void(MDB_cursor*)>> cursorPtr(cursorRawPtr,
                                                                            [](MDB_cursor* p) {
                                                                                if (p)
                                                                                    mdb_cursor_close(p);
                                                                            });

    // Seek to start key.
    CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
    ssStartKey << make_pair(string("blockindex"), uint256(0));
    std::string&& keyBin = ssStartKey.str();
    MDB_val       key    = {(size_t)ssStartKey.size(), (void*)keyBin.data()};
    MDB_val       data;

    int firstItemRes = mdb_cursor_get(cursorPtr.get(), &key, &data, MDB_SET_RANGE);
    if (firstItemRes != 0 && firstItemRes != MDB_NOTFOUND) {
        return error("Error while opening cursor to load index. Error code %i, and error: %s\n",
                     firstItemRes, mdb_strerror(firstItemRes));
    }

    // Now read each entry.
    do {
        if (firstItemRes) {
            break;
        }

        std::string keyStr = LmdbValToString(key);
        std::string valStr = LmdbValToString(data);

        // Unpack keys and values.
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(keyStr.data(), keyStr.size());
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.write(valStr.data(), valStr.size());
        string strType;
        ssKey >> strType;
        // Did we reach the end of the data to read?
        if (fRequestShutdown || strType != "blockindex")
            break;
        CDiskBlockIndex diskindex;
        ssValue >> diskindex;

        uint256 blockHash = diskindex.GetBlockHash();

        // Construct block index object
        CBlockIndex* pindexNew    = InsertBlockIndex(blockHash);
        pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
        pindexNew->pnext          = InsertBlockIndex(diskindex.hashNext);
        pindexNew->nFile          = diskindex.nFile;
        pindexNew->nBlockPos      = diskindex.nBlockPos;
        pindexNew->nHeight        = diskindex.nHeight;
        pindexNew->nMint          = diskindex.nMint;
        pindexNew->nMoneySupply   = diskindex.nMoneySupply;
        pindexNew->nFlags         = diskindex.nFlags;
        pindexNew->nStakeModifier = diskindex.nStakeModifier;
        pindexNew->prevoutStake   = diskindex.prevoutStake;
        pindexNew->nStakeTime     = diskindex.nStakeTime;
        pindexNew->hashProof      = diskindex.hashProof;
        pindexNew->nVersion       = diskindex.nVersion;
        pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
        pindexNew->nTime          = diskindex.nTime;
        pindexNew->nBits          = diskindex.nBits;
        pindexNew->nNonce         = diskindex.nNonce;

        // Watch for genesis block
        if (pindexGenesisBlock == nullptr &&
            blockHash == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
            pindexGenesisBlock = pindexNew;

        if (!pindexNew->CheckIndex()) {
            cursorPtr.reset();
            return error("LoadBlockIndex() : CheckIndex failed at %d", pindexNew->nHeight);
        }

        // NovaCoin: build setStakeSeen
        if (pindexNew->IsProofOfStake())
            setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));

    } while (mdb_cursor_get(cursorRawPtr, &key, &data, MDB_NEXT) == 0);
    cursorPtr.reset();
    localTxn.commit();

    if (fRequestShutdown)
        return true;

    // Calculate nChainTrust
    vector<pair<int, CBlockIndex*>> vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    BOOST_FOREACH (const PAIRTYPE(uint256, CBlockIndex*) & item, mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    BOOST_FOREACH (const PAIRTYPE(int, CBlockIndex*) & item, vSortedByHeight) {
        CBlockIndex* pindex = item.second;
        pindex->nChainTrust = (pindex->pprev ? pindex->pprev->nChainTrust : 0) + pindex->GetBlockTrust();
        // NovaCoin: calculate stake modifier checksum
        pindex->nStakeModifierChecksum = GetStakeModifierChecksum(pindex);
        if (!CheckStakeModifierCheckpoints(pindex->nHeight, pindex->nStakeModifierChecksum))
            return error("CTxDB::LoadBlockIndex() : Failed stake modifier checkpoint height=%d, "
                         "modifier=0x%016" PRIx64,
                         pindex->nHeight, pindex->nStakeModifier);
    }

    // Load hashBestChain pointer to end of best chain
    if (!ReadHashBestChain(hashBestChain)) {
        if (pindexGenesisBlock == nullptr)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }
    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");
    pindexBest      = mapBlockIndex[hashBestChain];
    nBestHeight     = pindexBest->nHeight;
    nBestChainTrust = pindexBest->nChainTrust;

    printf("LoadBlockIndex(): hashBestChain=%s  height=%d  trust=%s  date=%s\n",
           hashBestChain.ToString().substr(0, 20).c_str(), nBestHeight,
           CBigNum(nBestChainTrust).ToString().c_str(),
           DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());

    // NovaCoin: load hashSyncCheckpoint
    if (!ReadSyncCheckpoint(Checkpoints::hashSyncCheckpoint))
        return error("CTxDB::LoadBlockIndex() : hashSyncCheckpoint not loaded");
    printf("LoadBlockIndex(): synchronized checkpoint %s\n",
           Checkpoints::hashSyncCheckpoint.ToString().c_str());

    // Load bnBestInvalidTrust, OK if it doesn't exist
    CBigNum bnBestInvalidTrust;
    ReadBestInvalidTrust(bnBestInvalidTrust);
    nBestInvalidTrust = bnBestInvalidTrust.getuint256();

    // Verify blocks in the best chain
    int nCheckLevel = GetArg("-checklevel", 1);
    int nCheckDepth = GetArg("-checkblocks", 2500);
    if (nCheckDepth == 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > nBestHeight)
        nCheckDepth = nBestHeight;
    printf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CBlockIndex*                                        pindexFork = nullptr;
    map<pair<unsigned int, unsigned int>, CBlockIndex*> mapBlockPos;
    for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev) {
        if (fRequestShutdown || pindex->nHeight < nBestHeight - nCheckDepth)
            break;
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        // check level 1: verify block validity
        // check level 7: verify block signature too
        if (nCheckLevel > 0 && !block.CheckBlock(true, true, (nCheckLevel > 6))) {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n", pindex->nHeight,
                   pindex->GetBlockHash().ToString().c_str());
            pindexFork = pindex->pprev;
        }
        // check level 2: verify transaction index validity
        if (nCheckLevel > 1) {
            pair<unsigned int, unsigned int> pos = make_pair(pindex->nFile, pindex->nBlockPos);
            mapBlockPos[pos]                     = pindex;
            BOOST_FOREACH (const CTransaction& tx, block.vtx) {
                uint256  hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex)) {
                    // check level 3: checker transaction hashes
                    if (nCheckLevel > 2 || pindex->nFile != txindex.pos.nFile ||
                        pindex->nBlockPos != txindex.pos.nBlockPos) {
                        // either an error or a duplicate transaction
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos)) {
                            printf("LoadBlockIndex() : *** cannot read mislocated transaction %s\n",
                                   hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        } else if (txFound.GetHash() != hashTx) // not a duplicate tx
                        {
                            printf("LoadBlockIndex(): *** invalid tx position for %s\n",
                                   hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        }
                    }
                    // check level 4: check whether spent txouts were spent within the main chain
                    unsigned int nOutput = 0;
                    if (nCheckLevel > 3) {
                        BOOST_FOREACH (const CDiskTxPos& txpos, txindex.vSpent) {
                            if (!txpos.IsNull()) {
                                pair<unsigned int, unsigned int> posFind =
                                    make_pair(txpos.nFile, txpos.nBlockPos);
                                if (!mapBlockPos.count(posFind)) {
                                    printf("LoadBlockIndex(): *** found bad spend at %d, hashBlock=%s, "
                                           "hashTx=%s\n",
                                           pindex->nHeight, pindex->GetBlockHash().ToString().c_str(),
                                           hashTx.ToString().c_str());
                                    pindexFork = pindex->pprev;
                                }
                                // check level 6: check whether spent txouts were spent by a valid
                                // transaction that consume them
                                if (nCheckLevel > 5) {
                                    CTransaction txSpend;
                                    if (!txSpend.ReadFromDisk(txpos)) {
                                        printf("LoadBlockIndex(): *** cannot read spending transaction "
                                               "of %s:%i from disk\n",
                                               hashTx.ToString().c_str(), nOutput);
                                        pindexFork = pindex->pprev;
                                    } else if (!txSpend.CheckTransaction()) {
                                        printf("LoadBlockIndex(): *** spending transaction of %s:%i is "
                                               "invalid\n",
                                               hashTx.ToString().c_str(), nOutput);
                                        pindexFork = pindex->pprev;
                                    } else {
                                        bool fFound = false;
                                        BOOST_FOREACH (const CTxIn& txin, txSpend.vin)
                                            if (txin.prevout.hash == hashTx && txin.prevout.n == nOutput)
                                                fFound = true;
                                        if (!fFound) {
                                            printf("LoadBlockIndex(): *** spending transaction of %s:%i "
                                                   "does not spend it\n",
                                                   hashTx.ToString().c_str(), nOutput);
                                            pindexFork = pindex->pprev;
                                        }
                                    }
                                }
                            }
                            nOutput++;
                        }
                    }
                }
                // check level 5: check whether all prevouts are marked spent
                if (nCheckLevel > 4) {
                    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
                        CTxIndex txindex;
                        if (ReadTxIndex(txin.prevout.hash, txindex))
                            if (txindex.vSpent.size() - 1 < txin.prevout.n ||
                                txindex.vSpent[txin.prevout.n].IsNull()) {
                                printf("LoadBlockIndex(): *** found unspent prevout %s:%i in %s\n",
                                       txin.prevout.hash.ToString().c_str(), txin.prevout.n,
                                       hashTx.ToString().c_str());
                                pindexFork = pindex->pprev;
                            }
                    }
                }
            }
        }
    }
    if (pindexFork && !fRequestShutdown) {
        // Reorg back to the fork
        printf("LoadBlockIndex() : *** moving best chain pointer back to block %d\n",
               pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(pindexFork))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        CTxDB txdb;
        block.SetBestChain(txdb, pindexFork);
    }

    return true;
}

mdb_txn_safe::mdb_txn_safe(const bool check) : m_txn(nullptr), m_check(check)
{
    if (check) {
        while (creation_gate.test_and_set()) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        }
        num_active_txns++;
        creation_gate.clear();
    }
}

mdb_txn_safe::~mdb_txn_safe()
{
    if (!m_check)
        return;
    printf("mdb_txn_safe: destructor\n");
    if (m_txn != nullptr) {
        if (m_batch_txn) // this is a batch txn and should have been handled before this point for safety
        {
            printf("WARNING: mdb_txn_safe: m_txn is a batch txn and it's not NULL in destructor - "
                   "calling mdb_txn_abort()\n");
        } else {
            // Example of when this occurs: a lookup fails, so a read-only txn is
            // aborted through this destructor. However, successful read-only txns
            // ideally should have been committed when done and not end up here.
            //
            // NOTE: not sure if this is ever reached for a non-batch write
            // transaction, but it's probably not ideal if it did.
            printf("mdb_txn_safe: m_txn not NULL in destructor - calling mdb_txn_abort()\n");
        }
    }
    mdb_txn_abort(m_txn);

    num_active_txns--;
}

mdb_txn_safe& mdb_txn_safe::operator=(mdb_txn_safe&& other)
{
    m_txn             = other.m_txn;
    m_batch_txn       = other.m_batch_txn;
    m_check           = other.m_check;
    other.m_check     = false;
    other.m_txn       = nullptr;
    other.m_batch_txn = false;
    return *this;
}

mdb_txn_safe::mdb_txn_safe(mdb_txn_safe&& other)
    : m_txn(other.m_txn), m_batch_txn(other.m_batch_txn), m_check(other.m_check)
{
    other.m_check     = false;
    other.m_txn       = nullptr;
    other.m_batch_txn = false;
}

void mdb_txn_safe::uncheck()
{
    num_active_txns--;
    m_check = false;
}

void mdb_txn_safe::commit(std::string message)
{
    if (message.size() == 0) {
        message = "Failed to commit a transaction to the db";
    }

    if (auto result = mdb_txn_commit(m_txn)) {
        m_txn = nullptr;
        throw std::runtime_error(message + ": " + std::to_string(result));
    }
    m_txn = nullptr;
}

void mdb_txn_safe::commitIfValid(string message)
{
    if (m_txn) {
        commit(message);
    }
}

void mdb_txn_safe::abort()
{
    printf("mdb_txn_safe: abort()\n");
    if (m_txn != nullptr) {
        mdb_txn_abort(m_txn);
        m_txn = nullptr;
    } else {
        printf("WARNING: mdb_txn_safe: abort() called, but m_txn is NULL\n");
    }
}

void mdb_txn_safe::abortIfValid()
{
    if (m_txn) {
        abort();
    }
}

uint64_t mdb_txn_safe::num_active_tx() const { return num_active_txns; }

void mdb_txn_safe::prevent_new_txns()
{
    while (creation_gate.test_and_set()) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
    }
}

void mdb_txn_safe::wait_no_active_txns()
{
    while (num_active_txns > 0) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
    }
}

void mdb_txn_safe::allow_new_txns() { creation_gate.clear(); }

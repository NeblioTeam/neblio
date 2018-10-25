// Copyright (c) 2009-2012 The Bitcoin Developers.
// Authored by Google, Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LMDB_H
#define BITCOIN_LMDB_H

#include "main.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "liblmdb/lmdb.h"

#include "ntp1/ntp1transaction.h"

#define ENABLE_AUTO_RESIZE

extern std::unique_ptr<MDB_env, std::function<void(MDB_env*)>> dbEnv;
extern std::unique_ptr<MDB_dbi, std::function<void(MDB_dbi*)>>
    txdb; // global pointer for lmdb database object instance

constexpr static float DB_RESIZE_PERCENT = 0.9f;


// this custom size is used in tests
#ifndef CUSTOM_LMDB_DB_SIZE
#if defined(__arm__)
// force a value so it can compile with 32-bit ARM
constexpr static uint64_t DB_DEFAULT_MAPSIZE = UINT64_C(1) << 31;
#else
#if defined(ENABLE_AUTO_RESIZE)
constexpr static uint64_t DB_DEFAULT_MAPSIZE = UINT64_C(1) << 30;
#else
constexpr static uint64_t DB_DEFAULT_MAPSIZE = UINT64_C(1) << 33;
#endif
#endif
#else
constexpr static uint64_t DB_DEFAULT_MAPSIZE = CUSTOM_LMDB_DB_SIZE;
#endif

const std::string LMDB_MAINDB = "maindb";

class CTxDB;

void lmdb_resized(MDB_env* env);

inline int lmdb_txn_begin(MDB_env* env, MDB_txn* parent, unsigned int flags, MDB_txn** txn)
{
    int res = mdb_txn_begin(env, parent, flags, txn);
    if (res == MDB_MAP_RESIZED) {
        lmdb_resized(env);
        res = mdb_txn_begin(env, parent, flags, txn);
    }
    return res;
}

struct mdb_txn_safe
{
    mdb_txn_safe(const bool check = true);
    mdb_txn_safe(const mdb_txn_safe&) = delete;
    mdb_txn_safe& operator=(const mdb_txn_safe&) = delete;
    ~mdb_txn_safe();

    mdb_txn_safe(mdb_txn_safe&& other);
    mdb_txn_safe& operator=(mdb_txn_safe&& other);

    void commit(std::string message = "");
    void commitIfValid(std::string message = "");

    // This should only be needed for batch transaction which must be ensured to
    // be aborted before mdb_env_close, not after. So we can't rely on
    // BlockchainLMDB destructor to call mdb_txn_safe destructor, as that's too late
    // to properly abort, since mdb_env_close would have been called earlier.
    void abort();
    void abortIfValid();
    void uncheck();

    operator MDB_txn*() { return m_txn; }

    operator MDB_txn**() { return &m_txn; }

    MDB_txn* rawPtr() const { return m_txn; }

    uint64_t num_active_tx() const;

    static void prevent_new_txns();
    static void wait_no_active_txns();
    static void allow_new_txns();

    MDB_txn*                     m_txn;
    bool                         m_batch_txn = false;
    bool                         m_check;
    static std::atomic<uint64_t> num_active_txns;

    // could use a mutex here, but this should be sufficient.
    static std::atomic_flag creation_gate;
};

// Class that provides access to a LevelDB. Note that this class is frequently
// instantiated on the stack and then destroyed again, so instantiation has to
// be very cheap. Unfortunately that means, a CTxDB instance is actually just a
// wrapper around some global state.
//
// A LevelDB is a key/value store that is optimized for fast usage on hard
// disks. It prefers long read/writes to seeks and is based on a series of
// sorted key/value mapping files that are stacked on top of each other, with
// newer files overriding older files. A background thread compacts them
// together when too many files stack up.
//
// Learn more: http://code.google.com/p/leveldb/
class CTxDB
{
public:
    static boost::filesystem::path DB_DIR;

    CTxDB(const char* pszMode = "r+");
    ~CTxDB()
    {
        // Note that this is not the same as Close() because it deletes only
        // data scoped to this TxDB object.
        db_main = nullptr;
    }

    // Destroys the underlying shared global state accessed by this TxDB.
    void Close();

    static const int WriteReps = 32;

private:
    MDB_dbi* db_main; // Points to the global instance.

    // A batch stores up writes and deletes for atomic application. When this
    // field is non-NULL, writes/deletes go there instead of directly to disk.
    std::unique_ptr<mdb_txn_safe> activeBatch;
    bool                          fReadOnly;
    int                           nVersion;

protected:
public:
    static void __deleteDb();

    // Returns true and sets (value,false) if activeBatch contains the given key
    // or leaves value alone and sets deleted = true if activeBatch contains a
    // delete for it.
    //    bool ScanBatch(const CDataStream& key, std::string* value, bool* deleted) const;

    template <typename K, typename T>
    bool Read(const K& key, T& value, MDB_dbi* dbPtr)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string strValue;

        mdb_txn_safe localTxn(false);
        if (!activeBatch) {
            localTxn = mdb_txn_safe();
            if(auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error code: %s\n", res, mdb_strerror(res));
            }
        }
        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val     kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val     vS     = {0, nullptr};
        if (auto ret = mdb_get((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
            if (ret == MDB_NOTFOUND) {
                printf("Failed to read lmdb key %s as it doesn't exist\n", ssKey.str().c_str());
            } else {
                printf("Failed to read lmdb key with an unknown error of code %i; and error: %s\n", ret, mdb_strerror(ret));
            }
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }
        strValue.assign(static_cast<const char*>(vS.mv_data), vS.mv_size);
        // Unserialize value
        try {
            CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK,
                                CLIENT_VERSION);
            ssValue >> value;
        } catch (std::exception& e) {
            printf("Failed to deserialized data when reading for key %s\n", ssKey.str().c_str());
            return false;
        }
        return true;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, MDB_dbi* dbPtr)
    {
        if (fReadOnly){
            printf("Accessing lmdb write function in read only mode");
            assert("Write called on database in read-only mode");
            return false;
        }

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;

        // you can't resize the db when a tx is active
        if (!activeBatch && CTxDB::need_resize()) {
            printf("LMDB memory map needs to be resized, doing that now.\n");
            CTxDB::do_resize();
        }

        mdb_txn_safe localTxn(false);
        if (!activeBatch) {
            localTxn = mdb_txn_safe();
            if(auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res, mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        // TODO: bind to const reference to avoid copying
        std::string&& keyBin = ssKey.str();
        MDB_val     kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        std::string&& valBin = ssValue.str();
        MDB_val     vS     = {valBin.size(), (void*)(valBin.c_str())};

        if (auto ret = mdb_put((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS, 0)) {
            if (ret == MDB_MAP_FULL) {
                printf("Failed to write key %s with lmdb, MDB_MAP_FULL\n", ssKey.str().c_str());
            } else {
                printf("Failed to write key with lmdb, unknown reason\n");
            }
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }
        if (localTxn.rawPtr()) {
            localTxn.commitIfValid("Tx while writing");
        }
        return true;
    }

    template <typename K>
    bool Erase(const K& key, MDB_dbi* dbPtr)
    {
        if (!dbPtr)
            return false;
        if (fReadOnly) {
            printf("Accessing lmdb erase function in read-only mode.");
            assert("Erase called on database in read-only mode");
            return false;
        }

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        mdb_txn_safe localTxn(false);
        if (!activeBatch) {
            localTxn = mdb_txn_safe();
            if(auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res, mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val     kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val     vS{0, nullptr};

        if (auto ret = mdb_del((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
            printf("Failed to delete entry with key %s with lmdb\n", ssKey.str().c_str());
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }

        localTxn.commitIfValid("Tx while erasing");
        return true;
    }

    template <typename K>
    bool Exists(const K& key, MDB_dbi* dbPtr)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string unused;

        mdb_txn_safe localTxn(false);
        if (!activeBatch) {
            localTxn = mdb_txn_safe();
            if(auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res, mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val     kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val     vS{0, nullptr};

        if (auto ret = mdb_get((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            if (ret == MDB_NOTFOUND) {
                return false;
            } else {
                printf("Failed to check whether key %s exists with an unknown error of code %i; and error: %s\n",
                       ssKey.str().c_str(), ret, mdb_strerror(ret));
            }
            return false;
        } else {
            return true;
        }
    }

public:
    inline static void lmdb_db_open(MDB_txn* txn, const char* name, int flags, MDB_dbi& dbi,
                                    const std::string& error_string)
    {
        if (int res = mdb_dbi_open(txn, name, flags, &dbi)) {
            printf("Error opening lmdb database. Error code: %d; and error: %s\n", res, mdb_strerror(res));
            throw std::runtime_error(error_string + ": " + std::to_string(res));
        }
    }

    static bool need_resize(uint64_t threshold_size = 0);
    void        do_resize(uint64_t increase_size = 0);
    bool        TxnBegin(std::size_t required_size = 0);
    bool        TxnCommit();
    bool        TxnAbort();

    // for tests
    bool WriteStrKeyVal(const std::string& key, const std::string& val);
    bool ReadStrKeyVal(const std::string& key, std::string& val);
    bool ExistsStrKeyVal(const std::string& key);
    bool EraseStrKeyVal(const std::string& key);

    bool ReadVersion(int& nVersion);
    bool WriteVersion(int nVersion);
    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool ReadNTP1TxIndex(uint256 hash, DiskNTP1TxPos& txindex);
    bool WriteNTP1TxIndex(uint256 hash, const DiskNTP1TxPos& txindex);
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ContainsNTP1Tx(uint256 hash);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust);
    bool WriteBestInvalidTrust(CBigNum bnBestInvalidTrust);
    bool ReadSyncCheckpoint(uint256& hashCheckpoint);
    bool WriteSyncCheckpoint(uint256 hashCheckpoint);
    bool ReadCheckpointPubKey(std::string& strPubKey);
    bool WriteCheckpointPubKey(const std::string& strPubKey);
    bool LoadBlockIndex();

    void init_blockindex(bool fRemoveOld = false);

private:
    bool LoadBlockIndexGuts();
};

#endif // BITCOIN_LMDB_H

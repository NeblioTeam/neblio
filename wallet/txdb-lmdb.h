// Copyright (c) 2009-2012 The Bitcoin Developers.
// Authored by Google, Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LMDB_H
#define BITCOIN_LMDB_H

//#define DEEP_LMDB_LOGGING

#include <atomic>
#include <boost/filesystem.hpp>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "liblmdb/lmdb.h"

#include "diskblockindex.h"
#include "disktxpos.h"
#include "outpoint.h"
#include "txindex.h"
#include "util.h"

class NTP1Transaction;
class CBigNum;
class CBlock;
class CTransaction;

#define ENABLE_AUTO_RESIZE

// global environment pointer
extern std::unique_ptr<MDB_env, void (*)(MDB_env*)> dbEnv;
// global database pointers
using DbSmartPtrType = std::unique_ptr<MDB_dbi, void (*)(MDB_dbi*)>;
extern DbSmartPtrType glob_db_main;
extern DbSmartPtrType glob_db_blockIndex;
extern DbSmartPtrType glob_db_blocks;
extern DbSmartPtrType glob_db_tx;
extern DbSmartPtrType glob_db_ntp1Tx;
extern DbSmartPtrType glob_db_ntp1tokenNames;

const std::string LMDB_MAINDB           = "MainDb";
const std::string LMDB_BLOCKINDEXDB     = "BlockIndexDb";
const std::string LMDB_BLOCKSDB         = "BlocksDb";
const std::string LMDB_TXDB             = "TxDb";
const std::string LMDB_NTP1TXDB         = "Ntp1txDb";
const std::string LMDB_NTP1TOKENNAMESDB = "Ntp1NamesDb";

constexpr static float DB_RESIZE_PERCENT = 0.9f;

const std::string QuickSyncDataLink =
    "https://raw.githubusercontent.com/NeblioTeam/neblio-quicksync/master/download.json";

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

#define HAS_MEMBER_FUNC(func, name)                                                                     \
    template <typename T, typename Sign>                                                                \
    struct name                                                                                         \
    {                                                                                                   \
        typedef char yes[1];                                                                            \
        typedef char no[2];                                                                             \
        template <typename U, U>                                                                        \
        struct type_check;                                                                              \
        template <typename _1>                                                                          \
        static yes& chk(type_check<Sign, &_1::func>*);                                                  \
        template <typename>                                                                             \
        static no&        chk(...);                                                                     \
        static bool const value = sizeof(chk<T>(0)) == sizeof(yes);                                     \
    }

HAS_MEMBER_FUNC(toString, has_toString_class);
HAS_MEMBER_FUNC(ToString, has_ToString_class);

template <typename T>
constexpr bool Has_ToString()
{
    return has_ToString_class<T, std::string (T::*)()>::value;
}

template <typename T>
constexpr bool Has_toString()
{
    return has_toString_class<T, std::string (T::*)()>::value;
}

template <typename T>
typename std::enable_if<Has_ToString<T>() && Has_toString<T>(), std::string>::type
KeyAsString(const T& k, const std::string& /*keyStr*/)
{
    return k->ToString();
}

template <typename T>
typename std::enable_if<Has_ToString<T>() && !Has_toString<T>(), std::string>::type
KeyAsString(const T& k, const std::string& /*keyStr*/)
{
    return k->ToString();
}

template <typename T>
typename std::enable_if<!Has_ToString<T>() && Has_toString<T>(), std::string>::type
KeyAsString(const T& k, const std::string& /*keyStr*/)
{
    return k->toString();
}

template <typename T>
typename std::enable_if<!Has_ToString<T>() && !Has_toString<T>(), std::string>::type
KeyAsString(const T& /*t*/, const std::string& keyStr)
{
    if (std::all_of(keyStr.begin(), keyStr.end(), ::isprint)) {
        return keyStr;
    } else {
        return "(in hex: 0x" + boost::algorithm::hex(keyStr) + ")";
    }
}

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

    // this flag is useful for disabling quicksync manually, for example, for tests
    static bool QuickSyncHigherControl_Enabled;

    CTxDB(const char* pszMode = "r+");
    CTxDB(const CTxDB&) = delete;
    CTxDB(CTxDB&&)      = delete;
    CTxDB& operator=(const CTxDB&) = delete;
    CTxDB& operator=(CTxDB&&) = delete;
    ~CTxDB();

    // Destroys the underlying shared global state accessed by this TxDB.
    void Close();

    static void __deleteDb();

private:
    // Points to the global instance databases on construction.
    MDB_dbi* db_main;
    MDB_dbi* db_blockIndex;
    MDB_dbi* db_blocks;
    MDB_dbi* db_tx;
    MDB_dbi* db_ntp1Tx;
    MDB_dbi* db_ntp1tokenNames;

    // A batch stores up writes and deletes for atomic application. When this
    // field is non-NULL, writes/deletes go there instead of directly to disk.
    std::unique_ptr<mdb_txn_safe> activeBatch;
    bool                          fReadOnly;
    int                           nVersion;

    void (*dbDeleter)(MDB_dbi*) = [](MDB_dbi* p) {
        if (p) {
            mdb_close(dbEnv.get(), *p);
            delete p;
        }
    };

protected:
    // Returns true and sets (value,false) if activeBatch contains the given key
    // or leaves value alone and sets deleted = true if activeBatch contains a
    // delete for it.
    //    bool ScanBatch(const CDataStream& key, std::string* value, bool* deleted) const;

    template <typename K, typename T>
    bool Read(const K& key, T& value, MDB_dbi* dbPtr, int serializationTypeModifiers = 0,
              size_t offset = 0)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        mdb_txn_safe localTxn(false);
        if (!activeBatch) {
            localTxn = mdb_txn_safe();
            if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error code: %s\n",
                       res, mdb_strerror(res));
            }
        }
        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val       kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val       vS     = {0, nullptr};
        if (auto ret = mdb_get((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
            std::string dbgKey = KeyAsString(key, ssKey.str());

            if (ret == MDB_NOTFOUND) {
                printf("Failed to read lmdb key %s as it doesn't exist\n", dbgKey.c_str());
            } else {
                printf("Failed to read lmdb key %s with an unknown error of code %i; and error: %s\n",
                       dbgKey.c_str(), ret, mdb_strerror(ret));
            }
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }
        // Unserialize value
        assert(offset <= vS.mv_size);
        assert(vS.mv_data != nullptr);
        try {
            CDataStream ssValue(static_cast<const char*>(vS.mv_data) + offset,
                                static_cast<const char*>(vS.mv_data) + vS.mv_size,
                                SER_DISK | serializationTypeModifiers, CLIENT_VERSION);
            ssValue >> value;
        } catch (std::exception& e) {
            printf("Failed to deserialized data when reading for key %s\n", ssKey.str().c_str());
            return false;
        }
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
        return true;
    }

    /**
     * ReadMultiple key/value pairs, either starting at "key" or just all the keys in the db. If readAll
     * is true, everything in the db will be read
     */
    template <typename K, typename T, template <typename, typename = std::allocator<T>> class Container>
    bool ReadMultiple(const K& key, Container<T>& values, bool readAll, MDB_dbi* dbPtr)
    {
        values.clear();

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        mdb_txn_safe localTxn(false);
        if (!activeBatch) {
            localTxn = mdb_txn_safe();
            if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error code: %s\n",
                       res, mdb_strerror(res));
            }
        }
        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin       = ssKey.str();
        MDB_val       kS           = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val       vS           = {0, nullptr};
        MDB_cursor*   cursorRawPtr = nullptr;
        if (auto rc = mdb_cursor_open((!activeBatch ? localTxn : *activeBatch), *dbPtr, &cursorRawPtr)) {
            return error("ReadMultiple: Failed to open lmdb cursor with error code %d; and error: %s\n",
                         rc, mdb_strerror(rc));
        }

        std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
            if (p)
                mdb_cursor_close(p);
        });

        int itemRes = 1;

        if (readAll) {
            // read all items in that database
            itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_FIRST);
        } else {
            // read only starting at some valud
            itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_SET);
        }
        if (itemRes) {
            std::string dbgKey = KeyAsString(key, ssKey.str());
            if (itemRes != 0 && itemRes != MDB_NOTFOUND) {
                printf("txdb-lmdb: Cursor with key %s does not exist; with an error of code %i; and "
                       "error: %s\n",
                       dbgKey.c_str(), itemRes, mdb_strerror(itemRes));
                if (localTxn.rawPtr()) {
                    localTxn.abort();
                }
                return false;
            }
        }
        do {
            // if the first item is empty, break immediately
            if (itemRes) {
                break;
            }

            // Unserialize value
            assert(vS.mv_data != nullptr);
            try {
                CDataStream ssValue(static_cast<const char*>(vS.mv_data),
                                    static_cast<const char*>(vS.mv_data) + vS.mv_size, SER_DISK,
                                    CLIENT_VERSION);
                T           value;
                ssValue >> value;
                values.insert(values.end(), value);
            } catch (std::exception& e) {
                unsigned int sz = static_cast<unsigned int>(values.size());
                printf("Failed to deserialized element number %u in lmdb ReadMultiple() data when "
                       "reading for key %s\n",
                       sz, ssKey.str().c_str());
                return false;
            }

            itemRes = mdb_cursor_get(cursorRawPtr, &kS, &vS, MDB_NEXT);
        } while (itemRes == 0);

        cursorPtr.reset();
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
        return true;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, MDB_dbi* dbPtr)
    {
        if (fReadOnly) {
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
            if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res,
                       mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val       kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        std::string&& valBin = ssValue.str();
        MDB_val       vS     = {valBin.size(), (void*)(valBin.c_str())};

        if (auto ret = mdb_put((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS, 0)) {
            std::string dbgKey = KeyAsString(key, ssKey.str());
            if (ret == MDB_MAP_FULL) {
                printf("Failed to write key %s with lmdb, MDB_MAP_FULL\n", dbgKey.c_str());
            } else {
                printf("Failed to write key %s with lmdb; Code %i; Error: %s\n", dbgKey.c_str(), ret,
                       mdb_strerror(ret));
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

    template <typename K, typename T>
    bool WriteMultiple(const K& key, const T& value, MDB_dbi* dbPtr)
    {
        if (fReadOnly) {
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
            if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res,
                       mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin       = ssKey.str();
        MDB_val       kS           = {keyBin.size(), (void*)(keyBin.c_str())};
        std::string&& valBin       = ssValue.str();
        MDB_val       vS           = {valBin.size(), (void*)(valBin.c_str())};
        MDB_cursor*   cursorRawPtr = nullptr;
        if (auto rc = mdb_cursor_open((!activeBatch ? localTxn : *activeBatch), *dbPtr, &cursorRawPtr)) {
            return error("ReadMultiple: Failed to open lmdb cursor with error code %d; and error: %s\n",
                         rc, mdb_strerror(rc));
        }

        std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
            if (p)
                mdb_cursor_close(p);
        });

        if (auto ret = mdb_cursor_put(cursorPtr.get(), &kS, &vS, 0)) {
            std::string dbgKey = KeyAsString(key, ssKey.str());
            if (ret == MDB_MAP_FULL) {
                printf("Failed to write key %s with lmdb, MDB_MAP_FULL\n", dbgKey.c_str());
            } else {
                printf("Failed to write key %s with lmdb; Code %i; Error: %s\n", dbgKey.c_str(), ret,
                       mdb_strerror(ret));
            }
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }

        cursorPtr.reset();
        localTxn.commitIfValid("Tx while writing");
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
            if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res,
                       mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val       kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val       vS{0, nullptr};

        if (auto ret = mdb_del((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
            std::string dbgKey = KeyAsString(key, ssKey.str());
            printf("Failed to delete entry with key %s with lmdb; Code %i; Error message: %s\n",
                   dbgKey.c_str(), ret, mdb_strerror(ret));
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }

        localTxn.commitIfValid("Tx while erasing");
        return true;
    }

    template <typename K>
    bool EraseAll(const K& key, MDB_dbi* dbPtr)
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
            if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res,
                       mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val       kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val       vS{0, nullptr};

        MDB_cursor* cursorRawPtr = nullptr;
        if (auto rc = mdb_cursor_open((!activeBatch ? localTxn : *activeBatch), *dbPtr, &cursorRawPtr)) {
            return error("EraseDup: Failed to open lmdb cursor with error code %d; and error: %s\n", rc,
                         mdb_strerror(rc));
        }

        std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
            if (p)
                mdb_cursor_close(p);
        });

        int itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_SET);
        if (itemRes) {
            std::string dbgKey = KeyAsString(key, ssKey.str());
            if (itemRes != 0) {
                printf("Failed to erase lmdb key %s with an error of code %i; and error: %s\n",
                       dbgKey.c_str(), itemRes, mdb_strerror(itemRes));
            }
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }

        if (auto ret = mdb_cursor_del(cursorPtr.get(), MDB_NODUPDATA)) {
            std::string dbgKey = KeyAsString(key, ssKey.str());
            printf("Failed to delete entry with key %s with lmdb; Code %i; Error message: %s\n",
                   dbgKey.c_str(), ret, mdb_strerror(ret));
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return false;
        }

        cursorPtr.reset();
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
            if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
                printf("Failed to begin transaction at read with error code %i; and error: %s\n", res,
                       mdb_strerror(res));
            }
        }

        // only one of them should be active
        assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

        std::string&& keyBin = ssKey.str();
        MDB_val       kS     = {keyBin.size(), (void*)(keyBin.c_str())};
        MDB_val       vS{0, nullptr};

        if (auto ret = mdb_get((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            std::string dbgKey = KeyAsString(key, ssKey.str());
            if (ret == MDB_NOTFOUND) {
                return false;
            } else {
                printf("Failed to check whether key %s exists with an unknown error of code %i; and "
                       "error: %s\n",
                       dbgKey.c_str(), ret, mdb_strerror(ret));
            }
            return false;
        } else {
            if (localTxn.rawPtr()) {
                localTxn.abort();
            }
            return true;
        }
    }

public:
    inline static void lmdb_db_open(MDB_txn* txn, const char* name, int flags, MDB_dbi& dbi,
                                    const std::string& error_string)
    {
        if (int res = mdb_dbi_open(txn, name, flags, &dbi)) {
            printf("Error opening lmdb database. Error code: %d; and error: %s\n", res,
                   mdb_strerror(res));
            throw std::runtime_error(error_string + ": " + std::to_string(res));
        }
    }

    static bool need_resize(uint64_t threshold_size = 0);
    void        do_resize(uint64_t increase_size = 0);
    bool        TxnBegin(std::size_t required_size = 0);
    bool        TxnCommit();
    bool        TxnAbort();

    // for tests
    bool test1_WriteStrKeyVal(const std::string& key, const std::string& val);
    bool test1_ReadStrKeyVal(const std::string& key, std::string& val);
    bool test1_ExistsStrKeyVal(const std::string& key);
    bool test1_EraseStrKeyVal(const std::string& key);

    bool test2_ReadMultipleStr1KeyVal(const std::string& key, std::vector<std::string>& val);
    bool test2_WriteStrKeyVal(const std::string& key, const std::string& val);
    bool test2_ExistsStrKeyVal(const std::string& key);
    bool test2_EraseStrKeyVal(const std::string& key);

    bool ReadVersion(int& nVersion);
    bool WriteVersion(int nVersion);
    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool ReadTx(const CDiskTxPos& txPos, CTransaction& tx);
    bool ReadNTP1Tx(uint256 hash, NTP1Transaction& ntp1tx);
    bool WriteNTP1Tx(uint256 hash, const NTP1Transaction& ntp1tx);
    bool ReadAllIssuanceTxs(std::vector<uint256>& txs);
    bool ReadNTP1TxsWithTokenSymbol(const std::string& tokenName, std::vector<uint256>& txs);
    bool WriteNTP1TxWithTokenSymbol(const std::string& tokenName, const NTP1Transaction& tx);
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ContainsNTP1Tx(uint256 hash);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool ReadBlock(uint256 hash, CBlock& blk, bool fReadTransactions = true);
    bool WriteBlock(uint256 hash, const CBlock& blk);
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

    inline void        loadDbPointers();
    inline void        resetDbPointers();
    static inline void resetGlobalDbPointers();
};

void CTxDB::loadDbPointers()
{
    db_main           = glob_db_main.get();
    db_blockIndex     = glob_db_blockIndex.get();
    db_blocks         = glob_db_blocks.get();
    db_tx             = glob_db_tx.get();
    db_ntp1Tx         = glob_db_ntp1Tx.get();
    db_ntp1tokenNames = glob_db_ntp1tokenNames.get();
}

void CTxDB::resetDbPointers()
{
    db_main           = nullptr;
    db_blockIndex     = nullptr;
    db_blocks         = nullptr;
    db_tx             = nullptr;
    db_ntp1Tx         = nullptr;
    db_ntp1tokenNames = nullptr;
}

void CTxDB::resetGlobalDbPointers()
{
    glob_db_main.reset();
    glob_db_blockIndex.reset();
    glob_db_blocks.reset();
    glob_db_tx.reset();
    glob_db_ntp1Tx.reset();
    glob_db_ntp1tokenNames.reset();

    dbEnv.reset();
}

#endif // BITCOIN_LMDB_H

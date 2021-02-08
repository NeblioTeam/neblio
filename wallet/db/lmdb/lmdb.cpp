#include "lmdb.h"

#include "../defaultlogger/defaultlogger.h"
#include "stringmanip.h"
#include "ui_interface.h"
#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>

std::unique_ptr<MDB_env, void (*)(MDB_env*)> dbEnv(nullptr, [](MDB_env*) {});

std::unique_ptr<__lmdb_db_pointers> glob_lmdb_db_pointers;

const std::string LMDB_MAINDB           = "MainDb";
const std::string LMDB_BLOCKINDEXDB     = "BlockIndexDb";
const std::string LMDB_BLOCKSDB         = "BlocksDb";
const std::string LMDB_TXDB             = "TxDb";
const std::string LMDB_NTP1TXDB         = "Ntp1txDb";
const std::string LMDB_NTP1TOKENNAMESDB = "Ntp1NamesDb";
const std::string LMDB_ADDRSVSPUBKEYSDB = "AddrsVsPubKeysDb";

namespace {

#define ENABLE_AUTO_RESIZE

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

constexpr static float    DB_RESIZE_PERCENT     = 0.9f;
constexpr static uint64_t MIN_MAP_SIZE_INCREASE = UINT64_C(1) << 28; // ~256 MiB

static void resetGlobalDbPointers()
{
    glob_lmdb_db_pointers.reset();
    dbEnv.reset();
}

void (*dbDeleter)(MDB_dbi*) = [](MDB_dbi* p) {
    if (p) {
        mdb_close(dbEnv.get(), *p);
        delete p;
    }
};

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

void lmdb_db_open(ILog* logger, MDB_txn* txn, const char* name, int flags, MDB_dbi& dbi,
                  const std::string& error_string)
{
    if (int res = mdb_dbi_open(txn, name, flags, &dbi)) {
        logger->logWrite("Error opening lmdb database. Error code: " + std::to_string(res) +
                         "; and error: " + std::string(mdb_strerror(res)));
        throw std::runtime_error(error_string + ": " + std::to_string(res));
    }
}

void lmdb_resized(ILog* logger, MDB_env* env)
{
    logger->logWrite(std::string(__func__));
    LMDBTransaction::prevent_new_txns();
    BOOST_SCOPE_EXIT(void) { LMDBTransaction::allow_new_txns(); }
    BOOST_SCOPE_EXIT_END

    logger->logWrite("LMDB map resize detected.\n");

    MDB_envinfo mei;

    mdb_env_info(env, &mei);
    const uint64_t old = mei.me_mapsize;

    LMDBTransaction::wait_no_active_txns();

    const int result = mdb_env_set_mapsize(env, 0);
    if (result)
        logger->logWrite("Failed to set new mapsize: " + std::to_string(result));

    mdb_env_info(env, &mei);
    const uint64_t new_mapsize = mei.me_mapsize;

    std::stringstream ss;
    ss << "LMDB Mapsize increased."
       << "  Old: " << old / (1024 * 1024) << " MiB"
       << ", New: " << new_mapsize / (1024 * 1024) << " MiB";
    logger->logWrite(ss.str());
}

int lmdb_txn_begin(ILog* logger, MDB_env* env, MDB_txn* parent, unsigned int flags, MDB_txn** txn)
{
    int res = mdb_txn_begin(env, parent, flags, txn);
    if (res == MDB_MAP_RESIZED) {
        lmdb_resized(logger, env);
        res = mdb_txn_begin(env, parent, flags, txn);
    }
    return res;
}

// threshold_size is used for batch transactions
static bool need_resize(ILog* logger, uint64_t threshold_size = 0)
{
#ifdef DEEP_LMDB_LOGGING
    logger->logWrite("LMDB: " + std::string(__func__));
#endif
#if defined(ENABLE_AUTO_RESIZE)
    MDB_envinfo mei;

    mdb_env_info(dbEnv.get(), &mei);

    MDB_stat mst;

    mdb_env_stat(dbEnv.get(), &mst);

    // size_used doesn't include data yet to be committed, which can be
    // significant size during batch transactions. For that, we estimate the size
    // needed at the beginning of the batch transaction and pass in the
    // additional size needed.
    const uint64_t size_used = mst.ms_psize * mei.me_last_pgno;

#ifdef DEEP_LMDB_LOGGING
    logger->logWrite("Checking if resize is needed.");
    logger->logWrite("DB map size:     " + std::to_string(mei.me_mapsize));
    logger->logWrite("Space used:      " + std::to_string(size_used));
    logger->logWrite("Space remaining: " + std::to_string(mei.me_mapsize - size_used));
    logger->logWrite("Size threshold:  " + std::to_string(threshold_size));
#endif
    const float resize_percent = DB_RESIZE_PERCENT;
#ifdef DEEP_LMDB_LOGGING
    logger->logWrite("Percent used: %.04f  Percent threshold: %.04f\n",
                     ((double)size_used / mei.me_mapsize), resize_percent);
#endif

    if (threshold_size > 0) {
        if (mei.me_mapsize - size_used < threshold_size) {
            logger->logWrite("Threshold met (size-based)");
            return true;
        } else
            return false;
    }

    if ((double)size_used / mei.me_mapsize > resize_percent) {
        logger->logWrite("Mapsize threshold met (percent-based)");
        return true;
    }
    return false;
#else
    return false;
#endif
}
} // namespace

void LMDB::openDatabase(const boost::filesystem::path& directory, bool clearDBBeforeOpen)
{
    if (clearDBBeforeOpen) {
        clearDBData();
    }

    logger->logWrite("Opening the blockchain database...\n");
    uiInterface.InitMessage("Opening the blockchain database...");

    // open the database in the traditional way (whether quicksync succeeded or not)
    boost::filesystem::create_directories(directory);
    logger->logWrite("Opening lmdb in " + directory.string());
    MDB_env* envPtr = nullptr;
    if (const int rc = mdb_env_create(&envPtr)) {
        throw std::runtime_error("Error creating lmdb environment: " + std::to_string(rc) +
                                 "; message: " + std::string(mdb_strerror(rc)));
    }
    dbEnv = std::unique_ptr<MDB_env, void (*)(MDB_env*)>(envPtr, [](MDB_env* p) {
        if (p)
            mdb_env_close(p);
    });

    mdb_env_set_maxdbs(dbEnv.get(), 20);

    if (auto result = mdb_env_open(dbEnv.get(), PossiblyWideStringToString(directory.native()).c_str(),
                                   /*MDB_NOTLS*/ 0, 0644)) {
        throw std::runtime_error("Failed to open lmdb environment: " + std::to_string(result) +
                                 "; message: " + std::string(mdb_strerror(result)));
    }

    MDB_envinfo mei;
    mdb_env_info(dbEnv.get(), &mei);
    std::size_t currMapSize = mei.me_mapsize;

    const std::size_t mapSize = DB_DEFAULT_MAPSIZE;

    if (currMapSize < mapSize) {
        if (auto mapSizeErr = mdb_env_set_mapsize(dbEnv.get(), mapSize))
            throw std::runtime_error(
                "Error: set max memory map size failed: " + std::to_string(mapSizeErr) +
                "; message: " + std::string(mdb_strerror(mapSizeErr)));

        mdb_env_info(dbEnv.get(), &mei);
        currMapSize = (double)mei.me_mapsize;
        logger->logWrite("LMDB memory map size: " + std::to_string(currMapSize));
    }

    if (need_resize(logger)) {
        logger->logWrite("LMDB memory map needs to be resized, doing that now.");
        doResize();
    }

    LMDBTransaction txn;
    if (auto mdb_res = mdb_txn_begin(dbEnv.get(), NULL, 0, txn)) {
        throw std::runtime_error(
            "Failed to create a transaction for the db: " + std::to_string(mdb_res) +
            "; message: " + std::string(mdb_strerror(mdb_res)));
    }

    glob_lmdb_db_pointers = std::unique_ptr<__lmdb_db_pointers>(new __lmdb_db_pointers);

    glob_lmdb_db_pointers->db_main           = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_blockIndex     = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_blocks         = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_tx             = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_ntp1Tx         = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_ntp1tokenNames = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_addrsVsPubKeys = DbSmartPtrType(new MDB_dbi, dbDeleter);

    // MDB_CREATE: Create the named database if it doesn't exist.
    lmdb_db_open(logger, txn, LMDB_MAINDB.c_str(), MDB_CREATE, *glob_lmdb_db_pointers->db_main,
                 "Failed to open db handle for db_main");
    lmdb_db_open(logger, txn, LMDB_BLOCKINDEXDB.c_str(), MDB_CREATE,
                 *glob_lmdb_db_pointers->db_blockIndex, "Failed to open db handle for db_blockIndex");
    lmdb_db_open(logger, txn, LMDB_BLOCKSDB.c_str(), MDB_CREATE, *glob_lmdb_db_pointers->db_blocks,
                 "Failed to open db handle for db_blocks");
    lmdb_db_open(logger, txn, LMDB_TXDB.c_str(), MDB_CREATE, *glob_lmdb_db_pointers->db_tx,
                 "Failed to open db handle for glob_db_tx");
    lmdb_db_open(logger, txn, LMDB_NTP1TXDB.c_str(), MDB_CREATE, *glob_lmdb_db_pointers->db_ntp1Tx,
                 "Failed to open db handle for glob_db_ntp1Tx");
    lmdb_db_open(logger, txn, LMDB_NTP1TOKENNAMESDB.c_str(), MDB_CREATE | MDB_DUPSORT,
                 *glob_lmdb_db_pointers->db_ntp1tokenNames,
                 "Failed to open db handle for glob_db_ntp1Tx");
    lmdb_db_open(logger, txn, LMDB_ADDRSVSPUBKEYSDB.c_str(), MDB_CREATE,
                 *glob_lmdb_db_pointers->db_addrsVsPubKeys,
                 "Failed to open db handle for glob_db_ntp1Tx");

    // commit the transaction
    txn.commit();

    if (!glob_lmdb_db_pointers->db_main) {
        throw std::runtime_error("LMDB nullptr after opening the db_main database.");
    }
    if (!glob_lmdb_db_pointers->db_blockIndex) {
        throw std::runtime_error("LMDB nullptr after opening the db_blockIndex database.");
    }
    if (!glob_lmdb_db_pointers->db_blocks) {
        throw std::runtime_error("LMDB nullptr after opening the db_blocks database.");
    }
    if (!glob_lmdb_db_pointers->db_tx) {
        throw std::runtime_error("LMDB nullptr after opening the db_tx database.");
    }
    if (!glob_lmdb_db_pointers->db_ntp1Tx) {
        throw std::runtime_error("LMDB nullptr after opening the db_ntp1Tx database.");
    }
    if (!glob_lmdb_db_pointers->db_ntp1tokenNames) {
        throw std::runtime_error("LMDB nullptr after opening the db_ntp1tokenNames database.");
    }
    if (!glob_lmdb_db_pointers->db_addrsVsPubKeys) {
        throw std::runtime_error("LMDB nullptr after opening the db_addrsVsPubKeys database.");
    }

    boost::atomic_thread_fence(boost::memory_order_seq_cst);

    logger->logWrite("Done opening the database");
    uiInterface.InitMessage("Done opening the database");
}

void LMDB::doResize(uint64_t increase_size)
{
    logger->logWrite("LMDB::" + std::string(__func__));

    if (increase_size != 0 && increase_size < MIN_MAP_SIZE_INCREASE) {
        // protect from having very small incremental changes in the DB size, which is not efficient
        increase_size = MIN_MAP_SIZE_INCREASE;
    }

    const uintmax_t add_size = UINTMAX_C(1) << 30;

    // check disk capacity
    try {
        boost::filesystem::space_info si = boost::filesystem::space(*dbdir);
        if (si.available < add_size) {
            std::stringstream ss;
            ss << "!! WARNING: Insufficient free space to extend database !!: "
               << (si.available >> UINTMAX_C(20)) << " MB available, " << (add_size >> UINTMAX_C(20))
               << " MB needed";
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
#ifdef DEEP_LMDB_LOGGING
    logger->logWrite("Requesting to increase map size by: %zu\n", increase_size);
    logger->logWrite("Current map size                  : %zu\n", mei.me_mapsize);
    logger->logWrite("New size                          : %zu\n", new_mapsize);
    logger->logWrite("System page size                  : %u\n", mst.ms_psize);
#endif

    LMDBTransaction::prevent_new_txns();
    BOOST_SCOPE_EXIT(void) { LMDBTransaction::allow_new_txns(); }
    BOOST_SCOPE_EXIT_END

    if (activeBatch) {
        throw std::runtime_error(
            "attempting resize with write transaction in progress, this should not happen!");
    }

    LMDBTransaction::wait_no_active_txns();

    int result = mdb_env_set_mapsize(dbEnv.get(), new_mapsize);
    if (result)
        throw std::runtime_error("Failed to set new mapsize: " + std::to_string(result));

    std::stringstream ss;
    ss << "LMDB Mapsize increased."
       << "  Old: " << mei.me_mapsize / (1024 * 1024) << " MiB"
       << ", New: " << new_mapsize / (1024 * 1024) << " MiB";
    logger->logWrite(ss.str());
}

LMDB::LMDB(const boost::filesystem::path* const dbdir, ILog* logger, bool startNewDatabase)
    : dbdir(dbdir), logger(logger)
{
    assert(dbdir);

    openDB(startNewDatabase);
}

boost::optional<std::string> LMDB::read(IDB::Index dbindex, const std::string& key, std::size_t offset,
                                        const boost::optional<std::size_t>& size) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    // if there's no active transaction, we start one for this read
    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            logger->logWrite("Failed to begin transaction at read with error code " +
                             std::to_string(res) +
                             "; and error code: " + std::string(mdb_strerror(res)));
        }
    }
    // only one of them should be active
    assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

    BOOST_SCOPE_EXIT(&localTxn)
    {
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
    }
    BOOST_SCOPE_EXIT_END

    MDB_val kS = {key.size(), (void*)(key.c_str())};
    MDB_val vS = {0, nullptr};
    if (auto ret = mdb_get((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
        std::string dbgKey = KeyAsString(key, key);
        if (ret == MDB_NOTFOUND) {
            logger->logWrite("Failed to read lmdb key " + dbgKey + " as it doesn't exist");
        } else {
            logger->logWrite("Failed to read lmdb key " + dbgKey + " with an unknown error of code " +
                             std::to_string(ret) + "; and error: " + std::string(mdb_strerror(ret)));
        }
        return boost::none;
    }
    assert(vS.mv_data != nullptr);
    // offset is never larger than the size
    const std::size_t of    = vS.mv_size >= offset ? offset : vS.mv_size;
    std::size_t       pSize = size.value_or(vS.mv_size);
    // given size is never larger than the size
    pSize = pSize > vS.mv_size ? vS.mv_size : pSize;
    // the remaining size after the offset can't be larger the remaining string after the offset
    const std::size_t fSize = pSize > vS.mv_size - of ? vS.mv_size - of : pSize;
    std::string       result(static_cast<const char*>(vS.mv_data) + of, fSize);
    return boost::make_optional(std::move(result));
}

boost::optional<std::vector<std::string>> LMDB::readMultiple(IDB::Index         dbindex,
                                                             const std::string& key) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            logger->logWrite("readMultiple: Failed to begin transaction at read with error code " +
                             std::to_string(res) +
                             "; and error "
                             "code: " +
                             std::string(mdb_strerror(res)));
        }
    }
    // only one of them should be active
    assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

    BOOST_SCOPE_EXIT(&localTxn)
    {
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
    }
    BOOST_SCOPE_EXIT_END

    MDB_val     kS           = {key.size(), (void*)(key.c_str())};
    MDB_val     vS           = {0, nullptr};
    MDB_cursor* cursorRawPtr = nullptr;
    if (auto rc = mdb_cursor_open((!activeBatch ? localTxn : *activeBatch), *dbPtr, &cursorRawPtr)) {
        logger->logWrite("readMultiple: Failed to open lmdb cursor with error code " +
                         std::to_string(rc) + "; and error: " + std::string(mdb_strerror(rc)));
        return boost::none;
    }

    std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
        if (p)
            mdb_cursor_close(p);
    });

    int itemRes = 1;

    // set the pointer to the first value
    itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_SET_RANGE);
    if (itemRes) {
        if (itemRes != 0 && itemRes != MDB_NOTFOUND) {
            const std::string dbgKey = KeyAsString(key, key);
            logger->logWrite("readMultiple: Cursor with key " + dbgKey +
                             " does not exist; with an error of code " + std::to_string(itemRes) +
                             "; and error: " + std::string(mdb_strerror(itemRes)));
            return boost::none;
        }
    }
    std::vector<std::string> result;
    do {
        // if the first item is empty, break immediately
        if (itemRes) {
            break;
        }

        assert(vS.mv_data != nullptr);
        const std::string keyFound(static_cast<const char*>(kS.mv_data), kS.mv_size);
        if (keyFound != key) {
            break;
        }
        std::string value(static_cast<const char*>(vS.mv_data), vS.mv_size);
        result.insert(result.end(), std::move(value));

        itemRes = mdb_cursor_get(cursorRawPtr, &kS, &vS, MDB_NEXT);
    } while (itemRes == 0);

    cursorPtr.reset();
    return boost::make_optional(std::move(result));
}

boost::optional<std::map<std::string, std::vector<std::string>>> LMDB::readAll(IDB::Index dbindex) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            logger->logWrite("LMDB::readAll: Failed to begin transaction at read with error code " +
                             std::to_string(res) +
                             "; and error code: " + std::string(mdb_strerror(res)));
        }
    }
    // only one of them should be active
    assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

    BOOST_SCOPE_EXIT(&localTxn)
    {
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
    }
    BOOST_SCOPE_EXIT_END

    MDB_val     kS           = {0, nullptr};
    MDB_val     vS           = {0, nullptr};
    MDB_cursor* cursorRawPtr = nullptr;
    if (auto rc = mdb_cursor_open((!activeBatch ? localTxn : *activeBatch), *dbPtr, &cursorRawPtr)) {
        logger->logWrite("LMDB::readAll: Failed to open lmdb cursor with error code " +
                         std::to_string(rc) + "; and error: " + std::string(mdb_strerror(rc)));
        return boost::none;
    }

    std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
        if (p)
            mdb_cursor_close(p);
    });

    int itemRes = 1;

    // read all items in that database
    itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_FIRST);
    if (itemRes) {
        if (itemRes != 0 && itemRes != MDB_NOTFOUND) {
            logger->logWrite(
                "LMDB::readAll: Cursor does not exist while reading all entries; with an error of "
                "code " +
                std::to_string(itemRes) + "; and error: " + std::string(mdb_strerror(itemRes)));
            return boost::none;
        }
    }
    std::map<std::string, std::vector<std::string>> result;
    do {
        // if the first item is empty, break immediately
        if (itemRes) {
            break;
        }

        assert(vS.mv_data != nullptr);

        std::string keyFound(static_cast<const char*>(kS.mv_data), kS.mv_size);
        std::string value(static_cast<const char*>(vS.mv_data), vS.mv_size);
        result[keyFound].push_back(value);

        itemRes = mdb_cursor_get(cursorRawPtr, &kS, &vS, MDB_NEXT);
    } while (itemRes == 0);

    cursorPtr.reset();
    return result;
}

bool LMDB::write(IDB::Index dbindex, const std::string& key, const std::string& value)
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    // you can't resize the db when a tx is active
    if (!activeBatch && need_resize(logger)) {
        logger->logWrite("LMDB memory map needs to be resized, doing that now.");
        doResize();
    }

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, 0, localTxn)) {
            logger->logWrite("Failed to begin transaction at read with error code " +
                             std::to_string(res) + "; and error: " + std::string(mdb_strerror(res)));
        }
    }

    // only one of them should be active
    assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

    BOOST_SCOPE_EXIT(&localTxn)
    {
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
    }
    BOOST_SCOPE_EXIT_END

    MDB_val kS = {key.size(), (void*)(key.c_str())};
    MDB_val vS = {value.size(), (void*)(value.c_str())};

    if (auto ret = mdb_put((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS, 0)) {
        const std::string dbgKey = KeyAsString(key, key);
        if (ret == MDB_MAP_FULL) {
            if (need_resize(logger)) {
                logger->logWrite(
                    "Failed to write and LMDB memory map was found to need to be resized, doing "
                    "that now.");
                doResize();
            }
            logger->logWrite("Failed to write key " + dbgKey + " with lmdb, MDB_MAP_FULL");
        } else {
            logger->logWrite("Failed to write key " + dbgKey + " with lmdb; Code " +
                             std::to_string(ret) + "; Error: " + std::string(mdb_strerror(ret)));
        }
        return false;
    }
    localTxn.commitIfValid("Tx while writing");
    return true;
}

bool LMDB::erase(IDB::Index dbindex, const std::string& key)
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);
    if (!dbPtr)
        return false;

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, 0, localTxn)) {
            logger->logWrite("Failed to begin transaction at read with error code " +
                             std::to_string(res) + "; and error: " + std::string(mdb_strerror(res)));
        }
    }

    // only one of them should be active
    assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

    BOOST_SCOPE_EXIT(&localTxn)
    {
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
    }
    BOOST_SCOPE_EXIT_END

    MDB_val kS = {key.size(), (void*)(key.c_str())};
    MDB_val vS{0, nullptr};

    if (auto ret = mdb_del((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
        const std::string dbgKey = KeyAsString(key, key);
        logger->logWrite("Failed to delete entry with key " + dbgKey + " with lmdb; Code " +
                         std::to_string(ret) + "; Error message: " + std::string(mdb_strerror(ret)));
        return false;
    }

    localTxn.commitIfValid("Tx while erasing");
    return true;
}

bool LMDB::eraseAll(IDB::Index dbindex, const std::string& key)
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);
    if (!dbPtr)
        return false;

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, 0, localTxn)) {
            logger->logWrite("Failed to begin transaction at read with error code " +
                             std::to_string(res) + "; and error: " + std::string(mdb_strerror(res)));
        }
    }

    // only one of them should be active
    assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

    BOOST_SCOPE_EXIT(&localTxn)
    {
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
    }
    BOOST_SCOPE_EXIT_END

    MDB_val kS = {key.size(), (void*)(key.c_str())};
    MDB_val vS{0, nullptr};

    MDB_cursor* cursorRawPtr = nullptr;
    if (auto rc = mdb_cursor_open((!activeBatch ? localTxn : *activeBatch), *dbPtr, &cursorRawPtr)) {
        logger->logWrite("EraseDup: Failed to open lmdb cursor with error code " + std::to_string(rc) +
                         "; and error: " + std::string(mdb_strerror(rc)));
        return false;
    }

    std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
        if (p)
            mdb_cursor_close(p);
    });

    int itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_SET);
    if (itemRes) {
        std::string dbgKey = KeyAsString(key, key);
        if (itemRes != 0) {
            logger->logWrite("Failed to erase lmdb key " + dbgKey + " with an error of code " +
                             std::to_string(itemRes) +
                             "; and error: " + std::string(mdb_strerror(itemRes)));
        }
        return false;
    }

    if (auto ret = mdb_cursor_del(cursorPtr.get(), MDB_NODUPDATA)) {
        std::string dbgKey = KeyAsString(key, key);
        logger->logWrite("Failed to delete entry with key " + dbgKey + " with lmdb; Code " +
                         std::to_string(ret) + "; Error message: " + std::string(mdb_strerror(ret)));
        return false;
    }

    cursorPtr.reset();
    localTxn.commitIfValid("Tx while erasing");
    return true;
}

bool LMDB::exists(IDB::Index dbindex, const std::string& key) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);
    if (!dbPtr)
        return false;

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            logger->logWrite("Failed to begin transaction at read with error code " +
                             std::to_string(res) + "; and error: " + std::string(mdb_strerror(res)));
        }
    }

    // only one of them should be active
    assert(localTxn.rawPtr() == nullptr || activeBatch == nullptr);

    BOOST_SCOPE_EXIT(&localTxn)
    {
        if (localTxn.rawPtr()) {
            localTxn.abort();
        }
    }
    BOOST_SCOPE_EXIT_END

    MDB_val kS = {key.size(), (void*)(key.c_str())};
    MDB_val vS{0, nullptr};

    if (auto ret = mdb_get((!activeBatch ? localTxn : *activeBatch), *dbPtr, &kS, &vS)) {
        std::string dbgKey = KeyAsString(key, key);
        if (ret == MDB_NOTFOUND) {
            return false;
        } else {
            logger->logWrite("Failed to check whether key " + dbgKey +
                             " exists with an unknown error of code " + std::to_string(ret) +
                             "; and error: " + std::string(mdb_strerror(ret)));
        }
        return false;
    } else {
        return true;
    }
}

bool LMDB::beginDBTransaction(std::size_t expectedDataSize)
{
    assert(activeBatch == nullptr);
    if (need_resize(logger, expectedDataSize)) {
        logger->logWrite("LMDB memory map needs to be resized, doing that now.");
        doResize(expectedDataSize);
    }
    activeBatch = std::unique_ptr<LMDBTransaction>(new LMDBTransaction);
    if (auto res = lmdb_txn_begin(logger, dbEnv.get(), nullptr, 0, *activeBatch)) {
        logger->logWrite("Failed to begin transaction at read with error code " + std::to_string(res) +
                         "; with error: " + std::string(mdb_strerror(res)));
        activeBatch.reset();
    }
    return true;
}

bool LMDB::commitDBTransaction()
{
    assert(activeBatch);
    if (activeBatch) {
        activeBatch->commit();
        activeBatch.reset();
    }
    return true;
}

bool LMDB::abortDBTransaction()
{
    assert(activeBatch);
    if (activeBatch) {
        activeBatch->abort();
        activeBatch.reset();
    }
    return true;
}

bool LMDB::openDB(bool clearDataBeforeOpen)
{
    if (glob_lmdb_db_pointers && !clearDataBeforeOpen) {
        loadDbPointers();
        return true;
    }

    openDatabase(*dbdir, clearDataBeforeOpen); // Init database
    loadDbPointers();
    return true;
}

boost::optional<boost::filesystem::path> LMDB::getDataDir() const
{
    return dbdir ? boost::make_optional(*dbdir) : boost::none;
}

void LMDB::close()
{
    if (activeBatch) {
        activeBatch->abort();
        activeBatch.reset();
    }
    resetDbPointers();
    resetGlobalDbPointers();
}

MDB_dbi* LMDB::getDbByIndex(const IDB::Index index) const
{
    // clang-format off
    switch (index) {
        case IDB::Index::DB_MAIN_INDEX:           return dbPointers->db_main.get();
        case IDB::Index::DB_BLOCKINDEX_INDEX:     return dbPointers->db_blockIndex.get();
        case IDB::Index::DB_BLOCKS_INDEX:         return dbPointers->db_blocks.get();
        case IDB::Index::DB_TX_INDEX:             return dbPointers->db_tx.get();
        case IDB::Index::DB_NTP1TX_INDEX:         return dbPointers->db_ntp1Tx.get();
        case IDB::Index::DB_NTP1TOKENNAMES_INDEX: return dbPointers->db_ntp1tokenNames.get();
        case IDB::Index::DB_ADDRSVSPUBKEYS_INDEX: return dbPointers->db_addrsVsPubKeys.get();
    }
    // clang-format on
    throw std::runtime_error("Invalid db index provided in getDbByIndex");
}

void LMDB::clearDBData()
{
    // close the database before deleting
    this->close();

    boost::filesystem::remove_all(*dbdir); // remove directory
}

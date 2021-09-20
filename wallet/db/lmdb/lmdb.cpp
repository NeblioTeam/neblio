#include "lmdb.h"

#include "logging/logger.h"
#include "stringmanip.h"
#include "ui_interface.h"
#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>

std::unique_ptr<MDB_env, void (*)(MDB_env*)> dbEnv(nullptr, [](MDB_env*) {});

std::unique_ptr<__lmdb_db_pointers> glob_lmdb_db_pointers;

const char* LMDB_MAINDB           = "MainDb";
const char* LMDB_BLOCKINDEXDB     = "BlockIndexDb";
const char* LMDB_BLOCKSDB         = "BlocksDb";
const char* LMDB_TXDB             = "TxDb";
const char* LMDB_NTP1TXDB         = "Ntp1txDb";
const char* LMDB_NTP1TOKENNAMESDB = "Ntp1NamesDb";
const char* LMDB_ADDRSVSPUBKEYSDB = "AddrsVsPubKeysDb";
const char* LMDB_BLOCKMETADATADB  = "BlockMetadataDb";
const char* LMDB_BLOCKHEIGHTSDB   = "BlockHeightsDB";
const char* LMDB_STAKESDB         = "StakesDB";

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

void lmdb_db_open(MDB_txn* txn, const char* name, int flags, MDB_dbi& dbi,
                  const std::string& error_string)
{
    if (int res = mdb_dbi_open(txn, name, flags, &dbi)) {
        NLog.write(b_sev::critical, "Error opening lmdb database. Error code: {}; and error: {}", res,
                   mdb_strerror(res));
        throw std::runtime_error(error_string + ": " + std::to_string(res));
    }
}

void lmdb_resized(MDB_env* env)
{
    NLog.write(b_sev::info, std::string(__func__));
    LMDBTransaction::prevent_new_txns();
    BOOST_SCOPE_EXIT(void) { LMDBTransaction::allow_new_txns(); }
    BOOST_SCOPE_EXIT_END

    NLog.write(b_sev::info, "LMDB map resize detected.");

    MDB_envinfo mei;

    mdb_env_info(env, &mei);
    const uint64_t old = mei.me_mapsize;

    LMDBTransaction::wait_no_active_txns();

    const int result = mdb_env_set_mapsize(env, 0);
    if (result)
        NLog.write(b_sev::err, "Failed to set new mapsize: {}", result);

    mdb_env_info(env, &mei);
    const uint64_t new_mapsize = mei.me_mapsize;

    std::stringstream ss;
    ss << "LMDB Mapsize increased."
       << "  Old: " << old / (1024 * 1024) << " MiB"
       << ", New: " << new_mapsize / (1024 * 1024) << " MiB";
    NLog.write(b_sev::info, ss.str());
}

int lmdb_txn_begin(MDB_env* env, MDB_txn* parent, unsigned int flags, LMDBTransaction& txn)
{
    int res = mdb_txn_begin(env, parent, flags, txn);
    if (res == MDB_MAP_RESIZED) {
        {
            NLog.write(b_sev::warn, "Warning: MDB_MAP_RESIZED detected.");
            LMDBTransaction::increment_txns(txn.isChecked() ? -1 : 0);
            BOOST_SCOPE_EXIT(&txn) { LMDBTransaction::increment_txns(txn.isChecked() ? 1 : 0); }
            BOOST_SCOPE_EXIT_END
            lmdb_resized(env);
        }
        res = mdb_txn_begin(env, parent, flags, txn);
    }
    return res;
}

// threshold_size is used for batch transactions
static bool need_resize(uint64_t threshold_size = 0)
{
#ifdef DEEP_LMDB_LOGGING
    NLog.write("LMDB: " + std::string(__func__));
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
    NLog.write("Checking if resize is needed.");
    NLog.write("DB map size:     {}", mei.me_mapsize);
    NLog.write("Space used:      {}", size_used);
    NLog.write("Space remaining: {}", mei.me_mapsize - size_used);
    NLog.write("Size threshold:  {}", threshold_size);
#endif
    const float resize_percent = DB_RESIZE_PERCENT;
#ifdef DEEP_LMDB_LOGGING
    NLog.write("Percent used: %.04f  Percent threshold: %.04f", ((double)size_used / mei.me_mapsize),
               resize_percent);
#endif

    if (threshold_size > 0) {
        if (mei.me_mapsize - size_used < threshold_size) {
            NLog.write(b_sev::warn, "Threshold met (size-based)");
            return true;
        } else
            return false;
    }

    if ((double)size_used / mei.me_mapsize > resize_percent) {
        NLog.write(b_sev::warn, "Mapsize threshold met (percent-based)");
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
        LMDB::clearDBData();
    }

    NLog.write(b_sev::info, "Opening the blockchain database...");
    uiInterface.InitMessage("Opening the blockchain database...", 0);

    // open the database in the traditional way (whether quicksync succeeded or not)
    boost::filesystem::create_directories(directory);
    NLog.write(b_sev::info, "Opening lmdb in " + directory.string());
    MDB_env* envPtr = nullptr;
    if (const int rc = mdb_env_create(&envPtr)) {
        NLog.write(b_sev::critical, "Error creating lmdb environment: " + std::to_string(rc) +
                                        "; message: " + std::string(mdb_strerror(rc)));
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
        NLog.write(b_sev::info, "LMDB memory map size: {}", currMapSize);
    }

    if (need_resize()) {
        NLog.write(b_sev::info, "LMDB memory map needs to be resized, doing that now.");
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
    glob_lmdb_db_pointers->db_blockMetadata  = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_blockHeights   = DbSmartPtrType(new MDB_dbi, dbDeleter);
    glob_lmdb_db_pointers->db_stakes         = DbSmartPtrType(new MDB_dbi, dbDeleter);

    // MDB_CREATE: Create the named database if it doesn't exist.
    lmdb_db_open(txn, LMDB_MAINDB, MDB_CREATE, *glob_lmdb_db_pointers->db_main,
                 "Failed to open db handle for db_main");
    lmdb_db_open(txn, LMDB_BLOCKINDEXDB, MDB_CREATE, *glob_lmdb_db_pointers->db_blockIndex,
                 "Failed to open db handle for db_blockIndex");
    lmdb_db_open(txn, LMDB_BLOCKSDB, MDB_CREATE, *glob_lmdb_db_pointers->db_blocks,
                 "Failed to open db handle for db_blocks");
    lmdb_db_open(txn, LMDB_TXDB, MDB_CREATE, *glob_lmdb_db_pointers->db_tx,
                 "Failed to open db handle for db_tx");
    lmdb_db_open(txn, LMDB_NTP1TXDB, MDB_CREATE, *glob_lmdb_db_pointers->db_ntp1Tx,
                 "Failed to open db handle for db_ntp1Tx");
    lmdb_db_open(txn, LMDB_NTP1TOKENNAMESDB, MDB_CREATE | MDB_DUPSORT,
                 *glob_lmdb_db_pointers->db_ntp1tokenNames,
                 "Failed to open db handle for db_ntp1tokenNames");
    lmdb_db_open(txn, LMDB_ADDRSVSPUBKEYSDB, MDB_CREATE, *glob_lmdb_db_pointers->db_addrsVsPubKeys,
                 "Failed to open db handle for db_addrsVsPubKeys");
    lmdb_db_open(txn, LMDB_BLOCKMETADATADB, MDB_CREATE, *glob_lmdb_db_pointers->db_blockMetadata,
                 "Failed to open db handle for db_blockMetadata");
    lmdb_db_open(txn, LMDB_BLOCKHEIGHTSDB, MDB_CREATE, *glob_lmdb_db_pointers->db_blockHeights,
                 "Failed to open db handle for db_blockHeights");
    lmdb_db_open(txn, LMDB_STAKESDB, MDB_CREATE, *glob_lmdb_db_pointers->db_stakes,
                 "Failed to open db handle for db_stakes");

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
    if (!glob_lmdb_db_pointers->db_blockMetadata) {
        throw std::runtime_error("LMDB nullptr after opening the db_blockMetadata database.");
    }
    if (!glob_lmdb_db_pointers->db_blockHeights) {
        throw std::runtime_error("LMDB nullptr after opening the db_blockHeights database.");
    }
    if (!glob_lmdb_db_pointers->db_stakes) {
        throw std::runtime_error("LMDB nullptr after opening the db_stakes database.");
    }

    boost::atomic_thread_fence(boost::memory_order_seq_cst);

    NLog.write(b_sev::info, "Done opening the database");
    uiInterface.InitMessage("Done opening the database", 1);
}

void LMDB::doResize(uint64_t increase_size)
{
    NLog.write(b_sev::info, std::string(FUNCTIONSIG));
    NLog.write(b_sev::info, "Requesting to increase LMDB size by {}", increase_size);

    if (increase_size != 0 && increase_size < MIN_MAP_SIZE_INCREASE) {
        // protect from having very small incremental changes in the DB size, which is not efficient
        increase_size = MIN_MAP_SIZE_INCREASE;
    }

    const uintmax_t add_size = UINTMAX_C(1) << 30;

    // check disk capacity
    try {
        boost::filesystem::space_info si = boost::filesystem::space(*dbdir_);
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
    NLog.write("Requesting to increase map size by: %zu", increase_size);
    NLog.write("Current map size                  : %zu", mei.me_mapsize);
    NLog.write("New size                          : %zu", new_mapsize);
    NLog.write("System page size                  : %u", mst.ms_psize);
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
    NLog.write(b_sev::info, ss.str());
}

LMDB::LMDB(const boost::filesystem::path* const dbdir, bool startNewDatabase) : dbdir_(dbdir)
{
    assert(dbdir);

    LMDB::openDB(startNewDatabase);
}

Result<boost::optional<std::string>, int> LMDB::read(IDB::Index dbindex, const std::string& key,
                                                     std::size_t                         offset,
                                                     const boost::optional<std::size_t>& size) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    // if there's no active transaction, we start one for this read
    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error code: {}", res,
                       mdb_strerror(res));
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
            NLog.write(b_sev::debug, "Failed to read lmdb key {} and dbid {} as it doesn't exist",
                       dbgKey, dbindex);
            return Ok(boost::optional<std::string>());
        } else {
            NLog.write(b_sev::err,
                       "Failed to read lmdb key {} with dbid {} with an unknown error of code {}; and "
                       "error: {}",
                       dbgKey, dbindex, ret, mdb_strerror(ret));
        }
        return Err(ret);
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
    return Ok(boost::make_optional(std::move(result)));
}

Result<std::vector<std::string>, int> LMDB::readMultiple(IDB::Index         dbindex,
                                                         const std::string& key) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error: {}", res,
                       mdb_strerror(res));
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
        std::string dbgKey = KeyAsString(key, key);
        NLog.write(
            b_sev::err,
            "Failed to open lmdb cursor with key {} and dbid {} with error code {}; and error: {}",
            dbgKey, dbindex, rc, mdb_strerror(rc));
        return Err(rc);
    }

    std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
        if (p)
            mdb_cursor_close(p);
    });

    int itemRes = 1;

    // set the pointer to the first value
    itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_SET_RANGE);
    if (itemRes != 0 && itemRes != MDB_NOTFOUND) {
        const std::string dbgKey = KeyAsString(key, key);
        NLog.write(
            b_sev::err,
            "Cursor with key {} and dbid {} does not exist; with an error of code {}; and error: {}",
            dbgKey, dbindex, itemRes, mdb_strerror(itemRes));
        return Err(itemRes);
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
    return Ok(std::move(result));
}

Result<std::map<std::string, std::vector<std::string>>, int> LMDB::readAll(IDB::Index dbindex) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error code: {}", res,
                       mdb_strerror(res));
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
        NLog.write(b_sev::err,
                   "Failed to open lmdb cursor with dbid {} with error code {}; and error: {}", dbindex,
                   rc, mdb_strerror(rc));
        return Err(rc);
    }

    std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
        if (p)
            mdb_cursor_close(p);
    });

    int itemRes = 1;

    // read all items in that database
    itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_FIRST);
    if (itemRes != 0 && itemRes != MDB_NOTFOUND) {
        NLog.write(b_sev::err,
                   "Cursor does not exist while reading all entries; with an error of "
                   "code {}; and error: {}",
                   itemRes, mdb_strerror(itemRes));
        return Err(itemRes);
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
    return Ok(std::move(result));
}

Result<std::map<std::string, std::string>, int> LMDB::readAllUnique(IDB::Index dbindex) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error code: {}", res,
                       mdb_strerror(res));
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
        NLog.write(b_sev::err,
                   "Failed to open lmdb cursor with dbid {} with error code {}; and error: {}", dbindex,
                   rc, mdb_strerror(rc));
        return Err(rc);
    }

    std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
        if (p)
            mdb_cursor_close(p);
    });

    int itemRes = 1;

    // read all items in that database
    itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_FIRST);
    if (itemRes != 0 && itemRes != MDB_NOTFOUND) {
        NLog.write(b_sev::err,
                   "Cursor does not exist while reading all entries of dbid {}; with an error of "
                   "code {}; and error: {}",
                   dbindex, itemRes, mdb_strerror(itemRes));
        return Err(itemRes);
    }
    std::map<std::string, std::string> result;
    do {
        // if the first item is empty, break immediately
        if (itemRes) {
            break;
        }

        assert(vS.mv_data != nullptr);

        std::string keyFound(static_cast<const char*>(kS.mv_data), kS.mv_size);
        std::string value(static_cast<const char*>(vS.mv_data), vS.mv_size);
        result[keyFound] = value;

        itemRes = mdb_cursor_get(cursorRawPtr, &kS, &vS, MDB_NEXT);
    } while (itemRes == 0);

    cursorPtr.reset();
    return Ok(std::move(result));
}

Result<void, int> LMDB::write(IDB::Index dbindex, const std::string& key, const std::string& value)
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    // you can't resize the db when a tx is active
    if (!activeBatch && need_resize()) {
        NLog.write(b_sev::info, "LMDB memory map needs to be resized, doing that now.");
        doResize();
    }

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error: {}", res,
                       mdb_strerror(res));
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
            if (need_resize()) {
                NLog.write(b_sev::critical,
                           "Failed to write and LMDB memory map was found to need to be resized, doing "
                           "that now.");
            }
            NLog.write(b_sev::err,
                       "Failed to write key {} with dbid {} and data of size {} in lmdb, MDB_MAP_FULL",
                       dbgKey, dbindex, vS.mv_size);
        } else {
            NLog.write(
                b_sev::err,
                "Failed to write key {} with dbid {} and data of size {} in lmdb; Code {}; Error: {}",
                dbgKey, dbindex, vS.mv_size, ret, mdb_strerror(ret));
        }
        return Err(ret);
    }
    localTxn.commitIfValid("Tx while writing");
    return Ok();
}

Result<void, int> LMDB::erase(IDB::Index dbindex, const std::string& key)
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error: {}", res,
                       mdb_strerror(res));
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
        if (ret != MDB_NOTFOUND) {
            const std::string dbgKey = KeyAsString(key, key);
            NLog.write(
                b_sev::err,
                "Failed to delete entry with key {} with dbid {} in lmdb; Code {}; Error message: {}",
                dbgKey, dbindex, ret, mdb_strerror(ret));
            return Err(ret);
        }
    }

    localTxn.commitIfValid("Tx while erasing");
    return Ok();
}

Result<void, int> LMDB::eraseAll(IDB::Index dbindex, const std::string& key)
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error: {}", res,
                       mdb_strerror(res));
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
        std::string dbgKey = KeyAsString(key, key);
        NLog.write(b_sev::err,
                   "Failed to open lmdb cursor of dbid {} and key {} with error code {}; and error: {}",
                   dbindex, dbgKey, rc, mdb_strerror(rc));
        return Err(rc);
    }

    std::unique_ptr<MDB_cursor, void (*)(MDB_cursor*)> cursorPtr(cursorRawPtr, [](MDB_cursor* p) {
        if (p)
            mdb_cursor_close(p);
    });

    int itemRes = mdb_cursor_get(cursorPtr.get(), &kS, &vS, MDB_SET);
    if (itemRes) {
        std::string dbgKey = KeyAsString(key, key);
        if (itemRes != 0) {
            NLog.write(b_sev::err, "Failed to erase lmdb key {} with an error of code {}; and error: {}",
                       dbgKey, itemRes, mdb_strerror(itemRes));
        }
        return Err(itemRes);
    }

    if (auto ret = mdb_cursor_del(cursorPtr.get(), MDB_NODUPDATA)) {
        if (ret != MDB_NOTFOUND) {
            std::string dbgKey = KeyAsString(key, key);
            NLog.write(
                b_sev::err,
                "Failed to delete entry with key {} with dbid {} in lmdb; Code {}; Error message: {}",
                dbgKey, dbindex, ret, mdb_strerror(ret));
            return Err(ret);
        }
    }

    cursorPtr.reset();
    localTxn.commitIfValid("Tx while erasing");
    return Ok();
}

Result<bool, int> LMDB::exists(IDB::Index dbindex, const std::string& key) const
{
    const MDB_dbi* dbPtr = getDbByIndex(dbindex);

    LMDBTransaction localTxn(false);
    if (!activeBatch) {
        localTxn = LMDBTransaction();
        if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, MDB_RDONLY, localTxn)) {
            NLog.write(b_sev::err,
                       "Failed to begin transaction at read with error code {}; and error: {}", res,
                       mdb_strerror(res));
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
            return Ok(false);
        } else {
            NLog.write(b_sev::info,
                       "Failed to check whether key {} with dbid {} exists with an unknown error of "
                       "code {}; and error: {}",
                       dbgKey, dbindex, ret, mdb_strerror(ret));
        }
        return Err(ret);
    } else {
        return Ok(true);
    }
}

Result<void, int> LMDB::beginDBTransaction(std::size_t expectedDataSize)
{
    assert(activeBatch == nullptr);
    if (need_resize(expectedDataSize)) {
        NLog.write(b_sev::info, "LMDB memory map needs to be resized, doing that now.");
        doResize(expectedDataSize);
    }
    activeBatch = std::unique_ptr<LMDBTransaction>(new LMDBTransaction);
    if (auto res = lmdb_txn_begin(dbEnv.get(), nullptr, 0, *activeBatch)) {
        NLog.write(b_sev::err, "Failed to begin transaction with error code {}; with error: {}", res,
                   mdb_strerror(res));
        activeBatch.reset();
        return Err(res);
    }
    return Ok();
}

Result<void, int> LMDB::commitDBTransaction()
{
    assert(activeBatch);
    if (activeBatch) {
        int commitResult = activeBatch->commit();
        activeBatch.reset();
        if (commitResult != MDB_SUCCESS) {
            return Err(commitResult);
        }
    }
    return Ok();
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

    openDatabase(*dbdir_, clearDataBeforeOpen); // Init database
    loadDbPointers();
    return true;
}

boost::optional<boost::filesystem::path> LMDB::getDataDir() const
{
    return dbdir_ ? boost::make_optional(*dbdir_) : boost::none;
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
        case IDB::Index::DB_BLOCKMETADATA_INDEX:  return dbPointers->db_blockMetadata.get();
        case IDB::Index::DB_BLOCKHEIGHTS_INDEX:   return dbPointers->db_blockHeights.get();
        case IDB::Index::DB_STAKES_INDEX:         return dbPointers->db_stakes.get();
        case IDB::Index::Index_Last:              throw std::invalid_argument("Invalid DB Index");
    }
    // clang-format on
    throw std::runtime_error("Invalid db index provided in getDbByIndex");
}

void LMDB::clearDBData()
{
    // close the database before deleting
    LMDB::close();

    boost::filesystem::remove_all(*dbdir_); // remove directory

    LMDB::openDB(false);
}

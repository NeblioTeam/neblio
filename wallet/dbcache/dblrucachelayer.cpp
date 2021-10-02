#include "dblrucachelayer.h"

#include "db/lmdb/lmdb.h"
#include "dbcache/dbreadcachelayer.h"
#include "dblrucachestorage.h"
#include "logging/logger.h"
#include "util.h"
#include <boost/atomic.hpp>
#include <boost/scope_exit.hpp>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

using namespace DBOperation;

static DBLRUCacheStorage g_db_lru_cache;

static boost::atomic_int64_t  cachedTxCount{0};
static boost::atomic_uint64_t lruFlushCount{0};

static const int DBCACHE_FLUSH_SIZE = 1 << 25;

template <typename BaseDB>
DBLRUCacheLayer<BaseDB>::DBLRUCacheLayer(const boost::filesystem::path* const dbdir,
                                         bool startNewDatabase, int64_t flushOnSize)
    : flushOnSizeReached(flushOnSize), dbdir_(dbdir)
{
    DBLRUCacheLayer<BaseDB>::openDB(startNewDatabase);
}

static boost::atomic_uint_fast32_t LRUCacheRWCount{0};
static boost::atomic_uint_fast32_t LRUCacheFlushCount{0};
static boost::atomic_flag          LRUCacheRaceGuard = BOOST_ATOMIC_FLAG_INIT;

// the only guarding we do is protect that a flush and a read/write happen together, to ensure
// consistency
#define GUARD_RW()                                                                                      \
    boost::atomic_thread_fence(boost::memory_order_acquire);                                            \
    {                                                                                                   \
        while (LRUCacheRaceGuard.test_and_set()) {                                                      \
        }                                                                                               \
        BOOST_SCOPE_EXIT(void) { LRUCacheRaceGuard.clear(); }                                           \
        BOOST_SCOPE_EXIT_END                                                                            \
                                                                                                        \
        while (LRUCacheFlushCount > 0) {                                                                \
        }                                                                                               \
                                                                                                        \
        LRUCacheRWCount++;                                                                              \
    }                                                                                                   \
    boost::atomic_thread_fence(boost::memory_order_release);                                            \
    BOOST_SCOPE_EXIT(void) { LRUCacheRWCount--; }                                                       \
    BOOST_SCOPE_EXIT_END                                                                                \
    do {                                                                                                \
    } while (0)

#define GUARD_FLUSH()                                                                                   \
    boost::atomic_thread_fence(boost::memory_order_acquire);                                            \
    {                                                                                                   \
        while (LRUCacheRaceGuard.test_and_set()) {                                                      \
        }                                                                                               \
        BOOST_SCOPE_EXIT(void) { LRUCacheRaceGuard.clear(); }                                           \
        BOOST_SCOPE_EXIT_END                                                                            \
                                                                                                        \
        if (LRUCacheFlushCount > 0) {                                                                   \
            return false;                                                                               \
        }                                                                                               \
                                                                                                        \
        LRUCacheFlushCount++;                                                                           \
        while (LRUCacheRWCount > 0) {                                                                   \
        }                                                                                               \
    }                                                                                                   \
    boost::atomic_thread_fence(boost::memory_order_release);                                            \
    BOOST_SCOPE_EXIT(void) { LRUCacheFlushCount--; }                                                    \
    BOOST_SCOPE_EXIT_END                                                                                \
    do {                                                                                                \
    } while (0)

template <typename BaseDB>
Result<boost::optional<std::string>, int>
DBLRUCacheLayer<BaseDB>::read(Index dbindex, const std::string& key, std::size_t offset,
                              const boost::optional<std::size_t>& size) const
{

    if (tx) {
        boost::optional<TransactionOperation> txVal = tx->getOp(static_cast<int>(dbindex), key);
        if (txVal) {
            switch (txVal->getOpType()) {
            case WriteOperationType::Append:
            case WriteOperationType::UniqueSet:
                if (!txVal->getValues().empty()) {
                    if (size) {
                        return Ok(
                            boost::make_optional(txVal->getValues().front().substr(offset, *size)));
                    } else {
                        return Ok(boost::make_optional(txVal->getValues().front().substr(offset)));
                    }
                }
                break;
            case WriteOperationType::Erase:
                return Ok(boost::optional<std::string>());
            }
        }
    }

    BOOST_SCOPE_EXIT_ALL(this) { flushOnPolicy(); };

    {
        GUARD_RW();

        {
            const int                                                   dbid = static_cast<int>(dbindex);
            const boost::optional<DBLRUCacheStorage::StoredEntryResult> okv =
                g_db_lru_cache.get_one(dbid, key);

            if (okv) {
                const DBLRUCacheStorage::StoredEntryResult& kv = *okv;
                switch (kv.op) {
                case DBLRUCacheStorage::StoredOperationType::Erase:
                    return Ok(boost::optional<std::string>());
                case DBLRUCacheStorage::StoredOperationType::Write:
                    // single values should NEVER be empty... so if it's empty, we refresh the cache to
                    // fix the issue
                    if (size) {
                        return Ok(boost::make_optional(kv.value.substr(offset, *size)));
                    } else {
                        return Ok(boost::make_optional(kv.value.substr(offset)));
                    }
                }
            }

            BaseDB persistedDB(dbdir_, false, DBCACHE_FLUSH_SIZE);

            Result<boost::optional<std::string>, int> rdVal =
                persistedDB.read(dbindex, key, 0, boost::none);
            if (rdVal.isOk()) {
                const boost::optional<std::string>& dVal = rdVal.UNWRAP();
                if (dVal) {
                    if (size) {
                        return Ok(boost::make_optional(dVal->substr(offset, *size)));
                    } else {
                        return Ok(boost::make_optional(dVal->substr(offset)));
                    }
                }
            }
        }
    }

    return Ok(boost::optional<std::string>());
}

template <typename BaseDB>
Result<std::vector<std::string>, int> DBLRUCacheLayer<BaseDB>::readMultiple(Index              dbindex,
                                                                            const std::string& key) const
{
    std::vector<std::string> valuesToAppend;
    if (tx) {
        boost::optional<TransactionOperation> txVal = tx->getOp(static_cast<int>(dbindex), key);
        if (txVal) {
            switch (txVal->getOpType()) {
            case WriteOperationType::Append:
            case WriteOperationType::UniqueSet:
                if (!txVal->getValues().empty()) {
                    valuesToAppend = txVal->getValues();
                }
                break;
            case WriteOperationType::Erase:
                // we return this considering it's empty
                return Ok(std::move(valuesToAppend));
            }
        }
    }

    BOOST_SCOPE_EXIT_ALL(this) { flushOnPolicy(); };

    {
        GUARD_RW();

        std::vector<std::string> cachedWrites;
        {
            const int                                               dbid = static_cast<int>(dbindex);
            const std::vector<DBLRUCacheStorage::StoredEntryResult> okv  = g_db_lru_cache.get(dbid, key);

            for (const auto& kv : okv) {
                switch (kv.op) {
                case DBLRUCacheStorage::Erase:
                    // we short-circuit and exit if all following values are erased
                    cachedWrites.clear(); // all inserted values are to be erased now
                    // then we insert whatever we got from the tx before
                    cachedWrites.insert(cachedWrites.end(),
                                        std::make_move_iterator(valuesToAppend.begin()),
                                        std::make_move_iterator(valuesToAppend.end()));
                    return Ok(std::move(cachedWrites));
                case DBLRUCacheStorage::Write:
                    cachedWrites.push_back(kv.value);
                }
            }
        }

        cachedWrites.insert(cachedWrites.end(), std::make_move_iterator(valuesToAppend.begin()),
                            std::make_move_iterator(valuesToAppend.end()));

        BaseDB persistedDB(dbdir_, false, DBCACHE_FLUSH_SIZE);

        Result<std::vector<std::string>, int> rdVal = persistedDB.readMultiple(dbindex, key);
        if (rdVal.isOk()) {
            std::vector<std::string>& dVal = rdVal.UNWRAP();
            dVal.insert(dVal.end(), std::make_move_iterator(cachedWrites.begin()),
                        std::make_move_iterator(cachedWrites.end()));
            return Ok(std::move(dVal));
        }
    }
    return Err(1);
}

static void MergeCachedWritesWithData(
    std::map<std::string, std::vector<std::string>>&                           dataMap,
    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>>&& cachedWrites,
    bool                                                                       duplicatesAllowed)
{
    for (auto&& p : cachedWrites) {
        auto&& key    = p.first;
        auto&& values = p.second;
        if (!values.empty() && values.front().op == DBLRUCacheStorage::Erase) {
            // if the last operation is "erase", no point in pushing then erasing
            dataMap.erase(key);
            continue;
        }
        for (auto&& value : values) {
            switch (value.op) {
            case DBLRUCacheStorage::Erase:
                dataMap.erase(key);
                break;
            case DBLRUCacheStorage::Write:
                if (duplicatesAllowed) {
                    dataMap[value.key].push_back(value.value);
                    break;
                } else {
                    dataMap[value.key] = std::vector<std::string>(1, value.value);
                }
                break;
            }
        }
    }
}

static void MergeTxDataWithData(std::map<std::string, std::vector<std::string>>& dataMap,
                                std::map<std::string, TransactionOperation>&&    txData)
{
    for (auto&& p : txData) {
        auto&& key  = p.first;
        auto&& txOp = p.second;

        switch (txOp.getOpType()) {
        case WriteOperationType::Append:
            dataMap[key].insert(dataMap[key].end(), std::make_move_iterator(txOp.getValues().begin()),
                                std::make_move_iterator(txOp.getValues().end()));
            break;
        case WriteOperationType::UniqueSet:
            if (!txOp.getValues().empty()) {
                dataMap.erase(key);
                dataMap.insert(std::make_pair(key, txOp.getValues()));
            }
            break;
        case WriteOperationType::Erase:
            dataMap.erase(key);
            break;
        }
    }
}

template <typename BaseDB>
Result<std::map<std::string, std::vector<std::string>>, int>
DBLRUCacheLayer<BaseDB>::readAll(Index dbindex) const
{

    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>> allCachedWrites =
        g_db_lru_cache.getAll(static_cast<int>(dbindex));

    GUARD_RW();

    BaseDB persistedDB(dbdir_, false, DBCACHE_FLUSH_SIZE);

    auto tRes = persistedDB.readAll(dbindex);
    if (tRes.isErr()) {
        return tRes;
    }

    std::map<std::string, std::vector<std::string>>& res = tRes.UNWRAP();

    // first we apply cached writes
    MergeCachedWritesWithData(res, std::move(allCachedWrites), IDB::DuplicateKeysAllowed(dbindex));

    // then above it the results from the tx, if any
    MergeTxDataWithData(res, std::move(txOps));

    return Ok(std::move(res));
}

static void MergeCachedWritesWithData(
    std::map<std::string, std::string>&                                        dataMap,
    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>>&& cachedWrites)
{
    for (auto&& p : cachedWrites) {
        auto&& key    = p.first;
        auto&& values = p.second;
        if (!values.empty() && values.front().op == DBLRUCacheStorage::Erase) {
            // if the last operation is "erase", no point in pushing then erasing
            dataMap.erase(key);
            continue;
        }
        for (auto&& value : values) {
            switch (value.op) {
            case DBLRUCacheStorage::Erase:
                dataMap.erase(key);
                break;
            case DBLRUCacheStorage::Write:
                dataMap[value.key] = value.value;
                break;
            }
        }
    }
}
static void MergeTxDataWithData(std::map<std::string, std::string>&           dataMap,
                                std::map<std::string, TransactionOperation>&& txData)
{
    for (auto&& p : txData) {
        auto&& key  = p.first;
        auto&& txOp = p.second;

        switch (txOp.getOpType()) {
        case WriteOperationType::Append:
        case WriteOperationType::UniqueSet:
            if (!txOp.getValues().empty()) {
                dataMap[key] = std::move(txOp.getValues().front());
            }
            break;
        case WriteOperationType::Erase:
            dataMap.erase(key);
            break;
        }
    }
}

template <typename BaseDB>
Result<std::map<std::string, std::string>, int>
DBLRUCacheLayer<BaseDB>::readAllUnique(Index dbindex) const
{

    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>> allCachedWrites =
        g_db_lru_cache.getAll(static_cast<int>(dbindex));

    GUARD_RW();

    BaseDB persistedDB(dbdir_, false, DBCACHE_FLUSH_SIZE);

    auto tRes = persistedDB.readAllUnique(dbindex);
    if (tRes.isErr()) {
        return tRes;
    }
    std::map<std::string, std::string>& res = tRes.UNWRAP();

    // first we apply cached writes
    MergeCachedWritesWithData(res, std::move(allCachedWrites));

    // then above it the results from the tx, if any
    MergeTxDataWithData(res, std::move(txOps));

    return Ok(std::move(res));
}

template <typename BaseDB>
Result<void, int> DBLRUCacheLayer<BaseDB>::write(Index dbindex, const std::string& key,
                                                 const std::string& value)
{

    if (tx) {
        if (IDB::DuplicateKeysAllowed(dbindex)) {
            return tx->multi_append(static_cast<int>(dbindex), key, value) ? Result<void, int>(Ok())
                                                                           : Err(1);
        } else {
            return tx->unique_set(static_cast<int>(dbindex), key, value) ? Result<void, int>(Ok())
                                                                         : Err(1);
        }
    }

    BOOST_SCOPE_EXIT_ALL(this) { flushOnPolicy(); };

    {
        GUARD_RW();

        {
            const int dbid = static_cast<int>(dbindex);
            g_db_lru_cache.add(
                TransactableDBEntry(TransactableDBEntry::SingleKeyValue(dbid, key, value)));
            cachedTxCount.fetch_add(1, boost::memory_order_relaxed);
        }
    }

    return Ok();
}

template <typename BaseDB>
Result<void, int> DBLRUCacheLayer<BaseDB>::erase(Index dbindex, const std::string& key)
{

    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    {
        GUARD_RW();

        {
            const int dbid = static_cast<int>(dbindex);
            g_db_lru_cache.add(TransactableDBEntry(TransactableDBEntry::EraseKeyValue(dbid, key)));
        }
    }

    return Ok();
}

template <typename BaseDB>
Result<void, int> DBLRUCacheLayer<BaseDB>::eraseAll(Index dbindex, const std::string& key)
{

    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    {
        GUARD_RW();

        {
            const int dbid = static_cast<int>(dbindex);
            g_db_lru_cache.add(TransactableDBEntry(TransactableDBEntry::EraseKeyValue(dbid, key)));
        }
    }
    return Ok();
}

template <typename BaseDB>
Result<bool, int> DBLRUCacheLayer<BaseDB>::exists(Index dbindex, const std::string& key) const
{

    if (tx) {
        boost::optional<TransactionOperation> txVal = tx->getOp(static_cast<int>(dbindex), key);
        if (txVal) {
            switch (txVal->getOpType()) {
            case WriteOperationType::Append:
            case WriteOperationType::UniqueSet:
                if (!txVal->getValues().empty()) {
                    return Ok(true);
                }
                break;
            case WriteOperationType::Erase:
                return Ok(false);
            }
        }
    }

    BOOST_SCOPE_EXIT_ALL(this) { flushOnPolicy(); };

    {
        GUARD_RW();

        {
            const int                                                   dbid = static_cast<int>(dbindex);
            const boost::optional<DBLRUCacheStorage::StoredEntryResult> okv =
                g_db_lru_cache.get_one(dbid, key);

            if (okv) {
                const DBLRUCacheStorage::StoredEntryResult& kv = *okv;
                switch (kv.op) {
                case DBLRUCacheStorage::StoredOperationType::Erase:
                    return Ok(false);
                case DBLRUCacheStorage::StoredOperationType::Write:
                    return Ok(true);
                }
            }

            BaseDB persistedDB(dbdir_, false, DBCACHE_FLUSH_SIZE);

            return persistedDB.exists(dbindex, key);
        }
    }
}

template <typename BaseDB>
void DBLRUCacheLayer<BaseDB>::clearDBData()
{
    clearCache();
    g_db_lru_cache.clear();
    BaseDB persistedDB(dbdir_, true, DBCACHE_FLUSH_SIZE);
}

template <typename BaseDB>
Result<void, int> DBLRUCacheLayer<BaseDB>::beginDBTransaction(std::size_t /*expectedDataSize*/)
{
    if (tx) {
        return Err(-1);
    }
    tx = std::unique_ptr<HierarchicalDB<typename decltype(tx)::element_type::MutexT>>(
        new HierarchicalDB<typename decltype(tx)::element_type::MutexT>(""));
    return Ok();
}

template <typename MutexType>
static std::array<std::map<std::string, TransactionOperation>,
                  static_cast<std::size_t>(IDB::Index::Index_Last)>
GetAllTxData(HierarchicalDB<MutexType>* tx)
{
    if (!tx) {
        return {};
    }

    std::array<std::map<std::string, TransactionOperation>,
               static_cast<std::size_t>(IDB::Index::Index_Last)>
        txData;
    for (int index = 0; index < static_cast<int>(IDB::Index::Index_Last); index++) {
        txData[index] = tx->getAllDataForDB(index);
    }
    return txData;
}

template <typename BaseDB>
Result<void, int> DBLRUCacheLayer<BaseDB>::commitDBTransaction()
{
    if (!tx) {
        return Err(-1);
    }

    std::unique_ptr<HierarchicalDB<typename decltype(tx)::element_type::MutexT>> movedTx = std::move(tx);
    tx.reset();

    std::array<std::map<std::string, TransactionOperation>,
               static_cast<std::size_t>(IDB::Index::Index_Last)>
        txData = GetAllTxData(movedTx.get());

    bool result = true;

    BOOST_SCOPE_EXIT_ALL(this) { flushOnPolicy(); };

    {
        GUARD_RW();

        auto&& dbEntry = TransactableDBEntry(std::move(movedTx));
        g_db_lru_cache.add(std::move(dbEntry));
        cachedTxCount.fetch_add(1, boost::memory_order_relaxed);
    }

    return result ? Result<void, int>(Ok()) : Err(-1);
}

template <typename BaseDB>
bool DBLRUCacheLayer<BaseDB>::abortDBTransaction()
{
    tx.reset();
    return true;
}

template <typename BaseDB>
boost::optional<boost::filesystem::path> DBLRUCacheLayer<BaseDB>::getDataDir() const
{
    return *dbdir_;
}

template <typename BaseDB>
bool DBLRUCacheLayer<BaseDB>::openDB(bool clearDataBeforeOpen)
{
    if (clearDataBeforeOpen) {
        clearCache();
        DBLRUCacheLayer<BaseDB>::clearDBData();
        cachedTxCount.store(0, boost::memory_order_seq_cst);
        lruFlushCount.store(0, boost::memory_order_seq_cst);
    }

    BaseDB persistedDB(dbdir_, clearDataBeforeOpen, DBCACHE_FLUSH_SIZE);
    boost::atomic_thread_fence(boost::memory_order_seq_cst);

    return true;
}

template <typename BaseDB>
void DBLRUCacheLayer<BaseDB>::close()
{
    tx.reset();
    flush();
    BaseDB persistedDB(dbdir_, false, DBCACHE_FLUSH_SIZE);
    persistedDB.close();
    boost::atomic_thread_fence(boost::memory_order_seq_cst);
}

template <typename BaseDB>
boost::optional<bool> DBLRUCacheLayer<BaseDB>::flushOnPolicy() const
{
    if (flushOnSizeReached > 0) {
        if (cachedTxCount.load(boost::memory_order_acquire) > flushOnSizeReached) {
            bool res = flush();
            return boost::make_optional(res);
        }
    }
    return boost::none;
}

std::uintmax_t CalculateDataSize(const std::vector<DBLRUCacheStorage::StoredEntryResult>& data)
{
    std::uintmax_t totalSize = 0;
    for (const auto& d : data) {
        totalSize += d.key.size();
        totalSize += d.value.size();
    }
    return totalSize;
}

static std::vector<DBLRUCacheStorage::StoredEntryResult>
CollectSomeDataToPersist(const uintmax_t ApproximateMaxSize)
{
    std::vector<DBLRUCacheStorage::StoredEntryResult> result;
    std::uintmax_t                                    totalSize = 0;
    while (totalSize < ApproximateMaxSize) {
        boost::optional<std::vector<DBLRUCacheStorage::StoredEntryResult>> popped =
            g_db_lru_cache.pop_one();
        if (!popped) {
            break;
        }

        std::vector<DBLRUCacheStorage::StoredEntryResult>& poppedVec = *popped;
        totalSize += CalculateDataSize(poppedVec);

        result.insert(result.end(), std::make_move_iterator(poppedVec.begin()),
                      std::make_move_iterator(poppedVec.end()));
    }
    return result;
}

enum PersistValueToCacheResult
{
    NoError,
    RecoverableError,
    UnrecoverableError,
};

static PersistValueToCacheResult MakeCacheErrorFromValue(const int errVal)
{
    if (errVal == MDB_MAP_FULL || errVal == MDB_BAD_TXN || errVal == MDB_NOTFOUND) {
        return PersistValueToCacheResult::RecoverableError;
    } else {
        return PersistValueToCacheResult::UnrecoverableError;
    }
}

#define ReturnIfError(db, action, dbidInt, res)                                                         \
    {                                                                                                   \
        if (res.isErr()) {                                                                              \
            const int errVal = res.UNWRAP_ERR();                                                        \
            NLog.write(b_sev::err,                                                                      \
                       "Encountered error {} while attempting persist data (in {} "                     \
                       ") in DBID {}",                                                                  \
                       errVal, action, dbidInt);                                                        \
            NLog.flush();                                                                               \
            return MakeCacheErrorFromValue(errVal);                                                     \
        }                                                                                               \
    }

template <typename BaseDB>
static PersistValueToCacheResult PersistValueToCache(BaseDB& persistedDB,
                                                     const DBLRUCacheStorage::StoredEntryResult& cache)
{
    const IDB::Index dbindex = static_cast<IDB::Index>(cache.dbid);
    // in all cases, we keep checking if an error occurred. If that's the case, we retry
    switch (cache.op) {
    case DBLRUCacheStorage::StoredOperationType::Write: {
        Result<void, int> writeRes = persistedDB.write(dbindex, cache.key, cache.value);
        ReturnIfError(db, "write", cache.dbid, writeRes);
        break;
    }
    case DBLRUCacheStorage::StoredOperationType::Erase:
        if (IDB::DuplicateKeysAllowed(dbindex)) {
            const Result<void, int> eraseRes = persistedDB.eraseAll(dbindex, cache.key);
            ReturnIfError(db, "erase (1)", cache.dbid, eraseRes);
        } else {
            const Result<void, int> eraseRes = persistedDB.erase(dbindex, cache.key);
            ReturnIfError(db, "erase (2)", cache.dbid, eraseRes);
        }
        break;
    }

    return PersistValueToCacheResult::NoError;
}

static std::vector<std::vector<DBLRUCacheStorage::StoredEntryResult>>
partition_data(const std::vector<DBLRUCacheStorage::StoredEntryResult>& v, std::size_t n)
{
    std::vector<std::vector<DBLRUCacheStorage::StoredEntryResult>> vec;

    // determine the total number of sub-vectors of size `n`
    std::size_t size = (v.size() - 1) / n + 1;

    // each iteration of this loop process the next set of `n` elements
    // and store it in a vector at k'th index in `vec`
    for (std::size_t k = 0; k < size; ++k) {
        // get range for the next set of `n` elements
        auto start_itr = std::next(v.cbegin(), k * n);
        auto end_itr   = std::next(v.cbegin(), k * n + n);

        // allocate memory for the sub-vector
        vec[k].resize(n);

        // code to handle the last sub-vector as it might
        // contain fewer elements
        if (k * n + n > v.size()) {
            end_itr = v.cend();
            vec[k].resize(v.size() - k * n);
        }

        // copy elements from the input range to the sub-vector
        std::copy(start_itr, end_itr, vec[k].begin());
    }

    return vec;
}

template <typename BaseDB>
bool DBLRUCacheLayer<BaseDB>::flush(const boost::optional<uint64_t>& commitSizeIn) const
{
    GUARD_FLUSH();

    int64_t commitSize = commitSizeIn ? *commitSizeIn : 1 << 24;

    while (true) {
        const std::vector<DBLRUCacheStorage::StoredEntryResult> dataToWrite =
            CollectSomeDataToPersist(1 << 20);
        if (dataToWrite.empty()) {
            break;
        }

        // 12 retries will increase the diskspace by 4096 times!
        static const int MAX_RETRIES = 12;

        PersistValueToCacheResult singlePersisResult = PersistValueToCacheResult::NoError;

        for (int c = 0; c < MAX_RETRIES; c++) {

            BaseDB persistedDB(dbdir_, false, DBCACHE_FLUSH_SIZE);

            const Result<void, int> dbBeginRes = persistedDB.beginDBTransaction(commitSize);
            if (dbBeginRes.isErr()) {
                NLog.write(b_sev::critical, "Failed to start DB transaction with error code: {}",
                           dbBeginRes.UNWRAP_ERR());
                continue;
            }

            singlePersisResult = PersistValueToCacheResult::NoError;

            for (const DBLRUCacheStorage::StoredEntryResult& data : dataToWrite) {
                singlePersisResult = PersistValueToCache(persistedDB, data);
                if (singlePersisResult != PersistValueToCacheResult::NoError) {
                    break;
                }
            }

            // since we're done writing, attempt to commit only if there's no errors
            if (singlePersisResult == PersistValueToCacheResult::NoError) {
                NLog.write(b_sev::info, "About to commit {} changes to persisted DB",
                           dataToWrite.size());
                const Result<void, int> commitRes = persistedDB.commitDBTransaction();
                if (commitRes.isErr()) {
                    NLog.write(b_sev::err, "Database flush() commit failed with error {}",
                               commitRes.UNWRAP_ERR());
                    singlePersisResult = MakeCacheErrorFromValue(commitRes.UNWRAP_ERR());
                }
            }

            // committing done at this point, let's see how successful we've been
            if (singlePersisResult == PersistValueToCacheResult::NoError) {
                cachedTxCount.store(0, boost::memory_order_seq_cst);
                NLog.write(b_sev::info, "A flush() in cached DB finished successfully");
                break;
            } else if (singlePersisResult == PersistValueToCacheResult::RecoverableError) {
                // grow the DB again and retry
                persistedDB.abortDBTransaction();
                static const int64_t IncrementFactor = 2;
                if (commitSize > std::numeric_limits<decltype(commitSize)>::max() / IncrementFactor) {
                    NLog.flush();
                    throw std::runtime_error("Unable to resize DB more than " +
                                             std::to_string(commitSize) +
                                             " as it will overflow. Failed to persist data in DB.");
                }
                commitSize *= IncrementFactor;
                NLog.flush();
                continue;
            } else {
                NLog.write(b_sev::critical,
                           "Canceling flushing to DB as an unrecoverable error occurred");
                persistedDB.abortDBTransaction();
                NLog.flush();
                break;
            }
        }
    }
    lruFlushCount++;
    return true;
}

template <typename BaseDB>
void DBLRUCacheLayer<BaseDB>::clearCache()
{
    GUARD_RW();

    g_db_lru_cache.clear();
}

template <typename BaseDB>
uint64_t DBLRUCacheLayer<BaseDB>::GetFlushCount()
{
    return lruFlushCount;
}

template class DBLRUCacheLayer<LMDB>;
template class DBLRUCacheLayer<DBReadCacheLayer>;

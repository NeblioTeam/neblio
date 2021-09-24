#include "dbcachelayer.h"

#include "db/lmdb/lmdb.h"
#include "logging/logger.h"
#include "util.h"
#include <boost/atomic.hpp>
#include <boost/scope_exit.hpp>

using ReadCacheMapsType = std::array<std::unordered_map<std::string, DBCachedRead>,
                                     static_cast<std::size_t>(IDB::Index::Index_Last)>;

ReadCacheMapsType g_cached_db_read_cache;

using MutexType = std::mutex;
MutexType g_cached_db_read_cache_lock;

boost::atomic_int64_t  approxCacheSize;
boost::atomic_uint64_t flushCount;

using namespace DBOperation;

DBCacheLayer::DBCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase,
                           int64_t flushOnSize)
    : flushOnSizeReached(flushOnSize), dbdir_(dbdir)
{
    DBCacheLayer::openDB(startNewDatabase);
}

Result<boost::optional<std::string>, int>
DBCacheLayer::read(Index dbindex, const std::string& key, std::size_t offset,
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

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        const auto&                map = g_cached_db_read_cache[static_cast<std::size_t>(dbindex)];

        auto it = map.find(key);
        if (it != map.cend()) {
            const DBCachedRead& cache = it->second;
            switch (cache.getOpType()) {
            case ReadOperationType::Erased:
                return Ok(boost::optional<std::string>());
            case ReadOperationType::NotFound:
                return Ok(boost::optional<std::string>());
            case ReadOperationType::ValueRead:
            case ReadOperationType::ValueWritten:
                // single values should NEVER be empty... so if it's empty, we refresh the cache to fix
                // the issue
                if (!cache.getValues().empty()) {
                    if (size) {
                        return Ok(boost::make_optional(cache.getValues().front().substr(offset, *size)));
                    } else {
                        return Ok(boost::make_optional(cache.getValues().front().substr(offset)));
                    }
                } else {
                    NLog.write(b_sev::warn,
                               "The value with dbid {} and key {} doesn't support "
                               "duplicates, but was found to be empty.",
                               dbindex, key);
                }
            }
        }

        LMDB persistedDB(dbdir_, false);

        Result<boost::optional<std::string>, int> rdVal = persistedDB.read(dbindex, key, 0, boost::none);
        if (rdVal.isOk()) {
            const boost::optional<std::string>& dVal = rdVal.UNWRAP();
            if (dVal) {
                g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
                    std::make_pair(key, DBCachedRead(ReadOperationType::ValueRead, *dVal)));
                approxCacheSize.fetch_add(dVal->size(), boost::memory_order_relaxed);
                if (size) {
                    return Ok(boost::make_optional(dVal->substr(offset, *size)));
                } else {
                    return Ok(boost::make_optional(dVal->substr(offset)));
                }
            }
        }
    }

    return Ok(boost::optional<std::string>());
}

Result<std::vector<std::string>, int> DBCacheLayer::readMultiple(Index              dbindex,
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
                return Ok(std::move(valuesToAppend));
            }
        }
    }

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        const auto&                map = g_cached_db_read_cache[static_cast<std::size_t>(dbindex)];

        auto it = map.find(key);
        if (it != map.cend()) {
            const DBCachedRead& cache = it->second;
            switch (cache.getOpType()) {
            case ReadOperationType::Erased:
                return Ok(std::move(valuesToAppend));
            case ReadOperationType::NotFound:
                return Ok(std::move(valuesToAppend));
            case ReadOperationType::ValueRead:
            case ReadOperationType::ValueWritten: {
                std::vector<std::string> result = cache.getValues();
                result.insert(result.end(), std::make_move_iterator(valuesToAppend.begin()),
                              std::make_move_iterator(valuesToAppend.end()));
                return Ok(std::move(result));
            }
            }
        }

        LMDB persistedDB(dbdir_, false);

        Result<std::vector<std::string>, int> rdVal = persistedDB.readMultiple(dbindex, key);
        if (rdVal.isOk()) {
            std::vector<std::string>& dVal = rdVal.unwrap("");
            g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
                std::make_pair(key, DBCachedRead(ReadOperationType::ValueRead, dVal)));
            for (const auto& v : dVal) {
                approxCacheSize.fetch_add(v.size(), boost::memory_order_relaxed);
            }
            dVal.insert(dVal.end(), std::make_move_iterator(valuesToAppend.begin()),
                        std::make_move_iterator(valuesToAppend.end()));
            return Ok(std::move(dVal));
        }
    }

    return Err(1);
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

Result<std::map<std::string, std::vector<std::string>>, int> DBCacheLayer::readAll(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    LMDB persistedDB(dbdir_, false);

    auto                                             tRes = persistedDB.readAll(dbindex);
    std::map<std::string, std::vector<std::string>>& res  = tRes.UNWRAP();

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        const auto& cacheMap = [&]() { return g_cached_db_read_cache[static_cast<int>(dbindex)]; }();

        for (auto&& cachePair : cacheMap) {
            auto&& key   = cachePair.first;
            auto&& cache = cachePair.second;
            switch (cache.getOpType()) {
            case ReadOperationType::ValueRead:
            case ReadOperationType::ValueWritten:
                if (!cache.getValues().empty()) {
                    res[key] = cache.getValues();
                }
                break;
            case ReadOperationType::Erased:
                res.erase(key);
                break;
            case ReadOperationType::NotFound:
                break;
            }
        }
    }

    // then above it the results from the tx, if any
    MergeTxDataWithData(res, std::move(txOps));

    return Ok(std::move(res));
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

Result<std::map<std::string, std::string>, int> DBCacheLayer::readAllUnique(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    LMDB persistedDB(dbdir_, false);

    auto tRes = persistedDB.readAllUnique(dbindex);

    std::map<std::string, std::string>& res = tRes.UNWRAP();

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        const auto& cacheMap = [&]() { return g_cached_db_read_cache[static_cast<int>(dbindex)]; }();

        for (auto&& cachePair : cacheMap) {
            auto&& key   = cachePair.first;
            auto&& cache = cachePair.second;
            switch (cache.getOpType()) {
            case ReadOperationType::ValueRead:
            case ReadOperationType::ValueWritten:
                if (!cache.getValues().empty()) {
                    res[key] = cache.getValues().front();
                }
                break;
            case ReadOperationType::Erased:
                res.erase(key);
                break;
            case ReadOperationType::NotFound:
                break;
            }
        }
    }

    // then above it the results from the tx, if any
    MergeTxDataWithData(res, std::move(txOps));

    return Ok(std::move(res));
}

void AppendValueToMap(IDB::Index dbindex, const std::string& key, const std::string& value)
{
    // for possible duplicates, if the key already exists and it's
    auto&& map = g_cached_db_read_cache[static_cast<std::size_t>(dbindex)];
    auto   it  = map.find(key);
    if (it != map.end()) {
        DBCachedRead& cache = it->second;
        switch (cache.getOpType()) {
        case ReadOperationType::ValueRead:
        case ReadOperationType::ValueWritten:
            cache.getValues().insert(cache.getValues().end(), value);
            cache.switchOpToWrite();
            break;
        case ReadOperationType::NotFound:
        case ReadOperationType::Erased:
            map.erase(key);
            map.insert(std::make_pair(key, DBCachedRead(ReadOperationType::ValueWritten, value)));
            approxCacheSize.fetch_add(value.size(), boost::memory_order_relaxed);
            break;
        }
    } else {
        map.insert(std::make_pair(key, DBCachedRead(ReadOperationType::ValueWritten, value)));
    }
}

Result<void, int> DBCacheLayer::write(Index dbindex, const std::string& key, const std::string& value)
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

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);

        if (IDB::DuplicateKeysAllowed(dbindex)) {
            // for possible duplicates, if the key already exists and it's
            AppendValueToMap(dbindex, key, value);
        } else {
            // if no duplicates are possible, we just overwrite
            g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].erase(key);
            g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
                std::make_pair(key, DBCachedRead(ReadOperationType::ValueWritten, value)));
            approxCacheSize.fetch_add(value.size(), boost::memory_order_relaxed);
        }
    }

    return Ok();
}

Result<void, int> DBCacheLayer::erase(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);

    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].erase(key);
    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
        std::make_pair(key, DBCachedRead(ReadOperationType::Erased, "")));

    return Ok();
}

Result<void, int> DBCacheLayer::eraseAll(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].erase(key);
    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
        std::make_pair(key, DBCachedRead(ReadOperationType::Erased, "")));

    return Ok();
}

Result<bool, int> DBCacheLayer::exists(Index dbindex, const std::string& key) const
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

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        const auto&                map = g_cached_db_read_cache[static_cast<std::size_t>(dbindex)];

        auto it = map.find(key);
        if (it != map.cend()) {
            const DBCachedRead& cache = it->second;
            switch (cache.getOpType()) {
            case ReadOperationType::Erased:
                return Ok(false);
            case ReadOperationType::NotFound:
                return Ok(false);
            case ReadOperationType::ValueRead:
            case ReadOperationType::ValueWritten:
                // values should never be empty unless duplicates allowed and the vector is empty... so
                // if it's empty, we refresh the cache to fix the issue
                if (!cache.getValues().empty()) {
                    return Ok(true);
                }
            }
        }

        LMDB persistedDB(dbdir_, false);

        Result<boost::optional<std::string>, int> rdVal = persistedDB.read(dbindex, key, 0, boost::none);
        if (rdVal.isOk()) {
            const boost::optional<std::string>& dVal = rdVal.UNWRAP();
            if (dVal) {
                g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
                    std::make_pair(key, DBCachedRead(ReadOperationType::ValueRead, *dVal)));
                approxCacheSize.fetch_add(dVal->size(), boost::memory_order_relaxed);
                return Ok(true);
            } else {
                return Ok(false);
            }
        } else {
            return Err(rdVal.UNWRAP_ERR());
        }
    }
}

void DBCacheLayer::clearDBData()
{
    clearCache();
    LMDB persistedDB(dbdir_, true);
}

Result<void, int> DBCacheLayer::beginDBTransaction(std::size_t /*expectedDataSize*/)
{
    if (tx) {
        return Err(-1);
    }
    tx = std::unique_ptr<HierarchicalDB<decltype(tx)::element_type::MutexT>>(
        new HierarchicalDB<decltype(tx)::element_type::MutexT>(""));
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

Result<void, int> DBCacheLayer::commitDBTransaction()
{
    if (!tx) {
        return Err(-1);
    }

    std::unique_ptr<HierarchicalDB<decltype(tx)::element_type::MutexT>> movedTx = std::move(tx);
    tx.reset();

    std::array<std::map<std::string, TransactionOperation>,
               static_cast<std::size_t>(IDB::Index::Index_Last)>
        txData = GetAllTxData(movedTx.get());

    bool result = true;

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);

        for (std::size_t i = 0; i < txData.size(); i++) {
            const IDB::Index                             dbid    = static_cast<IDB::Index>(i);
            std::map<std::string, TransactionOperation>& txOpMap = txData[i];
            for (const auto& kv_pair : txOpMap) {
                const std::string&          key = kv_pair.first;
                const TransactionOperation& op  = kv_pair.second;

                switch (op.getOpType()) {
                case WriteOperationType::Append:
                    for (const auto& val : op.getValues()) {
                        AppendValueToMap(dbid, key, val);
                    }
                    break;
                case WriteOperationType::UniqueSet:
                    if (!op.getValues().empty()) {
                        g_cached_db_read_cache[static_cast<std::size_t>(dbid)].erase(key);
                        approxCacheSize.fetch_add(op.getValues().front().size(),
                                                  boost::memory_order_release);
                        g_cached_db_read_cache[static_cast<std::size_t>(dbid)].insert(std::make_pair(
                            key, DBCachedRead(ReadOperationType::ValueWritten, op.getValues().front())));
                    }
                    break;
                case WriteOperationType::Erase:
                    g_cached_db_read_cache[static_cast<std::size_t>(dbid)].erase(key);
                    g_cached_db_read_cache[static_cast<std::size_t>(dbid)].insert(
                        std::make_pair(key, DBCachedRead(ReadOperationType::Erased, "")));
                    break;
                }
            }
        }
    }

    return result ? Result<void, int>(Ok()) : Err(-1);
}

bool DBCacheLayer::abortDBTransaction()
{
    tx.reset();
    return true;
}

boost::optional<boost::filesystem::path> DBCacheLayer::getDataDir() const { return *dbdir_; }

bool DBCacheLayer::openDB(bool clearDataBeforeOpen)
{
    if (clearDataBeforeOpen) {
        clearCache();
        DBCacheLayer::clearDBData();
        approxCacheSize.store(0, boost::memory_order_seq_cst);
        flushCount.store(0, boost::memory_order_seq_cst);
    }

    LMDB persistedDB(dbdir_, clearDataBeforeOpen);
    boost::atomic_thread_fence(boost::memory_order_seq_cst);

    return true;
}

void DBCacheLayer::close()
{
    tx.reset();
    flush();
    LMDB persistedDB(dbdir_, false);
    persistedDB.close();
    boost::atomic_thread_fence(boost::memory_order_seq_cst);
}

std::uintmax_t CalculateTotalDataSize_unsafe()
{
    std::uintmax_t sizeToAdd = 0;
    for (const auto& db : g_cached_db_read_cache) {
        for (const auto& cacheMap : db) {
            sizeToAdd += cacheMap.first.size();
            const DBCachedRead& entry = cacheMap.second;
            switch (entry.getOpType()) {
            case ReadOperationType::Erased:
            case ReadOperationType::NotFound:
                break;
            case ReadOperationType::ValueRead:
                break;
            case ReadOperationType::ValueWritten:
                for (const std::string& data : entry.getValues()) {
                    sizeToAdd += data.size();
                }
            }
        }
    }
    return sizeToAdd;
}

enum PersistValueToCacheResult
{
    NoError,
    RecoverableError,
    UnrecoverableError,
};

#define ReturnIfError(db, action, dbidInt, res)                                                         \
    {                                                                                                   \
        if (res.isErr()) {                                                                              \
            const int errVal = res.UNWRAP_ERR();                                                        \
            NLog.write(b_sev::err,                                                                      \
                       "Encountered error {} while attempting persist data (in {} "                     \
                       ") in DBID {}",                                                                  \
                       errVal, action, dbidInt);                                                        \
            if (errVal == MDB_MAP_FULL || errVal == MDB_BAD_TXN || errVal == MDB_NOTFOUND) {            \
                return PersistValueToCacheResult::RecoverableError;                                     \
            } else {                                                                                    \
                return PersistValueToCacheResult::UnrecoverableError;                                   \
            }                                                                                           \
        }                                                                                               \
    }

static PersistValueToCacheResult PersistValueToCache(LMDB& persistedDB, IDB::Index dbid,
                                                     const std::string& key, const DBCachedRead& cache)
{
    const int i = static_cast<int>(dbid);
    // in all cases, we keep checking if an error occurred. If that's the case, we retry
    switch (cache.getOpType()) {
    case ReadOperationType::ValueWritten:
        // we check if the data exists, if it does, we erase it before rewriting it
        {
            const Result<bool, int> keyExists = persistedDB.exists(dbid, key);
            ReturnIfError(db, "exists check", i, keyExists);
            if (keyExists.UNWRAP()) {
                if (IDB::DuplicateKeysAllowed(dbid)) {
                    const Result<void, int> eraseRes = persistedDB.eraseAll(dbid, key);
                    ReturnIfError(db, "erase (1) check", i, eraseRes);
                } else {
                    const Result<void, int> eraseRes = persistedDB.erase(dbid, key);
                    ReturnIfError(db, "erase (2) check", i, eraseRes);
                }
            }
            // we rewrite the data
            for (const auto& val : cache.getValues()) {
                const Result<void, int> writeRes = persistedDB.write(dbid, key, val);
                ReturnIfError(db, "write", i, writeRes);
            }
            break;
        }
    case ReadOperationType::ValueRead:
        break;
    case ReadOperationType::Erased:
        if (IDB::DuplicateKeysAllowed(dbid)) {
            const Result<void, int> eraseRes = persistedDB.eraseAll(dbid, key);
            ReturnIfError(db, "erase (3) check", i, eraseRes);
        } else {
            const Result<void, int> eraseRes = persistedDB.erase(dbid, key);
            ReturnIfError(db, "erase (4) check", i, eraseRes);
        }
        break;
    case ReadOperationType::NotFound:
        break;
    }

    return PersistValueToCacheResult::NoError;
}

bool DBCacheLayer::flush(const boost::optional<uint64_t>& commitSizeIn) const
{
    NLog.write(b_sev::info, "Starting flush to persisted DB");
    std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);

    // 12 retries will increase the diskspace by 4096 times!
    static const int MAX_RETRIES = 12;

    PersistValueToCacheResult singlePersisResult = PersistValueToCacheResult::NoError;

    const std::uintmax_t sizeToAdd = [&]() {
        if (!commitSizeIn)
            return 2 * CalculateTotalDataSize_unsafe();
        else
            return static_cast<std::uintmax_t>(*commitSizeIn);
    }();
    int64_t commitSize = 2 * std::max(static_cast<std::uintmax_t>(flushOnSizeReached), sizeToAdd);

    for (int c = 0; c < MAX_RETRIES; c++) {
        NLog.write(b_sev::info, "Attempt {} to persist data from cache to DB with size: {}", c,
                   sizeToAdd);

        LMDB persistedDB(dbdir_, false);

        singlePersisResult = PersistValueToCacheResult::NoError;

        const Result<void, int> dbBeginRes = persistedDB.beginDBTransaction(commitSize);
        if (dbBeginRes.isErr()) {
            NLog.write(b_sev::critical, "Failed to start DB transaction with error code: {}",
                       dbBeginRes.UNWRAP_ERR());
            continue;
        }

        for (std::size_t i = 0; i < g_cached_db_read_cache.size(); i++) {
            const IDB::Index                     dbid     = static_cast<IDB::Index>(i);
            const ReadCacheMapsType::value_type& cacheMap = g_cached_db_read_cache[i];
            for (auto&& cachePair : cacheMap) {
                auto&& key   = cachePair.first;
                auto&& cache = cachePair.second;

                singlePersisResult = PersistValueToCache(persistedDB, dbid, key, cache);
                if (singlePersisResult != PersistValueToCacheResult::NoError) {
                    break;
                }
            }

            if (singlePersisResult != PersistValueToCacheResult::NoError) {
                break;
            }
        }

        if (singlePersisResult == PersistValueToCacheResult::NoError) {
            NLog.write(b_sev::info, "About to commit to persisted DB");
            persistedDB.commitDBTransaction();
            clearCache_unsafe();
            NLog.write(b_sev::info, "A flush() in cached DB finish");
            break;
        } else if (singlePersisResult == PersistValueToCacheResult::RecoverableError) {
            // grow the DB again and retry
            persistedDB.abortDBTransaction();
            static const int64_t IncrementFactor = 2;
            if (commitSize > std::numeric_limits<decltype(commitSize)>::max() / IncrementFactor) {
                NLog.flush();
                throw std::runtime_error("Unable to resize DB more than " + std::to_string(commitSize) +
                                         " as it will overflow. Failed to persist data in DB.");
            }
            commitSize *= IncrementFactor;
            NLog.flush();
            continue;
        } else {
            NLog.write(b_sev::critical, "Canceling flushing to DB as an unrecoverable error occurred");
            persistedDB.abortDBTransaction();
            break;
        }
    }
    flushCount.fetch_add(1, boost::memory_order_release);
    return singlePersisResult == PersistValueToCacheResult::NoError;
}

boost::optional<bool> DBCacheLayer::flushOnPolicy() const
{
    if (flushOnSizeReached > 0) {
        if (approxCacheSize.load(boost::memory_order_acquire) > flushOnSizeReached) {
            bool res = flush();
            return boost::make_optional(res);
        }
    }
    return boost::none;
}

void DBCacheLayer::clearCache()
{
    std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
    clearCache_unsafe();
}

void DBCacheLayer::clearCache_unsafe() const
{
    for (auto&& m : g_cached_db_read_cache) {
        m.clear();
    }
    approxCacheSize.store(0, boost::memory_order_seq_cst);
}

uint64_t DBCacheLayer::GetFlushCount() { return flushCount; }

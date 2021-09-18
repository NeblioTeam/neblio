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

boost::optional<std::string> DBCacheLayer::read(Index dbindex, const std::string& key,
                                                std::size_t                         offset,
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
                        return txVal->getValues().front().substr(offset, *size);
                    } else {
                        return txVal->getValues().front().substr(offset);
                    }
                }
                break;
            case WriteOperationType::Erase:
                return boost::none;
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
                return boost::none;
            case ReadOperationType::NotFound:
                return boost::none;
            case ReadOperationType::ValueFound:
                // values should NEVER be empty... so if it's empty, we refresh the cache to fix the
                // issue
                if (!cache.getValues().empty()) {
                    if (size) {
                        return cache.getValues().front().substr(offset, *size);
                    } else {
                        return cache.getValues().front().substr(offset);
                    }
                }
            }
        }

        LMDB persistedDB(dbdir_, false);

        boost::optional<std::string> dVal = persistedDB.read(dbindex, key, 0, boost::none);
        if (dVal) {
            g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
                std::make_pair(key, DBCachedRead(ReadOperationType::ValueFound, *dVal)));
            approxCacheSize.fetch_add(dVal->size(), boost::memory_order_relaxed);
            if (size) {
                return dVal->substr(offset, *size);
            } else {
                return dVal->substr(offset);
            }
        }
    }

    return boost::none;
}

boost::optional<std::vector<std::string>> DBCacheLayer::readMultiple(Index              dbindex,
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
                return boost::make_optional(std::move(valuesToAppend));
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
                return boost::make_optional(std::move(valuesToAppend));
            case ReadOperationType::NotFound:
                return boost::make_optional(std::move(valuesToAppend));
            case ReadOperationType::ValueFound: {
                std::vector<std::string> result = cache.getValues();
                result.insert(result.end(), std::make_move_iterator(valuesToAppend.begin()),
                              std::make_move_iterator(valuesToAppend.end()));
                return result;
            }
            }
        }

        LMDB persistedDB(dbdir_, false);

        boost::optional<std::vector<std::string>> dVal = persistedDB.readMultiple(dbindex, key);
        if (dVal) {
            g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
                std::make_pair(key, DBCachedRead(ReadOperationType::ValueFound, *dVal)));
            for (const auto& v : *dVal) {
                approxCacheSize.fetch_add(v.size(), boost::memory_order_relaxed);
            }
            dVal->insert(dVal->end(), std::make_move_iterator(valuesToAppend.begin()),
                         std::make_move_iterator(valuesToAppend.end()));
            return dVal;
        }
    }

    return boost::none;
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

boost::optional<std::map<std::string, std::vector<std::string>>>
DBCacheLayer::readAll(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    LMDB persistedDB(dbdir_, false);

    auto res = persistedDB.readAll(dbindex);
    if (!res) {
        return boost::none;
    }

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        const auto& cacheMap = [&]() { return g_cached_db_read_cache[static_cast<int>(dbindex)]; }();

        for (auto&& cachePair : cacheMap) {
            auto&& key   = cachePair.first;
            auto&& cache = cachePair.second;
            switch (cache.getOpType()) {
            case ReadOperationType::ValueFound:
                if (!cache.getValues().empty()) {
                    (*res)[key] = cache.getValues();
                }
                break;
            case ReadOperationType::Erased:
                res->erase(key);
                break;
            case ReadOperationType::NotFound:
                break;
            }
        }

        // then above it the results from the tx, if any
        MergeTxDataWithData(*res, std::move(txOps));
    }

    return res;
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

boost::optional<std::map<std::string, std::string>> DBCacheLayer::readAllUnique(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    LMDB persistedDB(dbdir_, false);
    auto res = persistedDB.readAllUnique(dbindex);
    if (!res) {
        return boost::none;
    }

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        const auto& cacheMap = [&]() { return g_cached_db_read_cache[static_cast<int>(dbindex)]; }();

        for (auto&& cachePair : cacheMap) {
            auto&& key   = cachePair.first;
            auto&& cache = cachePair.second;
            switch (cache.getOpType()) {
            case ReadOperationType::ValueFound:
                if (!cache.getValues().empty()) {
                    (*res)[key] = cache.getValues().front();
                }
                break;
            case ReadOperationType::Erased:
                res->erase(key);
                break;
            case ReadOperationType::NotFound:
                break;
            }
        }
    }

    // then above it the results from the tx, if any
    MergeTxDataWithData(*res, std::move(txOps));

    return res;
}

void AppendValueToMap(IDB::Index dbindex, const std::string& key, const std::string& value)
{
    // for possible duplicates, if the key already exists and it's
    auto&& map = g_cached_db_read_cache[static_cast<std::size_t>(dbindex)];
    auto   it  = map.find(key);
    if (it != map.end()) {
        DBCachedRead& cache = it->second;
        switch (cache.getOpType()) {
        case ReadOperationType::ValueFound:
            cache.getValues().insert(cache.getValues().end(), value);
            break;
        case ReadOperationType::NotFound:
        case ReadOperationType::Erased:
            map.erase(key);
            map.insert(std::make_pair(key, DBCachedRead(ReadOperationType::ValueFound, value)));
            approxCacheSize.fetch_add(value.size(), boost::memory_order_relaxed);
            break;
        }
    } else {
        map.insert(std::make_pair(key, DBCachedRead(ReadOperationType::ValueFound, value)));
    }
}

bool DBCacheLayer::write(Index dbindex, const std::string& key, const std::string& value)
{
    if (tx) {
        if (IDB::DuplicateKeysAllowed(dbindex)) {
            return tx->multi_append(static_cast<int>(dbindex), key, value);
        } else {
            return tx->unique_set(static_cast<int>(dbindex), key, value);
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
                std::make_pair(key, DBCachedRead(ReadOperationType::ValueFound, value)));
            approxCacheSize.fetch_add(value.size(), boost::memory_order_relaxed);
        }
    }

    return true;
}

bool DBCacheLayer::erase(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key);
    }

    std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);

    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].erase(key);
    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
        std::make_pair(key, DBCachedRead(ReadOperationType::Erased, "")));

    return true;
}

bool DBCacheLayer::eraseAll(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key);
    }

    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].erase(key);
    g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
        std::make_pair(key, DBCachedRead(ReadOperationType::Erased, "")));

    return true;
}

bool DBCacheLayer::exists(Index dbindex, const std::string& key) const
{
    if (tx) {
        boost::optional<TransactionOperation> txVal = tx->getOp(static_cast<int>(dbindex), key);
        if (txVal) {
            switch (txVal->getOpType()) {
            case WriteOperationType::Append:
            case WriteOperationType::UniqueSet:
                if (!txVal->getValues().empty()) {
                    return true;
                }
                break;
            case WriteOperationType::Erase:
                return false;
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
                return false;
            case ReadOperationType::NotFound:
                return false;
            case ReadOperationType::ValueFound:
                // values should NEVER be empty... so if it's empty, we refresh the cache to fix the
                // issue
                if (!cache.getValues().empty()) {
                    return true;
                }
            }
        }

        LMDB persistedDB(dbdir_, false);

        boost::optional<std::string> dVal = persistedDB.read(dbindex, key, 0, boost::none);
        if (dVal) {
            g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
                std::make_pair(key, DBCachedRead(ReadOperationType::ValueFound, *dVal)));
            approxCacheSize.fetch_add(dVal->size(), boost::memory_order_relaxed);
            return true;
        }
    }

    return false;
}

void DBCacheLayer::clearDBData()
{
    clearCache();
    LMDB persistedDB(dbdir_, true);
}

bool DBCacheLayer::beginDBTransaction(std::size_t /*expectedDataSize*/)
{
    if (tx) {
        return false;
    }
    tx = std::unique_ptr<HierarchicalDB<decltype(tx)::element_type::MutexT>>(
        new HierarchicalDB<decltype(tx)::element_type::MutexT>(""));
    return true;
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

bool DBCacheLayer::commitDBTransaction()
{
    if (!tx) {
        return false;
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
                            key, DBCachedRead(ReadOperationType::ValueFound, op.getValues().front())));
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

    return result;
}

bool DBCacheLayer::abortDBTransaction()
{
    if (!tx) {
        return false;
    }
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
            case ReadOperationType::ValueFound:
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

#define ReturnIfError(db, action)                                                                       \
    {                                                                                                   \
        if (persistedDB.getLastError() != 0) {                                                          \
            NLog.write(b_sev::err,                                                                      \
                       "Encountered error {} while attempting persist data (in {} "                     \
                       ") in DBID {}",                                                                  \
                       persistedDB.getLastError(), action, i);                                          \
            if (persistedDB.getLastError() == MDB_MAP_FULL ||                                           \
                persistedDB.getLastError() == MDB_BAD_TXN ||                                            \
                persistedDB.getLastError() == MDB_NOTFOUND) {                                           \
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
    case ReadOperationType::ValueFound:
        // we check if the data exists, if it does, we erase it before rewriting it
        if (persistedDB.exists(dbid, key)) {
            ReturnIfError(db, "exists check");
            if (IDB::DuplicateKeysAllowed(dbid)) {
                persistedDB.eraseAll(dbid, key);
            } else {
                persistedDB.erase(dbid, key);
            }
            ReturnIfError(db, "erase (1)");
        }
        // we rewrite the data
        for (const auto& val : cache.getValues()) {
            persistedDB.write(dbid, key, val);
            ReturnIfError(db, "write");
        }
        break;
    case ReadOperationType::Erased:
        if (IDB::DuplicateKeysAllowed(dbid)) {
            persistedDB.eraseAll(dbid, key);
        } else {
            persistedDB.erase(dbid, key);
        }
        ReturnIfError(db, "erase (2)");
        break;
    case ReadOperationType::NotFound:
        break;
    }

    return PersistValueToCacheResult::NoError;
}

bool DBCacheLayer::flush() const
{
    NLog.write(b_sev::info, "Starting flush to persisted DB");
    std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);

    // 12 retries will increase the diskspace by 4096 times!
    static const int MAX_RETRIES = 12;

    PersistValueToCacheResult singlePersisResult = PersistValueToCacheResult::NoError;

    const std::uintmax_t sizeToAdd = 2 * CalculateTotalDataSize_unsafe();
    int64_t commitSize = 2 * std::max(static_cast<std::uintmax_t>(flushOnSizeReached), sizeToAdd);

    for (int c = 0; c < MAX_RETRIES; c++) {
        NLog.write(b_sev::info, "Attempt {} to persist da8ta from cache to DB with size: {}", c,
                   sizeToAdd);

        LMDB persistedDB(dbdir_, false);

        singlePersisResult = PersistValueToCacheResult::NoError;

        persistedDB.beginDBTransaction(commitSize);

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
    approxCacheSize.store(0, boost::memory_order_release);
    return singlePersisResult == PersistValueToCacheResult::NoError;
}

boost::optional<bool> DBCacheLayer::flushOnPolicy() const
{
    if (flushOnSizeReached > 0) {
        if (approxCacheSize.load(boost::memory_order_acquire) > flushOnSizeReached) {
            bool res = flush();
            flushCount.fetch_add(res, boost::memory_order_release);
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

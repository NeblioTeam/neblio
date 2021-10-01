#include "dbreadcachelayer.h"

#include "concurrentmap/ConcurrentMap.h"
#include "db/lmdb/lmdb.h"
#include "logging/logger.h"
#include "util.h"
#include <boost/atomic.hpp>
#include <boost/scope_exit.hpp>
#include <cstddef>

using ReadCacheMapType  = ConcurrentMap<std::string, DBCachedRead, 5000>;
using ReadCacheMapsType = std::array<ReadCacheMapType, static_cast<std::size_t>(IDB::Index::Index_Last)>;

ReadCacheMapsType g_db_read_cache;

static boost::atomic_int64_t  approxReadCacheSize;
static boost::atomic_uint64_t readCacheFlushCount;

using namespace DBOperation;

static boost::atomic_uint_fast32_t ReadCacheRWCount{0};
static boost::atomic_uint_fast32_t ReadCacheTxCount{0};
static boost::atomic_flag          ReadCacheRaceGuard = BOOST_ATOMIC_FLAG_INIT;

// the only guarding we do is protect that a tx and a read/write happen together, to ensure consistency
#define GUARD_RW()                                                                                      \
    boost::atomic_thread_fence(boost::memory_order_acquire);                                            \
    {                                                                                                   \
        while (ReadCacheRaceGuard.test_and_set()) {                                                     \
        }                                                                                               \
        BOOST_SCOPE_EXIT(void) { ReadCacheRaceGuard.clear(); }                                          \
        BOOST_SCOPE_EXIT_END                                                                            \
                                                                                                        \
        while (ReadCacheTxCount > 0) {                                                                  \
        }                                                                                               \
        ReadCacheRWCount++;                                                                             \
    }                                                                                                   \
    boost::atomic_thread_fence(boost::memory_order_release);                                            \
    BOOST_SCOPE_EXIT(void) { ReadCacheRWCount--; }                                                      \
    BOOST_SCOPE_EXIT_END                                                                                \
    do {                                                                                                \
    } while (0)

#define GUARD_TX()                                                                                      \
    boost::atomic_thread_fence(boost::memory_order_acquire);                                            \
    {                                                                                                   \
        while (ReadCacheRaceGuard.test_and_set()) {                                                     \
        }                                                                                               \
        BOOST_SCOPE_EXIT(void) { ReadCacheRaceGuard.clear(); }                                          \
        BOOST_SCOPE_EXIT_END                                                                            \
                                                                                                        \
        ReadCacheTxCount++;                                                                             \
        while (ReadCacheRWCount > 0) {                                                                  \
        }                                                                                               \
    }                                                                                                   \
    boost::atomic_thread_fence(boost::memory_order_release);                                            \
    BOOST_SCOPE_EXIT(void) { ReadCacheTxCount--; }                                                      \
    BOOST_SCOPE_EXIT_END                                                                                \
    do {                                                                                                \
    } while (0)

DBReadCacheLayer::DBReadCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase,
                                   int64_t flushOnSize)
    : flushOnSizeReached(flushOnSize), dbdir_(dbdir)
{
    DBReadCacheLayer::openDB(startNewDatabase);
}

Result<boost::optional<std::string>, int>
DBReadCacheLayer::read(Index dbindex, const std::string& key, std::size_t offset,
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

        {
            GUARD_RW();

            auto item = g_db_read_cache[static_cast<std::size_t>(dbindex)].get(key);

            if (item) {
                const DBCachedRead& cache = *item;
                switch (cache.getOpType()) {
                case ReadOperationType::Erased:
                    return Ok(boost::optional<std::string>());
                case ReadOperationType::NotFound:
                    return Ok(boost::optional<std::string>());
                case ReadOperationType::ValueRead:
                case ReadOperationType::ValueWritten:
                    // single values should NEVER be empty... so if it's empty, we refresh the cache to
                    // fix the issue
                    if (!cache.getValues().empty()) {
                        if (size) {
                            return Ok(
                                boost::make_optional(cache.getValues().front().substr(offset, *size)));
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
        }

        LMDB persistedDB(dbdir_, false);

        Result<boost::optional<std::string>, int> rdVal = persistedDB.read(dbindex, key, 0, boost::none);
        if (rdVal.isOk()) {
            const boost::optional<std::string>& dVal = rdVal.UNWRAP();
            if (dVal) {
                g_db_read_cache[static_cast<std::size_t>(dbindex)].set(
                    key, DBCachedRead(ReadOperationType::ValueRead, *dVal));
                approxReadCacheSize.fetch_add(dVal->size(), boost::memory_order_relaxed);
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

Result<std::vector<std::string>, int> DBReadCacheLayer::readMultiple(Index              dbindex,
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
        {
            GUARD_RW();

            const auto& map = g_db_read_cache[static_cast<std::size_t>(dbindex)];

            auto item = map.get(key);
            if (item) {
                const DBCachedRead& cache = *item;
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
        }

        LMDB persistedDB(dbdir_, false);

        Result<std::vector<std::string>, int> rdVal = persistedDB.readMultiple(dbindex, key);
        if (rdVal.isOk()) {
            std::vector<std::string>& dVal = rdVal.UNWRAP();
            g_db_read_cache[static_cast<std::size_t>(dbindex)].set(
                key, DBCachedRead(ReadOperationType::ValueRead, dVal));
            for (const auto& v : dVal) {
                approxReadCacheSize.fetch_add(v.size(), boost::memory_order_relaxed);
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

Result<std::map<std::string, std::vector<std::string>>, int>
DBReadCacheLayer::readAll(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    LMDB persistedDB(dbdir_, false);

    GUARD_RW();

    auto tRes = persistedDB.readAll(dbindex);
    if (tRes.isErr()) {
        return tRes;
    }
    std::map<std::string, std::vector<std::string>>& res = tRes.UNWRAP();

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        const auto& cacheMap = g_db_read_cache[static_cast<int>(dbindex)].getAllData();

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

Result<std::map<std::string, std::string>, int> DBReadCacheLayer::readAllUnique(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    LMDB persistedDB(dbdir_, false);

    GUARD_RW();

    auto tRes = persistedDB.readAllUnique(dbindex);
    if (tRes.isErr()) {
        return tRes;
    }
    std::map<std::string, std::string>& res = tRes.UNWRAP();

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    {
        const auto& cacheMap = g_db_read_cache[static_cast<int>(dbindex)].getAllData();

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

static void AppendValueToMap(IDB::Index dbindex, const std::string& key, const std::string& value)
{
    // for possible duplicates, if the key already exists and it's
    auto&& map = g_db_read_cache[static_cast<std::size_t>(dbindex)];
    map.apply(key, [&](ReadCacheMapType::BucketMapType& m, const std::string& k) {
        // append only if we have the key already, otherwise we won't be in sync with the
        // permanent DB (either we read the db then append, or do nothing)

        auto it = m.find(k);
        if (it != m.end()) {
            DBCachedRead& cache = it->second;
            switch (cache.getOpType()) {
            case ReadOperationType::ValueRead:
            case ReadOperationType::ValueWritten:
                cache.getValues().insert(cache.getValues().end(), value);
                cache.switchOpToWrite();
                break;
            case ReadOperationType::NotFound:
            case ReadOperationType::Erased:
                m.erase(k);
                m.insert(std::make_pair(k, DBCachedRead(ReadOperationType::ValueWritten, value)));
                approxReadCacheSize.fetch_add(value.size(), boost::memory_order_relaxed);
                break;
            }
        } else {
            // we don't do the insert because we should first read the value from the database then
            // append to it
        }
    });
}

Result<void, int> DBReadCacheLayer::write(Index dbindex, const std::string& key,
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

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    GUARD_RW();

    {
        // since this is a read-cache, we first apply the operation in the database, then we update the
        // cache
        LMDB persistedDB(dbdir_, false);

        Result<void, int> writeRes = persistedDB.write(dbindex, key, value);
        if (writeRes.isErr()) {
            return writeRes;
        }
    }

    {
        if (IDB::DuplicateKeysAllowed(dbindex)) {
            AppendValueToMap(dbindex, key, value);
        } else {
            // if no duplicates are possible, we just overwrite
            g_db_read_cache[static_cast<std::size_t>(dbindex)].set(
                key, DBCachedRead(ReadOperationType::ValueWritten, value));
            approxReadCacheSize.fetch_add(value.size(), boost::memory_order_relaxed);
        }
    }

    return Ok();
}

Result<void, int> DBReadCacheLayer::erase(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    GUARD_RW();

    {
        // since this is a read-cache, we first apply the operation in the database, then we update the
        // cache
        LMDB persistedDB(dbdir_, false);

        Result<void, int> eraseRes = persistedDB.erase(dbindex, key);
        if (eraseRes.isErr()) {
            return eraseRes;
        }
    }

    g_db_read_cache[static_cast<std::size_t>(dbindex)].set(key,
                                                           DBCachedRead(ReadOperationType::Erased, ""));

    return Ok();
}

Result<void, int> DBReadCacheLayer::eraseAll(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    GUARD_RW();

    {
        // since this is a read-cache, we first apply the operation in the database, then we update the
        // cache
        LMDB persistedDB(dbdir_, false);

        Result<void, int> eraseRes = persistedDB.eraseAll(dbindex, key);
        if (eraseRes.isErr()) {
            return eraseRes;
        }
    }

    g_db_read_cache[static_cast<std::size_t>(dbindex)].set(key,
                                                           DBCachedRead(ReadOperationType::Erased, ""));

    return Ok();
}

Result<bool, int> DBReadCacheLayer::exists(Index dbindex, const std::string& key) const
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
        GUARD_RW();

        const auto& map = g_db_read_cache[static_cast<std::size_t>(dbindex)];

        auto item = map.get(key);
        if (item) {
            const DBCachedRead& cache = *item;
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
                g_db_read_cache[static_cast<std::size_t>(dbindex)].set(
                    key, DBCachedRead(ReadOperationType::ValueRead, *dVal));
                approxReadCacheSize.fetch_add(dVal->size(), boost::memory_order_relaxed);
                return Ok(true);
            } else {
                return Ok(false);
            }
        } else {
            return Err(rdVal.UNWRAP_ERR());
        }
    }
}

void DBReadCacheLayer::clearDBData()
{
    clearCache();
    LMDB persistedDB(dbdir_, true);
}

Result<void, int> DBReadCacheLayer::beginDBTransaction(std::size_t /*expectedDataSize*/)
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

Result<void, int> DBReadCacheLayer::commitDBTransaction()
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

    GUARD_TX();

    LMDB persistedDB(dbdir_, false);
    persistedDB.beginDBTransaction(1 << 24);

    {
        for (std::size_t i = 0; i < txData.size(); i++) {
            const IDB::Index                             dbid    = static_cast<IDB::Index>(i);
            std::map<std::string, TransactionOperation>& txOpMap = txData[i];
            for (const auto& kv_pair : txOpMap) {
                const std::string&          key = kv_pair.first;
                const TransactionOperation& op  = kv_pair.second;

                switch (op.getOpType()) {
                case WriteOperationType::Append:
                    for (const auto& val : op.getValues()) {
                        Result<void, int> writeRes = persistedDB.write(dbid, key, val);
                        if (writeRes.isErr()) {
                            return writeRes;
                        }
                    }
                    break;
                case WriteOperationType::UniqueSet:
                    if (!op.getValues().empty()) {
                        Result<void, int> writeRes =
                            persistedDB.write(dbid, key, op.getValues().front());
                        if (writeRes.isErr()) {
                            return writeRes;
                        }
                    }
                    break;
                case WriteOperationType::Erase:
                    g_db_read_cache[static_cast<std::size_t>(dbid)].set(
                        key, DBCachedRead(ReadOperationType::Erased, ""));
                    break;
                }
            }
        }
    }

    {
        Result<void, int> commitRes = persistedDB.commitDBTransaction();
        if (commitRes.isErr()) {
            NLog.write(b_sev::critical, "Failed to commit transaction");
            return commitRes;
        }
    }

    {
        for (std::size_t i = 0; i < txData.size(); i++) {
            const IDB::Index                             dbid    = static_cast<IDB::Index>(i);
            std::map<std::string, TransactionOperation>& txOpMap = txData[i];
            for (const auto& kv_pair : txOpMap) {
                const std::string&          key = kv_pair.first;
                const TransactionOperation& op  = kv_pair.second;

                switch (op.getOpType()) {
                case WriteOperationType::Append:
                    // append only if we have the key already, otherwise we won't be in sync with the
                    // permanent DB (either we read the db then append, or do nothing)
                    //
                    // the next line is just an optimization in order not to rerun the function again and
                    // again
                    if (g_db_read_cache[static_cast<std::size_t>(dbid)].get(key)) {
                        for (const auto& val : op.getValues()) {
                            AppendValueToMap(dbid, key, val);
                        }
                    }
                    break;
                case WriteOperationType::UniqueSet:
                    if (!op.getValues().empty()) {
                        approxReadCacheSize.fetch_add(op.getValues().front().size(),
                                                      boost::memory_order_release);
                        g_db_read_cache[static_cast<std::size_t>(dbid)].set(
                            key, DBCachedRead(ReadOperationType::ValueWritten, op.getValues().front()));
                    }
                    break;
                case WriteOperationType::Erase:
                    g_db_read_cache[static_cast<std::size_t>(dbid)].set(
                        key, DBCachedRead(ReadOperationType::Erased, ""));
                    break;
                }
            }
        }
    }

    return result ? Result<void, int>(Ok()) : Err(-1);
}

bool DBReadCacheLayer::abortDBTransaction()
{
    tx.reset();
    return true;
}

boost::optional<boost::filesystem::path> DBReadCacheLayer::getDataDir() const { return *dbdir_; }

bool DBReadCacheLayer::openDB(bool clearDataBeforeOpen)
{
    if (clearDataBeforeOpen) {
        clearCache();
        DBReadCacheLayer::clearDBData();
        approxReadCacheSize.store(0, boost::memory_order_seq_cst);
        readCacheFlushCount.store(0, boost::memory_order_seq_cst);
    }

    LMDB persistedDB(dbdir_, clearDataBeforeOpen);
    boost::atomic_thread_fence(boost::memory_order_seq_cst);

    return true;
}

void DBReadCacheLayer::close()
{
    tx.reset();
    flush();
    LMDB persistedDB(dbdir_, false);
    persistedDB.close();
    boost::atomic_thread_fence(boost::memory_order_seq_cst);
}

bool DBReadCacheLayer::flush(const boost::optional<uint64_t>&) const
{
    NLog.write(b_sev::info, "Clearning cache");

    clearCache();

    readCacheFlushCount.fetch_add(1, boost::memory_order_release);
    return true;
}

boost::optional<bool> DBReadCacheLayer::flushOnPolicy() const
{
    if (flushOnSizeReached > 0) {
        if (approxReadCacheSize.load(boost::memory_order_acquire) > flushOnSizeReached) {
            bool res = flush();
            return boost::make_optional(res);
        }
    }
    return boost::none;
}

void DBReadCacheLayer::clearCache() const
{
    GUARD_RW();
    clearCache_unsafe();
}

void DBReadCacheLayer::clearCache_unsafe() const
{
    for (auto&& m : g_db_read_cache) {
        m.clear();
    }
    approxReadCacheSize.store(0, boost::memory_order_seq_cst);
}

uint64_t DBReadCacheLayer::GetFlushCount() { return readCacheFlushCount; }

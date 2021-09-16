#include "dbcachelayer.h"

#include "db/lmdb/lmdb.h"
#include <boost/atomic.hpp>

std::unique_ptr<IDB> g_cached_db_instance;
std::array<std::map<std::string, DBCachedRead>, static_cast<std::size_t>(IDB::Index::Index_Last)>
    g_cached_db_read_cache;

using MutexType = std::mutex;
MutexType g_cached_db_read_cache_lock;

boost::atomic_int64_t  approxCacheSize;
boost::atomic_uint64_t flushCount;

using namespace DBOperation;

DBCacheLayer::DBCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase,
                           int64_t flushOnSize)
    : flushOnSizeReached(flushOnSize)
{
    if (startNewDatabase) {
        clearCache();
    }
    if (!g_cached_db_instance || startNewDatabase) {
        approxCacheSize.store(0, boost::memory_order_seq_cst);
        flushCount.store(0, boost::memory_order_seq_cst);
        g_cached_db_instance = std::unique_ptr<IDB>(new LMDB(dbdir, startNewDatabase));
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
    }
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
            // values should NEVER be empty... so if it's empty, we refresh the cache to fix the issue
            if (!cache.getValues().empty()) {
                if (size) {
                    return cache.getValues().front().substr(offset, *size);
                } else {
                    return cache.getValues().front().substr(offset);
                }
            }
        }
    }

    boost::optional<std::string> dVal = g_cached_db_instance->read(dbindex, key);
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

    boost::optional<std::vector<std::string>> dVal = g_cached_db_instance->readMultiple(dbindex, key);
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

    const auto& cacheMap = [&]() {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        return g_cached_db_read_cache[static_cast<int>(dbindex)];
    }();

    auto res = g_cached_db_instance->readAll(dbindex);
    if (!res) {
        return boost::none;
    }

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

    const auto& cacheMap = [&]() {
        std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
        return g_cached_db_read_cache[static_cast<int>(dbindex)];
    }();

    auto res = g_cached_db_instance->readAllUnique(dbindex);
    if (!res) {
        return boost::none;
    }

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
    flushOnSizePolicy();
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
            // values should NEVER be empty... so if it's empty, we refresh the cache to fix the issue
            if (!cache.getValues().empty()) {
                return true;
            }
        }
    }

    boost::optional<std::string> dVal = g_cached_db_instance->read(dbindex, key);
    if (dVal) {
        g_cached_db_read_cache[static_cast<std::size_t>(dbindex)].insert(
            std::make_pair(key, DBCachedRead(ReadOperationType::ValueFound, *dVal)));
        return true;
    }

    return false;
}

void DBCacheLayer::clearDBData()
{
    clearCache();
    g_cached_db_instance->clearDBData();
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
    flushOnSizePolicy();
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

boost::optional<boost::filesystem::path> DBCacheLayer::getDataDir() const
{
    return g_cached_db_instance->getDataDir();
}

bool DBCacheLayer::openDB(bool clearDataBeforeOpen)
{
    if (clearDataBeforeOpen) {
        DBCacheLayer::clearDBData();
    }
    return g_cached_db_instance->openDB(clearDataBeforeOpen);
}

void DBCacheLayer::close()
{
    tx.reset();
    g_cached_db_instance->close();
    g_cached_db_instance.reset();
    boost::atomic_thread_fence(boost::memory_order_seq_cst);
}

bool DBCacheLayer::flush()
{
    std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
    g_cached_db_instance->beginDBTransaction(500000);
    bool result = true;
    for (std::size_t i = 0; i < g_cached_db_read_cache.size(); i++) {
        const IDB::Index                     dbid     = static_cast<IDB::Index>(i);
        std::map<std::string, DBCachedRead>& cacheMap = g_cached_db_read_cache[i];
        for (auto&& cachePair : cacheMap) {
            auto&& key   = cachePair.first;
            auto&& cache = cachePair.second;
            switch (cache.getOpType()) {
            case ReadOperationType::ValueFound:
                g_cached_db_instance->eraseAll(dbid, key);
                for (auto&& val : cache.getValues()) {
                    result = result && g_cached_db_instance->write(dbid, key, std::move(val));
                }
                break;
            case ReadOperationType::Erased:
                g_cached_db_instance->eraseAll(dbid, key);
                break;
            case ReadOperationType::NotFound:
                break;
            }
        }
        cacheMap.clear();
    }
    g_cached_db_instance->commitDBTransaction();
    approxCacheSize.store(0, boost::memory_order_release);
    return true;
}

boost::optional<bool> DBCacheLayer::flushOnSizePolicy()
{
    if (flushOnSizeReached == 0) {
        return boost::none;
    }

    if (approxCacheSize.load(boost::memory_order_acquire)) {
        bool res = flush();
        flushCount.fetch_add(res, boost::memory_order_release);
        return boost::make_optional(res);
    }

    return boost::none;
}

void DBCacheLayer::clearCache()
{
    std::lock_guard<MutexType> lg(g_cached_db_read_cache_lock);
    for (auto&& m : g_cached_db_read_cache) {
        m.clear();
    }
    approxCacheSize.store(0, boost::memory_order_seq_cst);
}

uint64_t DBCacheLayer::GetFlushCount() { return flushCount; }

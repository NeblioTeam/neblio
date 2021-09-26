#include "dblrucachelayer.h"

#include "db/lmdb/lmdb.h"
#include "dblrucachestorage.h"
#include "logging/logger.h"
#include "util.h"
#include <boost/atomic.hpp>
#include <boost/scope_exit.hpp>
#include <vector>

using namespace DBOperation;

static DBLRUCacheStorage g_db_lru_cache;

static boost::atomic_int64_t  approxLRUCacheSize;
static boost::atomic_uint64_t lruFlushCount;

DBLRUCacheLayer::DBLRUCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase,
                                 int64_t flushOnSize)
    : flushOnSizeReached(flushOnSize), dbdir_(dbdir)
{
    DBLRUCacheLayer::openDB(startNewDatabase);
}

Result<boost::optional<std::string>, int>
DBLRUCacheLayer::read(Index dbindex, const std::string& key, std::size_t offset,
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
        const int                                                   dbid = static_cast<int>(dbindex);
        const boost::optional<DBLRUCacheStorage::StoredEntryResult> okv =
            g_db_lru_cache.get_one(dbid, key);

        if (okv) {
            const DBLRUCacheStorage::StoredEntryResult& kv = *okv;
            switch (kv.op) {
            case DBLRUCacheStorage::StoredOperationType::Erase:
                return Ok(boost::optional<std::string>());
            case DBLRUCacheStorage::StoredOperationType::Write:
                // single values should NEVER be empty... so if it's empty, we refresh the cache to fix
                // the issue
                if (size) {
                    return Ok(boost::make_optional(kv.value.substr(offset, *size)));
                } else {
                    return Ok(boost::make_optional(kv.value.substr(offset)));
                }
            }
        }

        LMDB persistedDB(dbdir_, false);

        Result<boost::optional<std::string>, int> rdVal = persistedDB.read(dbindex, key, 0, boost::none);
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

    return Ok(boost::optional<std::string>());
}

Result<std::vector<std::string>, int> DBLRUCacheLayer::readMultiple(Index              dbindex,
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

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

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
                cachedWrites.insert(cachedWrites.end(), std::make_move_iterator(valuesToAppend.begin()),
                                    std::make_move_iterator(valuesToAppend.end()));
                return Ok(std::move(cachedWrites));
            case DBLRUCacheStorage::Write:
                cachedWrites.push_back(kv.value);
            }
        }
    }

    cachedWrites.insert(cachedWrites.end(), std::make_move_iterator(valuesToAppend.begin()),
                        std::make_move_iterator(valuesToAppend.end()));

    LMDB persistedDB(dbdir_, false);

    Result<std::vector<std::string>, int> rdVal = persistedDB.readMultiple(dbindex, key);
    if (rdVal.isOk()) {
        std::vector<std::string>& dVal = rdVal.UNWRAP();
        dVal.insert(dVal.end(), std::make_move_iterator(cachedWrites.begin()),
                    std::make_move_iterator(cachedWrites.end()));
        return Ok(std::move(dVal));
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

Result<std::map<std::string, std::vector<std::string>>, int>
DBLRUCacheLayer::readAll(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>> allCachedWrites =
        g_db_lru_cache.getAll(static_cast<int>(dbindex));

    LMDB persistedDB(dbdir_, false);

    auto tRes = persistedDB.readAll(dbindex);
    if (tRes.isErr()) {
        return tRes;
    }
    std::map<std::string, std::vector<std::string>>& res = tRes.UNWRAP();

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

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

Result<std::map<std::string, std::string>, int> DBLRUCacheLayer::readAllUnique(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txOps;
    if (tx) {
        txOps = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>> allCachedWrites =
        g_db_lru_cache.getAll(static_cast<int>(dbindex));

    LMDB persistedDB(dbdir_, false);

    auto tRes = persistedDB.readAllUnique(dbindex);
    if (tRes.isErr()) {
        return tRes;
    }
    std::map<std::string, std::string>& res = tRes.UNWRAP();

    BOOST_SCOPE_EXIT(this_) { this_->flushOnPolicy(); }
    BOOST_SCOPE_EXIT_END

    // first we apply cached writes
    MergeCachedWritesWithData(res, std::move(allCachedWrites));

    // then above it the results from the tx, if any
    MergeTxDataWithData(res, std::move(txOps));

    return Ok(std::move(res));
}

Result<void, int> DBLRUCacheLayer::write(Index dbindex, const std::string& key, const std::string& value)
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
        const int dbid = static_cast<int>(dbindex);
        g_db_lru_cache.add(TransactableDBEntry(TransactableDBEntry::SingleKeyValue(dbid, key, value)));
        approxLRUCacheSize.fetch_add(value.size(), boost::memory_order_relaxed);
    }

    return Ok();
}

Result<void, int> DBLRUCacheLayer::erase(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    {
        const int dbid = static_cast<int>(dbindex);
        g_db_lru_cache.add(TransactableDBEntry(TransactableDBEntry::EraseKeyValue(dbid, key)));
    }

    return Ok();
}

Result<void, int> DBLRUCacheLayer::eraseAll(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key) ? Result<void, int>(Ok()) : Err(1);
    }

    {
        const int dbid = static_cast<int>(dbindex);
        g_db_lru_cache.add(TransactableDBEntry(TransactableDBEntry::EraseKeyValue(dbid, key)));
    }

    return Ok();
}

Result<bool, int> DBLRUCacheLayer::exists(Index dbindex, const std::string& key) const
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
        const int                                                   dbid = static_cast<int>(dbindex);
        const boost::optional<DBLRUCacheStorage::StoredEntryResult> okv =
            g_db_lru_cache.get_one(dbid, key);

        if (okv) {
            const DBLRUCacheStorage::StoredEntryResult& kv = *okv;
            switch (kv.op) {
            case DBLRUCacheStorage::StoredOperationType::Erase:
                return Ok(false);
            case DBLRUCacheStorage::StoredOperationType::Write:
                // single values should NEVER be empty... so if it's empty, we refresh the cache to fix
                // the issue
                return Ok(true);
            }
        }

        LMDB persistedDB(dbdir_, false);

        // TODO: this may be optimized, no need to read, instead maybe we can use exists()
        Result<boost::optional<std::string>, int> rdVal = persistedDB.read(dbindex, key, 0, boost::none);
        if (rdVal.isOk()) {
            const boost::optional<std::string>& dVal = rdVal.UNWRAP();
            if (dVal) {
                return Ok(true);
            } else {
                return Ok(false);
            }
        } else {
            return Err(rdVal.UNWRAP_ERR());
        }
    }
}

void DBLRUCacheLayer::clearDBData()
{
    // clearCache(); // TODO
    g_db_lru_cache.clear();
    LMDB persistedDB(dbdir_, true);
}

Result<void, int> DBLRUCacheLayer::beginDBTransaction(std::size_t /*expectedDataSize*/)
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

Result<void, int> DBLRUCacheLayer::commitDBTransaction()
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

    auto&& dbEntry = TransactableDBEntry(std::move(movedTx));
    g_db_lru_cache.add(std::move(dbEntry));

    return result ? Result<void, int>(Ok()) : Err(-1);
}

bool DBLRUCacheLayer::abortDBTransaction()
{
    tx.reset();
    return true;
}

boost::optional<boost::filesystem::path> DBLRUCacheLayer::getDataDir() const { return *dbdir_; }

bool DBLRUCacheLayer::openDB(bool clearDataBeforeOpen)
{
    if (clearDataBeforeOpen) {
        // clearCache(); // TODO
        DBLRUCacheLayer::clearDBData();
        approxLRUCacheSize.store(0, boost::memory_order_seq_cst);
        lruFlushCount.store(0, boost::memory_order_seq_cst);
    }

    LMDB persistedDB(dbdir_, clearDataBeforeOpen);
    boost::atomic_thread_fence(boost::memory_order_seq_cst);

    return true;
}

void DBLRUCacheLayer::close()
{
    tx.reset();
    // flush(); // TODO
    LMDB persistedDB(dbdir_, false);
    persistedDB.close();
    boost::atomic_thread_fence(boost::memory_order_seq_cst);
}

// TODO
boost::optional<bool> DBLRUCacheLayer::flushOnPolicy() const { return boost::none; }

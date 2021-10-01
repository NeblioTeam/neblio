#include "dblrucachestorage.h"
#include "logging/logger.h"
#include <algorithm>
#include <boost/atomic.hpp>
#include <boost/exception/exception.hpp>
#include <boost/mpl/clear.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/thread/lock_types.hpp>
#include <cstddef>
#include <cstdint>
#include <curl/curl.h>
#include <deque>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>

DBLRUCacheStorage::DBLRUCacheStorage() {}

template <typename T>
boost::shared_ptr<typename std::decay<T>::type>
DBLRUCacheStorage::AppendToCache(DBLRUCacheStorage& cache, T&& entry)
{
    boost::unique_lock<DBLRUCacheStorage::MutexType> lg(cache.dataLock);
    auto ptr = boost::make_shared<typename std::decay<T>::type>(std::forward<T>(entry));
    cache.data.push_back(ptr);
    return ptr;
}

bool DBLRUCacheStorage::add(const TransactableDBEntry& entry)
{
    auto ptr = AppendToCache(*this, entry);
    assert(ptr);
    auto&& keys = ptr->getAllKeys();
    for (auto&& key : keys) {
        dataMap[key.first].apply(
            key.second, [&](MapType::BucketMapType& m, const std::string& k) { m[k].push_back(ptr); });
    }
    return true;
}

bool DBLRUCacheStorage::add(TransactableDBEntry&& entry)
{
    auto ptr = AppendToCache(*this, std::move(entry));
    assert(ptr);
    auto&& keys = ptr->getAllKeys();
    for (auto&& key : keys) {
        dataMap[key.first].apply(
            key.second, [&](MapType::BucketMapType& m, const std::string& k) { m[k].push_back(ptr); });
    }
    return true;
}

void DBLRUCacheStorage::clear()
{
    boost::unique_lock<DBLRUCacheStorage::MutexType> lg(dataLock);
    data.clear();
    for (auto&& map : dataMap) {
        map.clear();
    }
}

std::size_t DBLRUCacheStorage::size() const
{
    boost::shared_lock<DBLRUCacheStorage::MutexType> lg(dataLock);
    return data.size();
}

boost::shared_ptr<TransactableDBEntry> DBLRUCacheStorage::pop_internal()
{
    boost::unique_lock<DBLRUCacheStorage::MutexType> lg(dataLock);

    if (data.empty()) {
        return nullptr;
    }

    boost::shared_ptr<TransactableDBEntry> ptr = boost::atomic_load(&data.front());
    data.pop_front();

    return ptr;
}

boost::optional<std::vector<DBLRUCacheStorage::StoredEntryResult>> DBLRUCacheStorage::pop_one()
{
    boost::shared_ptr<TransactableDBEntry> queuePtr = pop_internal();

    if (!queuePtr) {
        return boost::none;
    }

    const TransactableDBEntry& entry = *queuePtr;

    std::vector<DBLRUCacheStorage::StoredEntryResult> result;

    switch (entry.getOp()) {
    case TransactableDBEntry::EntryOperation::Erase: {
        auto&&    kv         = boost::get<TransactableDBEntry::EraseKeyValue>(entry.getValue());
        const int storedDbid = kv.first;
        auto&&    storedKey  = kv.second;
        result.push_back(StoredEntryResult::MakeErase(storedDbid, storedKey));
        break;
    }
    case TransactableDBEntry::EntryOperation::Write: {
        auto&& kv         = boost::get<TransactableDBEntry::SingleKeyValue>(entry.getValue());
        auto&& storedDbid = std::get<0>(kv);
        auto&& storedKey  = std::get<1>(kv);
        auto&& storedVal  = std::get<2>(kv);
        result.push_back(StoredEntryResult::MakeWrite(storedDbid, storedKey, storedVal));
        break;
    }
    case TransactableDBEntry::EntryOperation::Transaction: {
        const TransactableDBEntry::TransactionValues& tx =
            boost::get<TransactableDBEntry::TransactionValues>(entry.getValue());
        auto&& allData = tx->getAllData();
        for (auto&& dbindexVsMap : allData) {
            auto&&    dbindex = dbindexVsMap.first;
            const int dbid    = static_cast<int>(dbindex);
            auto&&    dbData  = dbindexVsMap.second;
            for (auto&& kvOp : dbData) {
                auto&& storedKey = kvOp.first;
                auto&& storedOp  = kvOp.second;
                switch (storedOp.getOpType()) {
                case DBOperation::WriteOperationType::Append:
                    for (const auto& val : boost::adaptors::reverse(storedOp.getValues())) {
                        result.push_back(
                            DBLRUCacheStorage::StoredEntryResult::MakeWrite(dbid, storedKey, val));
                    }
                    break;
                case DBOperation::WriteOperationType::UniqueSet:
                    if (!storedOp.getValues().empty()) {
                        result.push_back(DBLRUCacheStorage::StoredEntryResult::MakeWrite(
                            dbid, storedKey, storedOp.getValues().front()));
                    }
                    break;
                case DBOperation::WriteOperationType::Erase:
                    result.push_back(StoredEntryResult::MakeErase(dbid, storedKey));
                    break;
                }
            }
        }
        break;
    }
    }

    auto&& allKeys = queuePtr->getAllKeys();
    for (auto&& key : allKeys) {
        dataMap[key.first].apply(key.second, [&](MapType::BucketMapType& m, const std::string& k) {
            auto it = m.find(k);
            if (it == m.end()) {
                NLog.write(b_sev::critical,
                           "No keys were registered in the LRU cache storage for key {} in dbid {}", k,
                           key.first);
                return;
            }
            auto&& entries = it->second;
            while (entries.size() > 0) {
                // since entries are inserted in order, we expect them from the front to all point to the
                // data we pushed to the queue
                auto&& entryInMap = entries.front();
                if (entryInMap.lock().get() == queuePtr.get()) {
                    entries.erase(entries.begin());
                } else {
                    break;
                }
            }
            // no more items left in that key, why keep it?
            if (entries.empty()) {
                m.erase(it);
            }
        });
    }

    return result;
}

std::vector<DBLRUCacheStorage::StoredEntryResult> ExtractValues(const TransactableDBEntry& entry,
                                                                const int dbid, const std::string& key)
{
    std::vector<DBLRUCacheStorage::StoredEntryResult> result;
    switch (entry.getOp()) {
    case TransactableDBEntry::EntryOperation::Erase: {
        const std::pair<int, std::string>& store =
            boost::get<TransactableDBEntry::EraseKeyValue>(entry.getValue());
        if (store.first == dbid && store.second == key) {
            // when meeting erase, we just shortcut and exit since all following values are erased
            result.push_back(DBLRUCacheStorage::StoredEntryResult::MakeErase(store.first, store.second));
            return result;
        }
        break;
    }
    case TransactableDBEntry::EntryOperation::Write: {
        const TransactableDBEntry::SingleKeyValue& store =
            boost::get<TransactableDBEntry::SingleKeyValue>(entry.getValue());
        const int          storedDbid = std::get<0>(store);
        const std::string& storedKey  = std::get<1>(store);
        const std::string& storedVal  = std::get<2>(store);
        if (storedDbid == dbid && storedKey == key) {
            result.push_back(
                DBLRUCacheStorage::StoredEntryResult::MakeWrite(storedDbid, storedKey, storedVal));
        }
        break;
    }
    case TransactableDBEntry::EntryOperation::Transaction: {
        const TransactableDBEntry::TransactionValues& tx =
            boost::get<TransactableDBEntry::TransactionValues>(entry.getValue());
        boost::optional<TransactionOperation> storedOp = tx->getOp(dbid, key);
        if (storedOp) {
            switch (storedOp->getOpType()) {
            case DBOperation::WriteOperationType::Append:
                for (const auto& val : boost::adaptors::reverse(storedOp->getValues())) {
                    result.push_back(DBLRUCacheStorage::StoredEntryResult::MakeWrite(dbid, key, val));
                }
                break;
            case DBOperation::WriteOperationType::UniqueSet:
                if (!storedOp->getValues().empty()) {
                    result.push_back(DBLRUCacheStorage::StoredEntryResult::MakeWrite(
                        dbid, key, storedOp->getValues().front()));
                }
                break;
            case DBOperation::WriteOperationType::Erase:
                // when meeting erase, we just shortcut and exit since all following values are erased
                result.push_back(DBLRUCacheStorage::StoredEntryResult::MakeErase(dbid, key));
                return result;
                break;
            }
        }
        break;
    }
    }
    return result;
}

boost::optional<DBLRUCacheStorage::StoredEntryResult>
ExtractSingleValue(const TransactableDBEntry& entry, const int dbid, const std::string& key)
{
    switch (entry.getOp()) {
    case TransactableDBEntry::EntryOperation::Erase: {
        const TransactableDBEntry::EraseKeyValue& store =
            boost::get<TransactableDBEntry::EraseKeyValue>(entry.getValue());
        if (store.first == dbid && store.second == key) {
            return boost::make_optional(
                DBLRUCacheStorage::StoredEntryResult::MakeErase(store.first, store.second));
        }
        break;
    }
    case TransactableDBEntry::EntryOperation::Write: {
        const TransactableDBEntry::SingleKeyValue& store =
            boost::get<TransactableDBEntry::SingleKeyValue>(entry.getValue());
        const int          storedDbid = std::get<0>(store);
        const std::string& storedKey  = std::get<1>(store);
        const std::string& storedVal  = std::get<2>(store);
        if (storedDbid == dbid && storedKey == key) {
            return boost::make_optional(
                DBLRUCacheStorage::StoredEntryResult::MakeWrite(storedDbid, storedKey, storedVal));
        }
        break;
    }
    case TransactableDBEntry::EntryOperation::Transaction: {
        const TransactableDBEntry::TransactionValues& tx =
            boost::get<TransactableDBEntry::TransactionValues>(entry.getValue());
        boost::optional<TransactionOperation> storedOp = tx->getOp(dbid, key);
        if (storedOp) {
            switch (storedOp->getOpType()) {
            case DBOperation::WriteOperationType::Append:
                for (const auto& val : storedOp->getValues()) {
                    return DBLRUCacheStorage::StoredEntryResult::MakeWrite(dbid, key, val);
                }
                break;
            case DBOperation::WriteOperationType::UniqueSet:
                if (!storedOp->getValues().empty()) {
                    return DBLRUCacheStorage::StoredEntryResult::MakeWrite(
                        dbid, key, storedOp->getValues().front());
                }
                break;
            case DBOperation::WriteOperationType::Erase:
                break;
            }
        }
        break;
    }
    }

    return boost::none;
}

std::vector<DBLRUCacheStorage::StoredEntryResult> DBLRUCacheStorage::get(const int          dbid,
                                                                         const std::string& key) const
{
    std::vector<StoredEntryResult> result;

    boost::optional<std::deque<boost::weak_ptr<TransactableDBEntry>>> els = dataMap[dbid].get(key);
    if (!els) {
        return {};
    }

    for (const auto& entryWeakPtr : boost::adaptors::reverse(*els)) {
        auto entryPtr = entryWeakPtr.lock();
        if (!entryPtr) {
            break;
        }
        std::vector<StoredEntryResult> subResult = ExtractValues(*entryPtr, dbid, key);
        result.insert(result.end(), std::make_move_iterator(subResult.begin()),
                      std::make_move_iterator(subResult.end()));
        if (!result.empty() && result.back().op == DBLRUCacheStorage::StoredOperationType::Erase) {
            // the last operation is erase, no point in continuing. Everything before that point is void
            // remember, this is because the loop is reversed
            break;
        }
    }
    std::reverse(result.begin(), result.end());
    return result;
}

boost::optional<DBLRUCacheStorage::StoredEntryResult>
DBLRUCacheStorage::get_one(int dbid, const std::string& key) const
{
    boost::shared_ptr<TransactableDBEntry> el =
        dataMap[dbid].produce(key,
                              [&](const MapType::BucketMapType& m,
                                  const std::string& k) -> boost::shared_ptr<TransactableDBEntry> {
                                  auto it = m.find(k);
                                  if (it != m.cend()) {
                                      auto vec = it->second;
                                      if (!vec.empty()) {
                                          return vec.back().lock();
                                      }
                                  }
                                  return boost::shared_ptr<TransactableDBEntry>();
                              });
    if (!el) {
        // if everything is being popped in order, that means that all the elements before this one don't
        // exist if this one was already popped
        return boost::none;
    }

    return ExtractSingleValue(*el, dbid, key);
}

std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>>
DBLRUCacheStorage::getAll(int dbid) const
{
    // since we loop in reverse, we keep track of erased items so that we stop appending them
    // unnecessarily, since an erase means that everything before that point doesn't count
    std::set<std::string> erased;

    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>> result;

    // avoid locking for a long time... take a copy and unlock
    const std::deque<boost::shared_ptr<TransactableDBEntry>> dataCopy = [this]() {
        boost::shared_lock<DBLRUCacheStorage::MutexType> lg(dataLock);
        return data;
    }();

    for (const boost::shared_ptr<TransactableDBEntry>& entryPtr : boost::adaptors::reverse(dataCopy)) {
        assert(entryPtr);
        const TransactableDBEntry& entry = *entryPtr;
        switch (entry.getOp()) {
        case TransactableDBEntry::EntryOperation::Erase: {
            auto&&    kv         = boost::get<TransactableDBEntry::EraseKeyValue>(entry.getValue());
            const int storedDbid = kv.first;
            auto&&    storedKey  = kv.second;
            if (storedDbid != dbid) {
                continue;
            }
            // if this is already erased, ignore it
            if (erased.count(storedKey)) {
                continue;
            }
            result[storedKey].push_back(StoredEntryResult::MakeErase(dbid, storedKey));
            erased.insert(storedKey);
            break;
        }
        case TransactableDBEntry::EntryOperation::Write: {
            auto&& kv         = boost::get<TransactableDBEntry::SingleKeyValue>(entry.getValue());
            auto&& storedDbid = std::get<0>(kv);
            auto&& storedKey  = std::get<1>(kv);
            auto&& storedVal  = std::get<2>(kv);
            if (storedDbid != dbid) {
                continue;
            }
            // if this is already erased, ignore it
            if (erased.count(storedKey)) {
                continue;
            }
            result[storedKey].push_back(StoredEntryResult::MakeWrite(dbid, storedKey, storedVal));
            break;
        }
        case TransactableDBEntry::EntryOperation::Transaction: {
            const TransactableDBEntry::TransactionValues& tx =
                boost::get<TransactableDBEntry::TransactionValues>(entry.getValue());
            auto&& allData = tx->getAllDataForDB(dbid);
            for (auto&& kvOp : allData) {
                auto&& storedKey = kvOp.first;
                auto&& storedOp  = kvOp.second;
                switch (storedOp.getOpType()) {
                case DBOperation::WriteOperationType::Append:
                    if (erased.count(storedKey)) {
                        // if this is erased, ignore anything that comes after it
                        continue;
                    }
                    for (const auto& val : boost::adaptors::reverse(storedOp.getValues())) {
                        result[storedKey].push_back(
                            DBLRUCacheStorage::StoredEntryResult::MakeWrite(dbid, storedKey, val));
                    }
                    break;
                case DBOperation::WriteOperationType::UniqueSet:
                    if (erased.count(storedKey)) {
                        // if this is erased, ignore anything that comes after it
                        continue;
                    }
                    if (!storedOp.getValues().empty()) {
                        result[storedKey].push_back(DBLRUCacheStorage::StoredEntryResult::MakeWrite(
                            dbid, storedKey, storedOp.getValues().front()));
                    }
                    break;
                case DBOperation::WriteOperationType::Erase:
                    if (erased.count(storedKey)) {
                        // if this is already erased, ignore it
                        continue;
                    }
                    result[storedKey].push_back(StoredEntryResult::MakeErase(dbid, storedKey));
                    erased.insert(storedKey);
                    break;
                }
            }
            break;
        }
        }
    }

    // reverse since we started in reverse order
    for (auto&& entry : result) {
        std::reverse(entry.second.begin(), entry.second.end());
    }

    return result;
}

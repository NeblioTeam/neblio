#include "dblrucachestorage.h"
#include <algorithm>
#include <boost/mpl/clear.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <set>

DBLRUCacheStorage::DBLRUCacheStorage() {}

void DBLRUCacheStorage::add(const TransactableDBEntry& entry)
{
    // auto&& keys = entry.getAllKeys();
    // for (auto&& key : keys) {
    //     dataMap[key.first][key.second].push_back(data.size());
    // }
    data.push_back(entry);
}

void DBLRUCacheStorage::add(TransactableDBEntry&& entry)
{
    // auto&& keys = entry.getAllKeys();
    // for (auto&& key : keys) {
    //     dataMap[key.first][key.second].push_back(data.size());
    // }
    data.push_back(std::move(entry));
}

void DBLRUCacheStorage::clear() { data.clear(); }

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
    for (const TransactableDBEntry& entry : boost::adaptors::reverse(data)) {
        std::vector<StoredEntryResult> subResult = ExtractValues(entry, dbid, key);
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
    for (const TransactableDBEntry& entry : boost::adaptors::reverse(data)) {
        boost::optional<StoredEntryResult> subResult = ExtractSingleValue(entry, dbid, key);
        if (subResult) {
            return subResult;
        }
    }
    return boost::none;
}

std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>>
DBLRUCacheStorage::getAll(int dbid) const
{
    // since we loop in reverse, we keep track of erased items so that we stop appending them
    // unnecessarily, since an erase means that everything before that point doesn't count
    std::set<std::string> erased;

    std::map<std::string, std::vector<DBLRUCacheStorage::StoredEntryResult>> result;

    for (const TransactableDBEntry& entry : boost::adaptors::reverse(data)) {
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

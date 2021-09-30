#ifndef DBLRUCACHESTORAGE_H
#define DBLRUCACHESTORAGE_H

#include "concurrentmap/ConcurrentMap.h"
#include "db/idb.h"
#include "transactabledbentry.h"
#include <array>
#include <boost/atomic/atomic.hpp>
#include <boost/exception/exception.hpp>
#include <boost/lockfree/policies.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>

class DBLRUCacheStorage
{
public:
    enum StoredOperationType
    {
        Write,
        Erase
    };

    struct StoredEntryResult
    {
        StoredOperationType op;
        int                 dbid;
        std::string         key;
        std::string         value;

        template <typename T>
        static StoredEntryResult MakeErase(int dbid_, T&& key_)
        {
            StoredEntryResult res;
            res.dbid = dbid_;
            res.key  = std::forward<T>(key_);
            res.op   = StoredOperationType::Erase;
            return res;
        }

        template <typename T, typename U>
        static StoredEntryResult MakeWrite(int dbid_, T&& key_, U&& value_)
        {
            StoredEntryResult res;
            res.dbid  = dbid_;
            res.key   = std::forward<T>(key_);
            res.value = std::forward<U>(value_);
            res.op    = StoredOperationType::Write;
            return res;
        }
    };

    using MapType = ConcurrentMap<std::string, std::deque<boost::weak_ptr<TransactableDBEntry>>, 5000>;

    static const std::size_t QueueCapacity = 10000;

    using MutexType = boost::shared_mutex;

private:
    mutable MutexType dataLock;

    std::deque<boost::shared_ptr<TransactableDBEntry>> data = {};

    std::array<MapType, static_cast<int>(IDB::Index::Index_Last)> dataMap;

    template <typename T>
    static boost::shared_ptr<typename std::decay<T>::type> AppendToCache(DBLRUCacheStorage& cache,
                                                                         T&&                entry);

    boost::shared_ptr<TransactableDBEntry> pop_internal();

public:
    DBLRUCacheStorage();
    bool add(const TransactableDBEntry& entry);
    bool add(TransactableDBEntry&& entry);

    void        clear();
    std::size_t size() const;

    boost::optional<std::vector<StoredEntryResult>> pop_one();

    [[nodiscard]] std::vector<StoredEntryResult>     get(int dbid, const std::string& key) const;
    [[nodiscard]] boost::optional<StoredEntryResult> get_one(int dbid, const std::string& key) const;
    [[nodiscard]] std::map<std::string, std::vector<StoredEntryResult>> getAll(int dbid) const;
};

#endif // DBLRUCACHESTORAGE_H

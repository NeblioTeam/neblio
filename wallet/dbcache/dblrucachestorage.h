#ifndef DBLRUCACHESTORAGE_H
#define DBLRUCACHESTORAGE_H

#include "db/idb.h"
#include "transactabledbentry.h"
#include <array>
#include <cstddef>
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

private:
    std::deque<TransactableDBEntry> data;
    // std::array<std::map<std::string, std::vector<std::size_t>>,
    // static_cast<int>(IDB::Index::Index_Last)>
    //             dataMap;
    // std::size_t popCount = 0;

public:
    DBLRUCacheStorage();
    void add(const TransactableDBEntry& entry);
    void add(TransactableDBEntry&& entry);

    void clear();

    [[nodiscard]] std::vector<StoredEntryResult>     get(int dbid, const std::string& key) const;
    [[nodiscard]] boost::optional<StoredEntryResult> get_one(int dbid, const std::string& key) const;
    [[nodiscard]] std::map<std::string, std::vector<StoredEntryResult>> getAll(int dbid) const;
};

#endif // DBLRUCACHESTORAGE_H

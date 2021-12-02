#ifndef IDB_H
#define IDB_H

#include "result.h"
#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>
#include <map>
#include <vector>

class IDB
{
public:
    enum class Index : int
    {
        DB_MAIN_INDEX           = 0,
        DB_BLOCKINDEX_INDEX     = 1,
        DB_BLOCKS_INDEX         = 2,
        DB_TX_INDEX             = 3,
        DB_NTP1TX_INDEX         = 4,
        DB_NTP1TOKENNAMES_INDEX = 5,
        DB_ADDRSVSPUBKEYS_INDEX = 6,
        DB_BLOCKMETADATA_INDEX  = 7,
        DB_BLOCKHEIGHTS_INDEX   = 8,
        DB_STAKES_INDEX         = 9,

        Index_Last = 10
    };

    static bool DuplicateKeysAllowed(Index idx) { return idx == Index::DB_NTP1TOKENNAMES_INDEX; }

    virtual Result<boost::optional<std::string>, int>
    read(IDB::Index dbindex, const std::string& key, std::size_t offset = 0,
         const boost::optional<std::size_t>& size = boost::none) const = 0;

    /**
     * @brief ReadMultiple reads all the elements under the given key
     * @param dbindex
     * @param key
     */
    virtual Result<std::vector<std::string>, int> readMultiple(IDB::Index         dbindex,
                                                               const std::string& key) const = 0;

    /**
     * @brief ReadAll returns all the items in the database in a map
     * @param dbindex
     */
    virtual Result<std::map<std::string, std::vector<std::string>>, int>
    readAll(IDB::Index dbindex) const = 0;

    /**
     * @brief readAllUnique returns all items, just like readAll, but assumes key/value pairs are unique
     */
    virtual Result<std::map<std::string, std::string>, int> readAllUnique(IDB::Index dbindex) const = 0;

    virtual Result<void, int> write(IDB::Index dbindex, const std::string& key,
                                    const std::string& value) = 0;

    /**
     * @brief Erase erases a single entry under key (use it for DBs that don't support duplicates)
     * @param dbindex
     * @param key
     * @return true if the key doesn't exist or was deleted, false otherwise (db access failed, etc)
     */
    virtual Result<void, int> erase(IDB::Index dbindex, const std::string& key) = 0;

    /**
     * @brief EraseAll erases all the entries under key (use it for DBs that support duplicates)
     * @param dbindex
     * @param key
     * @return true if the key doesn't exist or was successfully fully deleted, false otherwise (db
     * access failed, etc)
     */
    virtual Result<void, int> eraseAll(IDB::Index dbindex, const std::string& key) = 0;

    virtual Result<bool, int> exists(IDB::Index dbindex, const std::string& key) const = 0;

    virtual void clearDBData() = 0;

    /**
     * Begin a transaction. Transactions are NOT thread safe. Once a transaction is opened, do not use
     * the object in other threads until it's committed or aborted.
     */
    virtual Result<void, int> beginDBTransaction(std::size_t expectedDataSize = 0) = 0;

    virtual Result<void, int> commitDBTransaction() = 0;

    virtual bool abortDBTransaction() = 0;

    virtual boost::optional<boost::filesystem::path> getDataDir() const = 0;

    virtual bool openDB(bool clearDataBeforeOpen) = 0;

    /**
     * @brief Close shutdowns the database (to be used before closing the program)
     */
    virtual void close() = 0;

    virtual ~IDB() {}
};

#endif // IDB_H

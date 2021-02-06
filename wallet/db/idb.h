#ifndef IDB_H
#define IDB_H

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
        DB_ADDRSVSPUBKEYS_INDEX = 6
    };

    virtual boost::optional<std::string>
    read(IDB::Index dbindex, const std::string& key, std::size_t offset = 0,
         const boost::optional<std::size_t>& size = boost::none) const = 0;

    /**
     * @brief ReadMultiple reads all the elements under the given key
     * @param dbindex
     * @param key
     * @return all elements that are under the given key
     */
    virtual boost::optional<std::vector<std::string>> readMultiple(IDB::Index         dbindex,
                                                                   const std::string& key) const = 0;

    /**
     * @brief ReadAll returns all the items in the database in a map
     * @param dbindex
     * @return
     */
    virtual std::map<std::string, std::vector<std::string>> readAll(IDB::Index dbindex) const = 0;

    virtual bool write(IDB::Index dbindex, const std::string& key, const std::string& value) = 0;

    /**
     * @brief Erase erases a single entry under key (use it for DBs that don't support duplicates)
     * @param dbindex
     * @param key
     * @return true if the key doesn't exist or was deleted, false otherwise (db access failed, etc)
     */
    virtual bool erase(IDB::Index dbindex, const std::string& key) = 0;

    /**
     * @brief EraseAll erases all the entries under key (use it for DBs that support duplicates)
     * @param dbindex
     * @param key
     * @return true if the key doesn't exist or was successfully fully deleted, false otherwise (db
     * access failed, etc)
     */
    virtual bool eraseAll(IDB::Index dbindex, const std::string& key) = 0;

    virtual bool exists(IDB::Index dbindex, const std::string& key) const = 0;

    virtual bool beginDBTransaction(std::size_t expectedDataSize = 0) = 0;

    virtual bool commitDBTransaction() = 0;

    virtual bool abortDBTransaction() = 0;

    /**
     * @brief Close shutdowns the database (to be used before closing the program)
     */
    virtual void close() = 0;
};

#endif // IDB_H

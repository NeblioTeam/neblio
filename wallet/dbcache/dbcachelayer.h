#ifndef DBCACHELAYER_H
#define DBCACHELAYER_H

#include "db/idb.h"
#include "hierarchicaldb.h"
#include "inmemorydb.h"

class LMDB;

class DBCacheLayer : public IDB
{
    std::unique_ptr<HierarchicalDB<hdb_dummy_mutex>> tx;
    const int64_t                                    flushOnSizeReached;
    const boost::filesystem::path* const             dbdir_;

public:
    /**
     * @brief DBCacheLayer
     * @param dbdir
     * @param startNewDatabase
     * @param flushOnSize if 0, it's only in memory cache with no flushing
     */
    DBCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase, int64_t flushOnSize);

    boost::optional<std::string> read(Index dbindex, const std::string& key, std::size_t offset,
                                      const boost::optional<std::size_t>& size) const override;
    boost::optional<std::vector<std::string>> readMultiple(Index              dbindex,
                                                           const std::string& key) const override;
    boost::optional<std::map<std::string, std::vector<std::string>>>
                                                        readAll(Index dbindex) const override;
    boost::optional<std::map<std::string, std::string>> readAllUnique(Index dbindex) const override;
    bool write(Index dbindex, const std::string& key, const std::string& value) override;
    bool erase(Index dbindex, const std::string& key) override;
    bool eraseAll(Index dbindex, const std::string& key) override;
    bool exists(Index dbindex, const std::string& key) const override;
    void clearDBData() override;
    bool beginDBTransaction(std::size_t /*expectedDataSize*/) override;
    bool commitDBTransaction() override;
    bool abortDBTransaction() override;
    boost::optional<boost::filesystem::path> getDataDir() const override;
    bool                                     openDB(bool clearDataBeforeOpen) override;
    void                                     close() override;

    bool                  flush() const;
    boost::optional<bool> flushOnPolicy() const;
    void                  clearCache();
    void                  clearCache_unsafe() const;
    static uint64_t       GetFlushCount();
};

#endif // DBCACHELAYER_H

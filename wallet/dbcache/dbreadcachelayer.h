#ifndef DBREADCACHELAYER_H
#define DBREADCACHELAYER_H

#include "db/idb.h"
#include "hierarchicaldb.h"
#include "inmemorydb.h"

class LMDB;

class DBReadCacheLayer : public IDB
{
    std::unique_ptr<HierarchicalDB<hdb_dummy_mutex>> tx;
    const int64_t                                    flushOnSizeReached;
    const boost::filesystem::path* const             dbdir_;

public:
    /**
     * @brief DBReadCacheLayer
     * @param dbdir
     * @param startNewDatabase
     * @param flushOnSize if 0, it's only in memory cache with no flushing
     */
    DBReadCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase,
                     int64_t flushOnSize);

    Result<boost::optional<std::string>, int>
                                          read(Index dbindex, const std::string& key, std::size_t offset,
                                               const boost::optional<std::size_t>& size) const override;
    Result<std::vector<std::string>, int> readMultiple(Index              dbindex,
                                                       const std::string& key) const override;
    Result<std::map<std::string, std::vector<std::string>>, int> readAll(Index dbindex) const override;
    Result<std::map<std::string, std::string>, int> readAllUnique(Index dbindex) const override;
    Result<void, int> write(Index dbindex, const std::string& key, const std::string& value) override;
    Result<void, int> erase(Index dbindex, const std::string& key) override;
    Result<void, int> eraseAll(Index dbindex, const std::string& key) override;
    Result<bool, int> exists(Index dbindex, const std::string& key) const override;
    void              clearDBData() override;
    Result<void, int> beginDBTransaction(std::size_t /*expectedDataSize*/) override;
    Result<void, int> commitDBTransaction() override;
    bool              abortDBTransaction() override;
    boost::optional<boost::filesystem::path> getDataDir() const override;
    bool                                     openDB(bool clearDataBeforeOpen) override;
    void                                     close() override;

    bool                  flush(const boost::optional<uint64_t>& commitSizeIn = boost::none) const;
    boost::optional<bool> flushOnPolicy() const;
    void                  clearCache() const;
    void                  clearCache_unsafe() const;
    static uint64_t       GetFlushCount();
};

#endif // DBREADCACHELAYER_H

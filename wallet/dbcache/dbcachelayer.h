#ifndef DBCACHELAYER_H
#define DBCACHELAYER_H

#include "db/idb.h"
#include "hierarchicaldb.h"
#include "inmemorydb.h"

extern std::unique_ptr<IDB> g_cached_db_instance;

class DBCacheLayer : public IDB
{
    std::unique_ptr<HierarchicalDB<hdb_dummy_mutex>> tx;

public:
    DBCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase);

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

    bool flush();
    void clearCache();
};

#endif // DBCACHELAYER_H

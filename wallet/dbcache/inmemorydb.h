#ifndef INMEMORYDB_H
#define INMEMORYDB_H

#include "../db/idb.h"
#include "hierarchicaldb.h"

#include <mutex>

class InMemoryDB : public IDB
{
public:
    using MutexType = std::mutex;

private:
    static std::array<std::map<std::string, std::vector<std::string>>,
                      static_cast<std::size_t>(IDB::Index::Index_Last)>
                     data;
    static MutexType mtx;

    std::unique_ptr<HierarchicalDB<hdb_dummy_mutex>> tx;

public:
    InMemoryDB(const boost::filesystem::path* const /*dbdir*/, bool startNewDatabase);
    InMemoryDB(bool startNewDatabase);

    boost::optional<std::string> read(Index dbindex, const std::string& key, std::size_t offset,
                                      const boost::optional<std::size_t>& size) const override;
    boost::optional<std::vector<std::string>> readMultiple(Index              dbindex,
                                                           const std::string& key) const override;
    boost::optional<std::map<std::string, std::vector<std::string>>>
                                                        readAll(Index dbindex) const override;
    boost::optional<std::map<std::string, std::string>> readAllUnique(Index dbindex) const override;
    bool write_in_tx(Index dbindex, const std::string& key, const std::string& value);
    bool write_unsafe(Index dbindex, const std::string& key, const std::string& value);
    bool write(Index dbindex, const std::string& key, const std::string& value) override;
    bool erase_unsafe(Index dbindex, const std::string& key);
    bool erase_in_tx(Index dbindex, const std::string& key);
    bool erase(Index dbindex, const std::string& key) override;
    bool eraseAll(Index dbindex, const std::string& key) override;
    bool exists(Index dbindex, const std::string& key) const override;
    void clearDBData() override;
    bool beginDBTransaction(std::size_t expectedDataSize) override;
    bool commitDBTransaction() override;
    bool abortDBTransaction() override;
    boost::optional<boost::filesystem::path> getDataDir() const override;
    bool                                     openDB(bool clearDataBeforeOpen) override;
    void                                     close() override;
};

#endif // INMEMORYDB_H

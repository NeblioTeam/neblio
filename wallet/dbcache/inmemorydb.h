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

    Result<boost::optional<std::string>, int>
                                          read(Index dbindex, const std::string& key, std::size_t offset,
                                               const boost::optional<std::size_t>& size) const override;
    Result<std::vector<std::string>, int> readMultiple(Index              dbindex,
                                                       const std::string& key) const override;
    Result<std::map<std::string, std::vector<std::string>>, int> readAll(Index dbindex) const override;
    Result<std::map<std::string, std::string>, int> readAllUnique(Index dbindex) const override;
    bool              write_in_tx(Index dbindex, const std::string& key, const std::string& value);
    Result<void, int> write_unsafe(Index dbindex, const std::string& key, const std::string& value);
    Result<void, int> write(Index dbindex, const std::string& key, const std::string& value) override;
    Result<void, int> erase_unsafe(Index dbindex, const std::string& key);
    bool              erase_in_tx(Index dbindex, const std::string& key);
    Result<void, int> erase(Index dbindex, const std::string& key) override;
    Result<void, int> eraseAll(Index dbindex, const std::string& key) override;
    Result<bool, int> exists(Index dbindex, const std::string& key) const override;
    void              clearDBData() override;
    Result<void, int> beginDBTransaction(std::size_t expectedDataSize) override;
    Result<void, int> commitDBTransaction() override;
    bool              abortDBTransaction() override;
    boost::optional<boost::filesystem::path> getDataDir() const override;
    bool                                     openDB(bool clearDataBeforeOpen) override;
    void                                     close() override;
};

#endif // INMEMORYDB_H

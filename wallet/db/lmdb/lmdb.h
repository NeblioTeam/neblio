#ifndef LMDB_H
#define LMDB_H

#include "../idb.h"

#include "liblmdb/lmdb.h"
#include "lmdbtransaction.h"
#include <memory>

// global environment pointer
extern std::unique_ptr<MDB_env, void (*)(MDB_env*)> dbEnv;
// global database pointers
using DbSmartPtrType = std::unique_ptr<MDB_dbi, void (*)(MDB_dbi*)>;

struct __lmdb_db_pointers
{
    DbSmartPtrType db_main;
    DbSmartPtrType db_blockIndex;
    DbSmartPtrType db_blocks;
    DbSmartPtrType db_tx;
    DbSmartPtrType db_ntp1Tx;
    DbSmartPtrType db_ntp1tokenNames;
    DbSmartPtrType db_addrsVsPubKeys;
    DbSmartPtrType db_blockMetadata;
    DbSmartPtrType db_blockHeights;
    DbSmartPtrType db_stakes;

    __lmdb_db_pointers()
        : db_main(nullptr, [](MDB_dbi*) {}), db_blockIndex(nullptr, [](MDB_dbi*) {}),
          db_blocks(nullptr, [](MDB_dbi*) {}), db_tx(nullptr, [](MDB_dbi*) {}),
          db_ntp1Tx(nullptr, [](MDB_dbi*) {}), db_ntp1tokenNames(nullptr, [](MDB_dbi*) {}),
          db_addrsVsPubKeys(nullptr, [](MDB_dbi*) {}), db_blockMetadata(nullptr, [](MDB_dbi*) {}),
          db_blockHeights(nullptr, [](MDB_dbi*) {}), db_stakes(nullptr, [](MDB_dbi*) {})
    {
    }

    ~__lmdb_db_pointers()
    {
        db_main.reset();
        db_blockIndex.reset();
        db_blocks.reset();
        db_tx.reset();
        db_ntp1Tx.reset();
        db_ntp1tokenNames.reset();
        db_addrsVsPubKeys.reset();
        db_blockMetadata.reset();
        db_blockHeights.reset();
        db_stakes.reset();
    }
};

extern std::unique_ptr<__lmdb_db_pointers> glob_lmdb_db_pointers;

namespace boost {
namespace filesystem {
class path;
}
} // namespace boost

class LMDB : public IDB
{
    const boost::filesystem::path* const dbdir_;

    __lmdb_db_pointers* dbPointers = nullptr;

    void loadDbPointers() { dbPointers = glob_lmdb_db_pointers.get(); }

    void resetDbPointers() { dbPointers = nullptr; }

    void openDatabase(const boost::filesystem::path& directory, bool clearDBBeforeOpen);

    void doResize(uint64_t increase_size = 0);

    MDB_dbi* getDbByIndex(const Index index) const;

    // A batch stores up writes and deletes for atomic application. When this
    // field is non-NULL, writes/deletes go there instead of directly to disk.
    std::unique_ptr<LMDBTransaction> activeBatch;

public:
    LMDB(const boost::filesystem::path* const dbdir, bool startNewDatabase);

    void clearDBData() override;

    boost::optional<std::string> read(IDB::Index dbindex, const std::string& key, std::size_t offset,
                                      const boost::optional<std::size_t>& size) const override;
    boost::optional<std::vector<std::string>> readMultiple(IDB::Index         dbindex,
                                                           const std::string& key) const override;
    boost::optional<std::map<std::string, std::vector<std::string>>>
                                                        readAll(IDB::Index dbindex) const override;
    boost::optional<std::map<std::string, std::string>> readAllUnique(IDB::Index dbindex) const override;
    bool write(IDB::Index dbindex, const std::string& key, const std::string& value) override;
    bool erase(IDB::Index dbindex, const std::string& key) override;
    bool eraseAll(IDB::Index dbindex, const std::string& key) override;
    bool exists(IDB::Index dbindex, const std::string& key) const override;
    bool beginDBTransaction(std::size_t expectedDataSize) override;
    bool commitDBTransaction() override;
    bool abortDBTransaction() override;
    bool openDB(bool clearDataBeforeOpen) override;

    boost::optional<boost::filesystem::path> getDataDir() const override;

    void close() override;
};

#endif // LMDB_H

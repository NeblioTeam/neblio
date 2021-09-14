#include "dbcachelayer.h"

#include "db/lmdb/lmdb.h"
#include <boost/atomic.hpp>

std::unique_ptr<IDB> g_cached_db_instance;

DBCacheLayer::DBCacheLayer(const boost::filesystem::path* const dbdir, bool startNewDatabase)
{
    if (!g_cached_db_instance) {
        g_cached_db_instance = std::unique_ptr<IDB>(new LMDB(dbdir, startNewDatabase));
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
    }
}

boost::optional<std::string> DBCacheLayer::read(Index dbindex, const std::string& key,
                                                std::size_t                         offset,
                                                const boost::optional<std::size_t>& size) const
{
    return g_cached_db_instance->read(dbindex, key, offset, size);
}

boost::optional<std::vector<std::string>> DBCacheLayer::readMultiple(Index              dbindex,
                                                                     const std::string& key) const
{
    return g_cached_db_instance->readMultiple(dbindex, key);
}

boost::optional<std::map<std::string, std::vector<std::string>>>
DBCacheLayer::readAll(Index dbindex) const
{
    return g_cached_db_instance->readAll(dbindex);
}

boost::optional<std::map<std::string, std::string>> DBCacheLayer::readAllUnique(Index dbindex) const
{
    return g_cached_db_instance->readAllUnique(dbindex);
}

bool DBCacheLayer::write(Index dbindex, const std::string& key, const std::string& value)
{
    return g_cached_db_instance->write(dbindex, key, value);
}

bool DBCacheLayer::erase(Index dbindex, const std::string& key)
{
    return g_cached_db_instance->erase(dbindex, key);
}

bool DBCacheLayer::eraseAll(Index dbindex, const std::string& key)
{
    return g_cached_db_instance->eraseAll(dbindex, key);
}

bool DBCacheLayer::exists(Index dbindex, const std::string& key) const
{
    return g_cached_db_instance->exists(dbindex, key);
}

void DBCacheLayer::clearDBData() { g_cached_db_instance->clearDBData(); }

bool DBCacheLayer::beginDBTransaction(std::size_t expectedDataSize)
{
    return g_cached_db_instance->beginDBTransaction(expectedDataSize);
}

bool DBCacheLayer::commitDBTransaction() { return g_cached_db_instance->commitDBTransaction(); }

bool DBCacheLayer::abortDBTransaction() { return g_cached_db_instance->abortDBTransaction(); }

boost::optional<boost::filesystem::path> DBCacheLayer::getDataDir() const
{
    return g_cached_db_instance->getDataDir();
}

bool DBCacheLayer::openDB(bool clearDataBeforeOpen)
{
    return g_cached_db_instance->openDB(clearDataBeforeOpen);
}

void DBCacheLayer::close()
{
    g_cached_db_instance->close();
    g_cached_db_instance.reset();
    boost::atomic_thread_fence(boost::memory_order_seq_cst);
}

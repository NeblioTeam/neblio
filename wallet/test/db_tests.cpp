#include "googletest/googletest/include/gtest/gtest.h"

#include "boost/scope_exit.hpp"
#include "curltools.h"
#include "db/defaultdblogger/defaultdblogger.h"
#include "db/lmdb/lmdb.h"
#include "hash.h"
#include "ntp1/ntp1tools.h"
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

const std::string TempNTP1File("ntp1txout.bin");

std::string RandomString(const int len)
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";

    std::string s;
    s.resize(len);
    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return s;
}

#define CUSTOM_LMDB_DB_SIZE (1 << 14)
#include "../txdb-lmdb.h"

TEST(lmdb_tests, basic)
{
    //    std::cout << "LMDB DB size: " << DB_DEFAULT_MAPSIZE << std::endl;

    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB::QuickSyncHigherControl_Enabled = false;
    CTxDB db;

    std::string k1 = "key1";
    std::string v1 = "val1";

    EXPECT_TRUE(db.test1_WriteStrKeyVal(k1, v1));
    std::string out;
    EXPECT_TRUE(db.test1_ReadStrKeyVal(k1, out));
    EXPECT_EQ(out, v1);

    EXPECT_TRUE(db.test1_ExistsStrKeyVal(k1));

    EXPECT_TRUE(db.test1_EraseStrKeyVal(k1));
    EXPECT_FALSE(db.test1_ExistsStrKeyVal(k1));

    db.Close();
}

TEST(lmdb_tests, basic_in_1_tx)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB::QuickSyncHigherControl_Enabled = false;
    CTxDB db;

    db.TxnBegin();

    std::string k1 = "key1";
    std::string v1 = "val1";

    EXPECT_TRUE(db.test1_WriteStrKeyVal(k1, v1));
    std::string out;
    EXPECT_TRUE(db.test1_ReadStrKeyVal(k1, out));
    EXPECT_EQ(out, v1);

    EXPECT_TRUE(db.test1_ExistsStrKeyVal(k1));

    db.TxnAbort();

    // uncommitted data shouldn't exist
    EXPECT_FALSE(db.test1_ExistsStrKeyVal(k1));

    db.Close();
}

TEST(lmdb_tests, many_inputs)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB::QuickSyncHigherControl_Enabled = false;
    CTxDB db;

    std::unordered_map<std::string, std::string> entries;

    const uint64_t entriesCount = 100;
    for (uint64_t i = 0; i < entriesCount; i++) {
        std::string k = RandomString(100);
        std::string v = RandomString(1000000);

        if (entries.find(k) != entries.end()) {
            continue;
        }

        entries[k] = v;

        EXPECT_TRUE(db.test1_WriteStrKeyVal(k, v));
        std::string out;

        EXPECT_TRUE(db.test1_ReadStrKeyVal(k, out));
        EXPECT_EQ(out, v);

        EXPECT_TRUE(db.test1_ExistsStrKeyVal(k));
    }

    for (const auto& pair : entries) {
        std::string out;
        EXPECT_TRUE(db.test1_ReadStrKeyVal(pair.first, out));
        EXPECT_EQ(out, pair.second);

        EXPECT_TRUE(db.test1_ExistsStrKeyVal(pair.first));
    }
    db.Close();
}

TEST(lmdb_tests, many_inputs_one_tx)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB::QuickSyncHigherControl_Enabled = false;
    CTxDB db;

    std::unordered_map<std::string, std::string> entries;

    const uint64_t entriesCount = 100;

    std::size_t keySize = 100;
    std::size_t valSize = 1000000;

    db.TxnBegin(keySize * valSize * 11 / 10);
    for (uint64_t i = 0; i < entriesCount; i++) {
        std::string k = RandomString(keySize);
        std::string v = RandomString(valSize);

        if (entries.find(k) != entries.end()) {
            continue;
        }

        entries[k] = v;

        EXPECT_TRUE(db.test1_WriteStrKeyVal(k, v));
        std::string out;

        EXPECT_TRUE(db.test1_ReadStrKeyVal(k, out));
        EXPECT_EQ(out, v);

        EXPECT_TRUE(db.test1_ExistsStrKeyVal(k));
    }
    db.TxnCommit();

    for (const auto& pair : entries) {
        std::string out;
        EXPECT_TRUE(db.test1_ReadStrKeyVal(pair.first, out));
        EXPECT_EQ(out, pair.second);

        EXPECT_TRUE(db.test1_ExistsStrKeyVal(pair.first));
    }
    db.Close();
}

TEST(lmdb_tests, basic_multiple_read)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB::QuickSyncHigherControl_Enabled = false;
    CTxDB db;

    const std::string k1 = "key1";
    const std::string k2 = "key2";
    const std::string v1 = "val1";
    const std::string v2 = "val2";
    const std::string v3 = "val3";
    const std::string v4 = "val4";
    const std::string v5 = "val5";
    const std::string v6 = "val6";

    EXPECT_TRUE(db.test2_WriteStrKeyVal(k1, v1));
    EXPECT_TRUE(db.test2_WriteStrKeyVal(k1, v2));
    EXPECT_TRUE(db.test2_WriteStrKeyVal(k1, v3));
    EXPECT_TRUE(db.test2_WriteStrKeyVal(k2, v4));
    EXPECT_TRUE(db.test2_WriteStrKeyVal(k2, v5));
    EXPECT_TRUE(db.test2_WriteStrKeyVal(k2, v6));
    std::vector<std::string> outs1;
    std::vector<std::string> outs2;
    EXPECT_TRUE(db.test2_ReadMultipleStr1KeyVal(k1, outs1));
    EXPECT_EQ(outs1, std::vector<std::string>({v1, v2, v3}));
    EXPECT_TRUE(db.test2_ReadMultipleStr1KeyVal(k2, outs2));
    EXPECT_EQ(outs2, std::vector<std::string>({v4, v5, v6}));

    std::map<std::string, std::vector<std::string>> allValsMap;
    EXPECT_TRUE(db.test2_ReadMultipleAllStr1KeyVal(allValsMap));
    EXPECT_EQ(allValsMap, (std::map<std::string, std::vector<std::string>>(
                              {{k1, std::vector<std::string>({v1, v2, v3})},
                               {k2, std::vector<std::string>({v4, v5, v6})}})));

    EXPECT_TRUE(db.test2_ExistsStrKeyVal(k1));

    EXPECT_TRUE(db.test2_EraseStrKeyVal(k1));

    EXPECT_FALSE(db.test2_ExistsStrKeyVal(k1));

    // uncommitted data shouldn't exist
    EXPECT_FALSE(db.test1_ExistsStrKeyVal(k1));

    db.Close();
}

TEST(lmdb_tests, basic_multiple_read_in_tx)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB::QuickSyncHigherControl_Enabled = false;
    CTxDB db;
    db.TxnBegin(100);

    std::string k1 = "key1";
    std::string v1 = "val1";
    std::string v2 = "val2";
    std::string v3 = "val3";

    EXPECT_TRUE(db.test2_WriteStrKeyVal(k1, v1));
    EXPECT_TRUE(db.test2_WriteStrKeyVal(k1, v2));
    EXPECT_TRUE(db.test2_WriteStrKeyVal(k1, v3));
    std::vector<std::string> outs;
    EXPECT_TRUE(db.test2_ReadMultipleStr1KeyVal(k1, outs));
    EXPECT_EQ(outs, std::vector<std::string>({v1, v2, v3}));

    EXPECT_TRUE(db.test2_ExistsStrKeyVal(k1));

    EXPECT_TRUE(db.test2_EraseStrKeyVal(k1));

    EXPECT_FALSE(db.test2_ExistsStrKeyVal(k1));

    // uncommitted data shouldn't exist
    EXPECT_FALSE(db.test1_ExistsStrKeyVal(k1));

    db.TxnCommit();

    db.Close();
}

TEST(lmdb_tests, basic_multiple_many_inputs)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB::QuickSyncHigherControl_Enabled = false;
    CTxDB db;

    std::vector<std::string> entries;

    std::string k = "TheKey";

    EXPECT_FALSE(db.test2_ExistsStrKeyVal(k));

    const uint64_t entriesCount = 1;
    for (uint64_t i = 0; i < entriesCount; i++) {
        std::string v = RandomString(508); // bigger size seems to create error: MDB_BAD_VALSIZE

        entries.push_back(v);

        EXPECT_TRUE(db.test2_WriteStrKeyVal(k, v));
        std::string out;

        EXPECT_TRUE(db.test2_ExistsStrKeyVal(k));
    }

    std::vector<std::string> outs;
    EXPECT_TRUE(db.test2_ReadMultipleStr1KeyVal(k, outs));
    EXPECT_EQ(outs, entries);

    EXPECT_TRUE(db.test2_ExistsStrKeyVal(k));

    EXPECT_TRUE(db.test2_EraseStrKeyVal(k));

    EXPECT_FALSE(db.test2_ExistsStrKeyVal(k));

    db.Close();
}

static void EnsureDBIsEmpty(IDB* db, IDB::Index dbindex)
{
    auto m = db->readAll(dbindex);
    ASSERT_TRUE(m);
    EXPECT_EQ(m->size(), 0);
}

static void TestReadWriteUnique(IDB* db, const std::map<std::string, std::string>& data)
{
    for (const auto& v : data) {
        db->write(IDB::Index::DB_MAIN_INDEX, v.first, v.second);
    }

    for (const auto& v : data) {
        boost::optional<std::string> r = db->read(IDB::Index::DB_MAIN_INDEX, v.first, 0, boost::none);
        ASSERT_TRUE(r);
        EXPECT_EQ(v.second, *r);
    }

    static constexpr std::size_t MAX_OFFSET_TESTS = 100;
    static constexpr std::size_t MAX_SIZE_TESTS   = 100;

    for (const auto& v : data) {
        const std::string expected = v.second;
        for (std::size_t sizeStep = 0; sizeStep <= MAX_SIZE_TESTS; sizeStep++) {
            const std::size_t size = rand() % (MAX_SIZE + 1);
            for (std::size_t offsetStep = 0; offsetStep < MAX_OFFSET_TESTS; offsetStep++) {
                const std::size_t offset = rand() % (expected.size() + 1);
                // offset can't be larger than string size
                const std::string subExpected = expected.substr(offset, size);

                const boost::optional<std::string> r =
                    db->read(IDB::Index::DB_MAIN_INDEX, v.first, offset, size);
                ASSERT_TRUE(r);
                EXPECT_EQ(subExpected, *r)
                    << "Failed with expected " << expected << "; subExpected " << subExpected
                    << "; offset: " << offset << "; size: " << size;
            }
        }
    }

    for (const auto& v : data) {
        const std::string& key = v.first;
        EXPECT_TRUE(db->exists(IDB::Index::DB_MAIN_INDEX, key));
    }

    {
        std::map<std::string, std::string> expected = data;
        while (!expected.empty()) {
            std::size_t indexToDelete = rand() % expected.size();
            auto        it            = expected.begin();
            std::advance(it, indexToDelete);
            const std::string key = it->first;
            EXPECT_TRUE(db->exists(IDB::Index::DB_MAIN_INDEX, key));

            // erase the key
            expected.erase(key);
            EXPECT_TRUE(db->erase(IDB::Index::DB_MAIN_INDEX, key));

            // value doesn't exist anymore, let's verify that
            EXPECT_FALSE(db->exists(IDB::Index::DB_MAIN_INDEX, key));
            EXPECT_EQ(db->read(IDB::Index::DB_MAIN_INDEX, key), boost::none);
        }
    }

    EnsureDBIsEmpty(db, IDB::Index::DB_BLOCKINDEX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_BLOCKS_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_TX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_NTP1TX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_NTP1TOKENNAMES_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_ADDRSVSPUBKEYS_INDEX);
}

TEST(db_interface_impl_tests, read_write_unique)
{
    static constexpr int MAX_ENTRIES = 20;
    static constexpr int MAX_SIZE    = 500;

    std::map<std::string, std::string> data;

    for (int i = 0; i < MAX_ENTRIES; i++) {
        const std::size_t keySize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
        const std::size_t valSize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
        const std::string key     = GeneratePseudoRandomString(keySize);
        const std::string val     = GeneratePseudoRandomString(valSize);

        data[key] = val;
    }

    const boost::filesystem::path p = GetDataDir() / "test-txdb";

    DefaultDBLogger logger;

    std::unique_ptr<IDB> db = MakeUnique<LMDB>(&p, &logger, true);

    BOOST_SCOPE_EXIT(&db) { db->close(); }
    BOOST_SCOPE_EXIT_END

    TestReadWriteUnique(db.get(), data);
}

TEST(db_interface_impl_tests, read_write_unique_with_transaction)
{
    static constexpr int MAX_ENTRIES = 20;
    static constexpr int MAX_SIZE    = 500;

    std::map<std::string, std::string> data;

    for (int i = 0; i < MAX_ENTRIES; i++) {
        const std::size_t keySize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
        const std::size_t valSize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
        const std::string key     = GeneratePseudoRandomString(keySize);
        const std::string val     = GeneratePseudoRandomString(valSize);

        data[key] = val;
    }

    const boost::filesystem::path p = GetDataDir() / "test-txdb";

    DefaultDBLogger logger;

    std::unique_ptr<IDB> db = MakeUnique<LMDB>(&p, &logger, true);

    BOOST_SCOPE_EXIT(&db) { db->close(); }
    BOOST_SCOPE_EXIT_END

    const std::pair<std::string, std::string> someRandomKeyVal =
        std::make_pair(GeneratePseudoRandomString(100), GeneratePseudoRandomString(100));

    db->write(IDB::Index::DB_MAIN_INDEX, someRandomKeyVal.first, someRandomKeyVal.second);

    db->beginDBTransaction();

    TestReadWriteUnique(db.get(), data);

    db->abortDBTransaction();

    // after having aborted the transaction, we only have the value we committed
    const boost::optional<std::map<std::string, std::vector<std::string>>> map =
        db->readAll(IDB::Index::DB_MAIN_INDEX);
    ASSERT_TRUE(map);
    ASSERT_EQ(map->size(), 1);
    ASSERT_EQ(map->count(someRandomKeyVal.first), 1);
}

static void TestReadMultipleAndRealAll(IDB*                                                   db,
                                       const std::map<std::string, std::vector<std::string>>& data)
{
    for (const auto& v : data) {
        for (const auto& e : v.second) {
            db->write(IDB::Index::DB_NTP1TOKENNAMES_INDEX, v.first, e);
        }
    }

    for (const auto& v : data) {
        std::vector<std::string>                  expected = v.second;
        boost::optional<std::vector<std::string>> r =
            db->readMultiple(IDB::Index::DB_NTP1TOKENNAMES_INDEX, v.first);
        ASSERT_TRUE(r);
        std::sort(r->begin(), r->end());
        std::sort(expected.begin(), expected.end());
        expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
        EXPECT_EQ(expected, *r);
    }

    {
        std::map<std::string, std::vector<std::string>> expected = data;
        for (auto&& v : expected) {
            std::sort(v.second.begin(), v.second.end());
            // ensure entries are unique
            v.second.erase(std::unique(v.second.begin(), v.second.end()), v.second.end());
        }
        boost::optional<std::map<std::string, std::vector<std::string>>> r =
            db->readAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX);
        ASSERT_TRUE(r);
        for (auto&& v : *r) {
            std::sort(v.second.begin(), v.second.end());
        }
        EXPECT_EQ(expected, r);
    }

    for (const auto& v : data) {
        const std::string& key = v.first;
        EXPECT_TRUE(db->exists(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));
    }

    {
        std::map<std::string, std::vector<std::string>> expected = data;
        while (!expected.empty()) {
            std::size_t indexToDelete = rand() % expected.size();
            auto        it            = expected.begin();
            std::advance(it, indexToDelete);
            const std::string key = it->first;
            EXPECT_TRUE(db->exists(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));

            // erase the key
            expected.erase(key);
            EXPECT_TRUE(db->eraseAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));

            // value doesn't exist anymore, let's verify that
            EXPECT_FALSE(db->exists(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));
            auto v = db->readMultiple(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key);
            ASSERT_TRUE(v);
            EXPECT_EQ(v->size(), 0);
            const boost::optional<std::map<std::string, std::vector<std::string>>> m =
                db->readAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX);
            ASSERT_TRUE(m);
            EXPECT_TRUE(m->find(key) == m->cend());
        }
    }

    EnsureDBIsEmpty(db, IDB::Index::DB_MAIN_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_BLOCKINDEX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_BLOCKS_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_TX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_NTP1TX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_ADDRSVSPUBKEYS_INDEX);
}

TEST(db_interface_impl_tests, read_write_multiple)
{
    static constexpr int MAX_ENTRIES    = 5;
    static constexpr int MAX_SUBENTRIES = 3;
    static constexpr int MAX_SIZE       = 500;

    std::map<std::string, std::vector<std::string>> data;

    for (int i = 0; i < MAX_ENTRIES; i++) {
        const std::size_t keySize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
        const std::string key     = GeneratePseudoRandomString(keySize);
        for (int j = 0; j < MAX_SUBENTRIES; j++) {
            const std::size_t valSize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
            const std::string val     = GeneratePseudoRandomString(valSize);

            data[key].push_back(val);
        }
    }

    const boost::filesystem::path p = GetDataDir() / "test-txdb";

    DefaultDBLogger logger;

    std::unique_ptr<IDB> db = MakeUnique<LMDB>(&p, &logger, true);

    BOOST_SCOPE_EXIT(&db) { db->close(); }
    BOOST_SCOPE_EXIT_END

    TestReadMultipleAndRealAll(db.get(), data);
}

static void TestReadMultipleAndRealAllWithTx(IDB*                                                   db,
                                             const std::map<std::string, std::vector<std::string>>& data)
{
    const std::pair<std::string, std::string> someRandomKeyVal =
        std::make_pair(GeneratePseudoRandomString(100), GeneratePseudoRandomString(100));

    db->write(IDB::Index::DB_NTP1TOKENNAMES_INDEX, someRandomKeyVal.first, someRandomKeyVal.second);

    db->beginDBTransaction();

    ////////////////
    for (const auto& v : data) {
        for (const auto& e : v.second) {
            db->write(IDB::Index::DB_NTP1TOKENNAMES_INDEX, v.first, e);
        }
    }

    for (const auto& v : data) {
        std::vector<std::string>                  expected = v.second;
        boost::optional<std::vector<std::string>> r =
            db->readMultiple(IDB::Index::DB_NTP1TOKENNAMES_INDEX, v.first);
        ASSERT_TRUE(r);
        std::sort(r->begin(), r->end());
        std::sort(expected.begin(), expected.end());
        expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
        EXPECT_EQ(expected, *r);
    }

    {
        std::map<std::string, std::vector<std::string>> expected = data;
        for (auto&& v : expected) {
            std::sort(v.second.begin(), v.second.end());
            // ensure entries are unique
            v.second.erase(std::unique(v.second.begin(), v.second.end()), v.second.end());
        }
        boost::optional<std::map<std::string, std::vector<std::string>>> r =
            db->readAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX);
        ASSERT_TRUE(r);
        for (auto&& v : *r) {
            std::sort(v.second.begin(), v.second.end());
        }
        r->erase(someRandomKeyVal.first);
        EXPECT_EQ(expected, r);
    }

    for (const auto& v : data) {
        const std::string& key = v.first;
        EXPECT_TRUE(db->exists(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));
    }

    {
        std::map<std::string, std::vector<std::string>> expected = data;
        while (!expected.empty()) {
            std::size_t indexToDelete = rand() % expected.size();
            auto        it            = expected.begin();
            std::advance(it, indexToDelete);
            const std::string key = it->first;
            EXPECT_TRUE(db->exists(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));

            // erase the key
            expected.erase(key);
            EXPECT_TRUE(db->eraseAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));

            // value doesn't exist anymore, let's verify that
            EXPECT_FALSE(db->exists(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key));
            auto v = db->readMultiple(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key);
            ASSERT_TRUE(v);
            EXPECT_EQ(v->size(), 0);
            const boost::optional<std::map<std::string, std::vector<std::string>>> m =
                db->readAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX);
            ASSERT_TRUE(m);
            EXPECT_TRUE(m->find(key) == m->cend());
        }
    }

    ////////////////
    db->abortDBTransaction();

    // after having aborted the transaction, we only have the value we committed
    const boost::optional<std::map<std::string, std::vector<std::string>>> map =
        db->readAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX);
    ASSERT_TRUE(map);
    ASSERT_EQ(map->size(), 1);
    ASSERT_EQ(map->count(someRandomKeyVal.first), 1);

    EnsureDBIsEmpty(db, IDB::Index::DB_MAIN_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_BLOCKINDEX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_BLOCKS_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_TX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_NTP1TX_INDEX);
    EnsureDBIsEmpty(db, IDB::Index::DB_ADDRSVSPUBKEYS_INDEX);
}

TEST(db_interface_impl_tests, read_write_multiple_with_db_transaction)
{
    static constexpr int MAX_ENTRIES    = 5;
    static constexpr int MAX_SUBENTRIES = 3;
    static constexpr int MAX_SIZE       = 500;

    std::map<std::string, std::vector<std::string>> data;

    for (int i = 0; i < MAX_ENTRIES; i++) {
        const std::size_t keySize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
        const std::string key     = GeneratePseudoRandomString(keySize);
        for (int j = 0; j < MAX_SUBENTRIES; j++) {
            const std::size_t valSize = static_cast<std::size_t>(1 + rand() % MAX_SIZE);
            const std::string val     = GeneratePseudoRandomString(valSize);

            data[key].push_back(val);
        }
    }

    const boost::filesystem::path p = GetDataDir() / "test-txdb";

    DefaultDBLogger logger;

    std::unique_ptr<IDB> db = MakeUnique<LMDB>(&p, &logger, true);

    BOOST_SCOPE_EXIT(&db) { db->close(); }
    BOOST_SCOPE_EXIT_END

    TestReadMultipleAndRealAllWithTx(db.get(), data);
}

TEST(quicksync_tests, download_index_file)
{
    std::string        s = cURLTools::GetFileFromHTTPS(QuickSyncDataLink, 30, false);
    json_spirit::Value parsedData;
    json_spirit::read_or_throw(s, parsedData);
    json_spirit::Array rootArray = parsedData.get_array();
    ASSERT_GE(rootArray.size(), 1u);
    for (const json_spirit::Value& val : rootArray) {
        json_spirit::Array files         = NTP1Tools::GetArrayField(val.get_obj(), "files");
        bool               lockFileFound = false;
        for (const json_spirit::Value& fileVal : files) {
            json_spirit::Array urlsObj  = NTP1Tools::GetArrayField(fileVal.get_obj(), "url");
            std::string        sum      = NTP1Tools::GetStrField(fileVal.get_obj(), "sha256sum");
            int64_t            fileSize = NTP1Tools::GetInt64Field(fileVal.get_obj(), "size");
            std::string        sumBin   = boost::algorithm::unhex(sum);
            EXPECT_GT(fileSize, 0);
            ASSERT_GE(urlsObj.size(), 0u);
            for (const auto& urlObj : urlsObj) {
                std::string url = urlObj.get_str();
                // test the lock file, if this iteration is for the lock file
                if (boost::algorithm::ends_with(url, "lock.mdb")) {
                    lockFileFound = true;
                    {
                        // test by loading to memory and calculating the hash
                        std::string lockFile = cURLTools::GetFileFromHTTPS(url, 30, false);
                        std::string sha256_result;
                        sha256_result.resize(32);
                        SHA256(reinterpret_cast<unsigned char*>(&lockFile.front()), lockFile.size(),
                               reinterpret_cast<unsigned char*>(&sha256_result.front()));
                        EXPECT_EQ(sumBin, sha256_result);
                    }
                    {
                        // test by downloading to a file and calculating the hash
                        std::atomic<float>      progress;
                        boost::filesystem::path testFilePath = "test_lock.mdb";
                        cURLTools::GetLargeFileFromHTTPS(url, 30, testFilePath, progress);
                        std::string sha256_result = CalculateHashOfFile<Sha256Calculator>(testFilePath);
                        EXPECT_EQ(sumBin, sha256_result);
                        boost::filesystem::remove(testFilePath);
                    }
                }
                // test the data file, if this iteration is for the data file
                //            if (boost::algorithm::ends_with(url, "data.mdb")) {
                //                std::string url    = NTP1Tools::GetStrField(fileVal.get_obj(), "url");
                //                std::string sum    = NTP1Tools::GetStrField(fileVal.get_obj(),
                //                "sha256sum"); std::string sumBin = boost::algorithm::unhex(sum);
                //                {
                //                    // test by downloading to a file and calculating the hash
                //                    std::atomic<float>      progress;
                //                    boost::filesystem::path testFilePath = "test_data.mdb";
                //                    std::atomic_bool        finishedDownload;
                //                    finishedDownload.store(false);
                //                    boost::thread downloadThread([&]() {
                //                        cURLTools::GetLargeFileFromHTTPS(url, 30, testFilePath,
                //                        progress); finishedDownload.store(true);
                //                    });
                //                    std::cout << "Downloading file: " << url << std::endl;
                //                    while (!finishedDownload) {
                //                        std::cout << "File download progress: " << progress.load() <<
                //                        "%"
                //                        << std::endl;
                //                        std::this_thread::sleep_for(std::chrono::seconds(2));
                //                    }
                //                    std::cout << "File download progress: "
                //                              << "100"
                //                              << "%" << std::endl;
                //                    downloadThread.join();
                //                    std::string sha256_result =
                //                    CalculateHashOfFile<Sha256Calculator>(testFilePath);
                //                    EXPECT_EQ(sumBin, sha256_result);
                //                    boost::filesystem::remove(testFilePath);
                //                }
                //            }
            }
        }
        EXPECT_TRUE(lockFileFound) << "For one entry, lock file not found: " << QuickSyncDataLink;
        std::string os = NTP1Tools::GetStrField(val.get_obj(), "os");
    }
}

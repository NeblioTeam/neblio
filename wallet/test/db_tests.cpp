#include "googletest/googletest/include/gtest/gtest.h"

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

#ifdef USE_LMDB

#define CUSTOM_LMDB_DB_SIZE (1 << 14)
#include "../txdb-lmdb.h"

TEST(lmdb_tests, basic)
{
    std::cout << "LMDB DB size: " << DB_DEFAULT_MAPSIZE << std::endl;

    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB db;

    std::string k1 = "key1";
    std::string v1 = "val1";

    EXPECT_TRUE(db.WriteStrKeyVal(k1, v1));
    std::string out;
    EXPECT_TRUE(db.ReadStrKeyVal(k1, out));
    EXPECT_EQ(out, v1);

    EXPECT_TRUE(db.ExistsStrKeyVal(k1));

    EXPECT_TRUE(db.EraseStrKeyVal(k1));
    EXPECT_FALSE(db.ExistsStrKeyVal(k1));

    db.Close();
}

TEST(lmdb_tests, basic_in_1_tx)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB db;

    db.TxnBegin();

    std::string k1 = "key1";
    std::string v1 = "val1";

    EXPECT_TRUE(db.WriteStrKeyVal(k1, v1));
    std::string out;
    EXPECT_TRUE(db.ReadStrKeyVal(k1, out));
    EXPECT_EQ(out, v1);

    EXPECT_TRUE(db.ExistsStrKeyVal(k1));

    db.TxnAbort();

    // uncommitted data shouldn't exist
    EXPECT_FALSE(db.ExistsStrKeyVal(k1));

    db.Close();
}

TEST(lmdb_tests, many_inputs)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB db;

    std::unordered_map<std::string, std::string> entries;

    const uint64_t entriesCount = 100;
    for (uint64_t i = 0; i < entriesCount; i++) {
        std::string k = RandomString(100);
        std::string v = RandomString(1000000);
        //        std::string k = "abcdefghijklmnopqrstuv";
        //        std::string v = "abcdefghijklmn";

        if (entries.find(k) != entries.end()) {
            continue;
        }

        entries[k] = v;

        EXPECT_TRUE(db.WriteStrKeyVal(k, v));
        std::string out;

        EXPECT_TRUE(db.ReadStrKeyVal(k, out));
        EXPECT_EQ(out, v);

        EXPECT_TRUE(db.ExistsStrKeyVal(k));
    }

    for (const auto& pair : entries) {
        std::string out;
        EXPECT_TRUE(db.ReadStrKeyVal(pair.first, out));
        EXPECT_EQ(out, pair.second);

        EXPECT_TRUE(db.ExistsStrKeyVal(pair.first));
    }
    db.Close();
}

TEST(lmdb_tests, many_inputs_one_tx)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

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

        EXPECT_TRUE(db.WriteStrKeyVal(k, v));
        std::string out;

        EXPECT_TRUE(db.ReadStrKeyVal(k, out));
        EXPECT_EQ(out, v);

        EXPECT_TRUE(db.ExistsStrKeyVal(k));
    }
    db.TxnCommit();

    for (const auto& pair : entries) {
        std::string out;
        EXPECT_TRUE(db.ReadStrKeyVal(pair.first, out));
        EXPECT_EQ(out, pair.second);

        EXPECT_TRUE(db.ExistsStrKeyVal(pair.first));
    }
    db.Close();
}

#endif

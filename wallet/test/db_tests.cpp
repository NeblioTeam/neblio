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

#include "../txdb-lmdb.h"

TEST(lmdb_tests, basic)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB db;

    std::string k1 = "key1";
    std::string v1 = "val1";

    EXPECT_TRUE(db.Write(k1, v1));
    std::string out;
    EXPECT_TRUE(db.Read(k1, out));
    EXPECT_EQ(out, v1);

    EXPECT_TRUE(db.Exists(k1));

    EXPECT_TRUE(db.Erase(k1));
    EXPECT_FALSE(db.Exists(k1));
}

TEST(lmdb_tests, basic_in_1_tx)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB db;

    db.TxnBegin();

    std::string k1 = "key1";
    std::string v1 = "val1";

    EXPECT_TRUE(db.Write(k1, v1));
    std::string out;
    EXPECT_TRUE(db.Read(k1, out));
    EXPECT_EQ(out, v1);

    EXPECT_TRUE(db.Exists(k1));

    db.TxnAbort();

    // uncommitted data shouldn't exist
    EXPECT_FALSE(db.Exists(k1));
}

TEST(lmdb_tests, many_inputs)
{
    CTxDB::DB_DIR = "test-txdb"; // avoid writing to the main database

    CTxDB::__deleteDb(); // clean up

    CTxDB db;

    std::unordered_map<std::string, std::string> entries;

    std::cout << mdb_env_get_maxkeysize(dbEnv.get()) << std::endl;

    const uint64_t entriesCount = 1;
    for (uint64_t i = 0; i < entriesCount; i++) {
        std::string k = RandomString(14);
        std::string v = RandomString(14);

        if (entries.find(k) != entries.end()) {
            continue;
        }

        entries[k] = v;

        EXPECT_TRUE(db.Write(k, v));
        std::string out;
        EXPECT_TRUE(db.Read(k, out));
        EXPECT_EQ(out, v);

        EXPECT_TRUE(db.Exists(k));
    }
    std::string tmp;
    std::cout << db.Read(std::string("version"), tmp) << std::endl;
    std::cout << tmp << std::endl;
}

#endif

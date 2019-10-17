#include "googletest/googletest/include/gtest/gtest.h"

#include "curltools.h"
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
    std::cout << "LMDB DB size: " << DB_DEFAULT_MAPSIZE << std::endl;

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
                //                        boost::this_thread::sleep_for(boost::chrono::seconds(2));
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

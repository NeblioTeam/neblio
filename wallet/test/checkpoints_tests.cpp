#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "block.h"
#include "blockindex.h"
#include "checkpoints.h"
#include "mocks/mtxdb.h"
#include "ntp1/ntp1transaction.h"
#include "util.h"

#include <cstdlib>

using namespace std;

namespace {
template <typename T>
std::string PODToString(const T& val)
{
    static_assert(std::is_pod<T>::value, "Must be POD type");
    std::string result(reinterpret_cast<const char*>(&val), sizeof(val));
    return result;
}

uint256 HashFromHeight(const int height)
{
    // we use this function to create a block hash that's deterministically derived from the block
    // heights. Very useful for tests
    Sha256Calculator sha256calculator;
    sha256calculator.push_data(PODToString(height));
    const std::string hash = sha256calculator.getHashAndReset();
    const uint256     res(boost::algorithm::hex(hash));
    return res;
}

ACTION_P(ReadBlockIndexAction, BlockIndexMap)
{
    const uint256& hash = arg0;

    const auto it = BlockIndexMap->find(hash);
    if (it != BlockIndexMap->cend()) {
        return it->second;
    } else {
        return boost::none;
    }

    boost::ignore_unused(args);
    boost::ignore_unused(arg1);
    boost::ignore_unused(arg2);
    boost::ignore_unused(arg3);
    boost::ignore_unused(arg4);
    boost::ignore_unused(arg5);
    boost::ignore_unused(arg6);
    boost::ignore_unused(arg7);
    boost::ignore_unused(arg8);
    boost::ignore_unused(arg9);
}

} // namespace

struct CheckpointsTests : public ::testing::Test
{
private:
    std::map<uint256, CBlockIndex> blockIndexMap;

public:
    void SetUp() override {}
    void TearDown() override {}

    void insertNewBlockIndex(uint256 hash, uint256 hashPrev, uint256 hashNext, int height)
    {

        CBlockIndex bi;
        bi.blockHash = hash;
        bi.hashPrev  = hashPrev;
        bi.hashNext  = hashNext;
        bi.nHeight   = height;

        blockIndexMap[bi.blockHash] = bi;

        if (height == 0) {
            pindexGenesisBlock = boost::make_shared<CBlockIndex>(bi);
        }
    }

    const CBlockIndex* getBlockIndex(const uint256& hash) const
    {
        const auto it = blockIndexMap.find(hash);
        if (it == blockIndexMap.cend()) {
            return nullptr;
        }
        return &it->second;
    }

    boost::shared_ptr<mTxDB> MakeDBMock(int bestHeight = 0) const
    {
        boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();

        EXPECT_CALL(*dbMock, ReadBlockIndex(::testing::_))
            .WillRepeatedly(ReadBlockIndexAction(&blockIndexMap));

        EXPECT_CALL(*dbMock, GetBestChainHeight()).WillRepeatedly(testing::Return(bestHeight));

        return dbMock;
    }
};

TEST_F(CheckpointsTests, CheckSync_ManyCheckpoints)
{

    for (int i = 0; i < 2; i++) {
        Checkpoints::BlockToCheckpointCache checkpointsCache;

        const bool cache = !!i;

        for (int j = 0; j < 200; j++) {
            if (j > 0) {
                insertNewBlockIndex(HashFromHeight(j), HashFromHeight(j - 1), HashFromHeight(j + 1), j);
            } else {
                insertNewBlockIndex(HashFromHeight(j), 0, HashFromHeight(j + 1), j);
            }
        }

        MapCheckpoints checkpointData({
            {0, HashFromHeight(0)},
            {10, HashFromHeight(10)},
            {20, HashFromHeight(20)},
            {30, HashFromHeight(30)},
            {40, HashFromHeight(40)},
            {50, HashFromHeight(50)},
            {60, HashFromHeight(60)},
            {70, HashFromHeight(70)},
            {80, HashFromHeight(80)},
            {90, HashFromHeight(90)},
            {100, HashFromHeight(100)},
        });

        auto dbMock = MakeDBMock();

        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(1), getBlockIndex(HashFromHeight(0)),
                                           cache, checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(2), getBlockIndex(HashFromHeight(1)),
                                           cache, checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(6), getBlockIndex(HashFromHeight(5)),
                                           cache, checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(10), getBlockIndex(HashFromHeight(9)),
                                           cache, checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(11),
                                           getBlockIndex(HashFromHeight(10)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(11)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(15)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(20),
                                           getBlockIndex(HashFromHeight(19)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(21),
                                           getBlockIndex(HashFromHeight(20)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(25)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(31),
                                           getBlockIndex(HashFromHeight(30)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(35)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(40),
                                           getBlockIndex(HashFromHeight(40)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(45)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(100),
                                           getBlockIndex(HashFromHeight(99)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(101),
                                           getBlockIndex(HashFromHeight(100)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(101)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(150)), cache, checkpointData,
                                           checkpointsCache));
    }
}

TEST_F(CheckpointsTests, CheckSync_OnlyGenesis)
{

    for (int i = 0; i < 2; i++) {
        Checkpoints::BlockToCheckpointCache checkpointsCache;

        const bool cache = !!i;

        for (int j = 0; j < 200; j++) {
            if (j > 0) {
                insertNewBlockIndex(HashFromHeight(j), HashFromHeight(j - 1), HashFromHeight(j + 1), j);
            } else {
                insertNewBlockIndex(HashFromHeight(j), 0, HashFromHeight(j + 1), j);
            }
        }

        MapCheckpoints checkpointData({
            {0, HashFromHeight(0)},
        });

        auto dbMock = MakeDBMock();

        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(0), getBlockIndex(HashFromHeight(0)),
                                           cache, checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(1)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(5)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(9)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(10)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(11)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(15)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(20)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(25)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(30)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(35)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(40)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(45)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(99)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(100)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(101)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(150)), cache, checkpointData,
                                           checkpointsCache));
    }
}

TEST_F(CheckpointsTests, CheckSync_EmptyCheckpoints)
{
    for (int i = 0; i < 2; i++) {
        Checkpoints::BlockToCheckpointCache checkpointsCache;

        const bool cache = !!i;

        for (int j = 0; j < 200; j++) {
            if (j > 0) {
                insertNewBlockIndex(HashFromHeight(j), HashFromHeight(j - 1), HashFromHeight(j + 1), j);
            } else {
                insertNewBlockIndex(HashFromHeight(j), 0, HashFromHeight(j + 1), j);
            }
        }

        MapCheckpoints checkpointData({});

        auto dbMock = MakeDBMock();

        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(0)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(1)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(5)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(9)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(10)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(11)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(15)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(20)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(25)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(30)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(35)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(40)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(45)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(99)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(100)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(101)), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(*dbMock, HashFromHeight(std::rand()),
                                           getBlockIndex(HashFromHeight(150)), cache, checkpointData,
                                           checkpointsCache));
    }
}

TEST_F(CheckpointsTests, CheckSync_GetLastCheckpoint)
{
    for (int j = 0; j < 200; j++) {
        if (j > 0) {
            insertNewBlockIndex(HashFromHeight(j), HashFromHeight(j - 1), HashFromHeight(j + 1), j);
        } else {
            insertNewBlockIndex(HashFromHeight(j), 0, HashFromHeight(j + 1), j);
        }
    }

    MapCheckpoints checkpointData({
        {0, HashFromHeight(0)},
        {10, HashFromHeight(10)},
        {20, HashFromHeight(20)},
        {30, HashFromHeight(30)},
        {40, HashFromHeight(40)},
        {50, HashFromHeight(50)},
        {60, HashFromHeight(60)},
        {70, HashFromHeight(70)},
        {80, HashFromHeight(80)},
        {90, HashFromHeight(90)},
        {100, HashFromHeight(100)},
    });

    boost::shared_ptr<mTxDB> dbMock;

    dbMock = MakeDBMock(0);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(1);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(5);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(9);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(10);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 10);
    dbMock = MakeDBMock(15);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 10);
    dbMock = MakeDBMock(19);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 10);
    dbMock = MakeDBMock(20);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 20);
    dbMock = MakeDBMock(25);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 20);
    dbMock = MakeDBMock(29);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 20);
    dbMock = MakeDBMock(50);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 50);
    dbMock = MakeDBMock(56);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 50);
    dbMock = MakeDBMock(59);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 50);
    dbMock = MakeDBMock(100);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 100);
    dbMock = MakeDBMock(120);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 100);
    dbMock = MakeDBMock(150);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 100);
    dbMock = MakeDBMock(190);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 100);
    dbMock = MakeDBMock(200);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 100);
}

TEST_F(CheckpointsTests, CheckSync_GetLastCheckpoint_OneGenesisCheckpoint)
{
    for (int j = 0; j < 200; j++) {
        if (j > 0) {
            insertNewBlockIndex(HashFromHeight(j), HashFromHeight(j - 1), HashFromHeight(j + 1), j);
        } else {
            insertNewBlockIndex(HashFromHeight(j), 0, HashFromHeight(j + 1), j);
        }
    }

    MapCheckpoints checkpointData({
        {0, HashFromHeight(0)},
    });

    boost::shared_ptr<mTxDB> dbMock;

    dbMock = MakeDBMock(0);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(1);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(5);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(9);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(10);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(15);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(19);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(20);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(25);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(29);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(50);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(56);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(59);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(100);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(120);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(150);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(190);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(200);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
}

TEST_F(CheckpointsTests, CheckSync_GetLastCheckpoint_NoCheckpoints)
{
    for (int j = 0; j < 200; j++) {
        if (j > 0) {
            insertNewBlockIndex(HashFromHeight(j), HashFromHeight(j - 1), HashFromHeight(j + 1), j);
        } else {
            insertNewBlockIndex(HashFromHeight(j), 0, HashFromHeight(j + 1), j);
        }
    }

    MapCheckpoints checkpointData({});

    boost::shared_ptr<mTxDB> dbMock;

    dbMock = MakeDBMock(0);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(1);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(5);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(9);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(10);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(15);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(19);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(20);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(25);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(29);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(50);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(56);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(59);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(100);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(120);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(150);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(190);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
    dbMock = MakeDBMock(200);
    EXPECT_EQ(Checkpoints::GetLastCheckpoint(*dbMock, checkpointData)->nHeight, 0);
}

TEST_F(CheckpointsTests, CheckSync_GetTotalBlocksEstimate)
{
    MapCheckpoints checkpointData({
        {0, HashFromHeight(0)},
        {10, HashFromHeight(10)},
        {20, HashFromHeight(20)},
        {30, HashFromHeight(30)},
        {40, HashFromHeight(40)},
        {50, HashFromHeight(50)},
        {60, HashFromHeight(60)},
        {70, HashFromHeight(70)},
        {80, HashFromHeight(80)},
        {90, HashFromHeight(90)},
        {100, HashFromHeight(100)},
    });

    EXPECT_EQ(Checkpoints::GetTotalBlocksEstimate(checkpointData), 100);
}

TEST_F(CheckpointsTests, CheckSync_GetTotalBlocksEstimate_OnlyGenesisCheckpoint)
{
    MapCheckpoints checkpointData({{0, HashFromHeight(0)}});

    EXPECT_EQ(Checkpoints::GetTotalBlocksEstimate(checkpointData), 0);
}

TEST_F(CheckpointsTests, CheckSync_GetTotalBlocksEstimate_NoCheckpoints)
{
    MapCheckpoints checkpointData({{0, HashFromHeight(0)}});

    EXPECT_EQ(Checkpoints::GetTotalBlocksEstimate(checkpointData), 0);
}

TEST_F(CheckpointsTests, CheckSync_CheckHardened)
{
    MapCheckpoints checkpointData({
        {0, HashFromHeight(0)},
        {10, HashFromHeight(10)},
        {20, HashFromHeight(20)},
        {30, HashFromHeight(30)},
        {40, HashFromHeight(40)},
        {50, HashFromHeight(50)},
        {60, HashFromHeight(60)},
        {70, HashFromHeight(70)},
        {80, HashFromHeight(80)},
        {90, HashFromHeight(90)},
        {100, HashFromHeight(100)},
    });

    for (const auto& cp : checkpointData) {
        // checkpoints must be exactly true
        EXPECT_TRUE(Checkpoints::CheckHardened(cp.first, cp.second, checkpointData));
    }

    for (const auto& cp : checkpointData) {
        // checkpoints must be exactly true
        EXPECT_FALSE(Checkpoints::CheckHardened(cp.first, cp.second + 1, checkpointData));
    }

    for (const auto& cp : checkpointData) {
        // everything that isn't in checkpoints shall pass
        EXPECT_TRUE(
            Checkpoints::CheckHardened(cp.first + 1, HashFromHeight(cp.first + 10), checkpointData));
    }
}

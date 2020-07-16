#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "blockindex.h"
#include "checkpoints.h"
#include "mocks/mtxdb.h"
#include "util.h"

#include <cstdlib>

using namespace std;

static CBlockIndexSmartPtr InsertBlockIndex(const uint256& hash, BlockIndexMapType& mapBlockIndexIn)
{
    // Return existing
    map<uint256, CBlockIndexSmartPtr>::iterator mi = mapBlockIndexIn.find(hash);
    if (mi != mapBlockIndexIn.end())
        return mi->second;

    // Create new
    CBlockIndexSmartPtr pindexNew = boost::make_shared<CBlockIndex>();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi                    = mapBlockIndexIn.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

TEST(checkpoints_tests, CheckSync1)
{

    for (int i = 0; i < 2; i++) {
        Checkpoints::BlockToCheckpointCache checkpointsCache;

        const bool cache = !!i;

        BlockIndexMapType blockIndex;
        for (int i = 0; i < 200; i++) {
            CBlockIndexSmartPtr pindexNew = InsertBlockIndex(i, blockIndex);
            if (i > 0) {
                pindexNew->pprev = InsertBlockIndex(i - 1, blockIndex);
                pindexNew->pnext = InsertBlockIndex(i + 1, blockIndex);
            }
            pindexNew->nHeight = i;
        }

        MapCheckpoints checkpointData({
            {0, uint256(0)},
            {10, uint256(10)},
            {20, uint256(20)},
            {30, uint256(30)},
            {40, uint256(40)},
            {50, uint256(50)},
            {60, uint256(60)},
            {70, uint256(70)},
            {80, uint256(80)},
            {90, uint256(90)},
            {100, uint256(100)},
        });

        EXPECT_TRUE(Checkpoints::CheckSync(uint256(0), blockIndex.at(0).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(0), blockIndex.at(1).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(0), blockIndex.at(5).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(0), blockIndex.at(9).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(10), blockIndex.at(10).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(11).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(15).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(20), blockIndex.at(20).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(25).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(30), blockIndex.at(30).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(35).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(40), blockIndex.at(40).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(45).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(90), blockIndex.at(99).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(100), blockIndex.at(100).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(101).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(150).get(), cache,
                                           checkpointData, checkpointsCache));

        for (auto&& bi : blockIndex) {
            bi.second->pprev.reset();
            bi.second->pnext.reset();
            bi.second.reset();
        }
    }
}

TEST(checkpoints_tests, CheckSync2)
{

    for (int i = 0; i < 2; i++) {
        Checkpoints::BlockToCheckpointCache checkpointsCache;

        const bool cache = !!i;

        BlockIndexMapType blockIndex;
        for (int i = 0; i < 200; i++) {
            CBlockIndexSmartPtr pindexNew = InsertBlockIndex(i, blockIndex);
            if (i > 0) {
                pindexNew->pprev = InsertBlockIndex(i - 1, blockIndex);
                pindexNew->pnext = InsertBlockIndex(i + 1, blockIndex);
            }
            pindexNew->nHeight = i;
        }

        MapCheckpoints checkpointData({
            {0, uint256(0)},
        });

        EXPECT_TRUE(Checkpoints::CheckSync(uint256(0), blockIndex.at(0).get(), cache, checkpointData,
                                           checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(1).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(5).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(9).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(10).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(11).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(15).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(20).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(25).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(30).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(35).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(40).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(45).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(99).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(100).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(101).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(150).get(), cache,
                                           checkpointData, checkpointsCache));

        for (auto&& bi : blockIndex) {
            bi.second->pprev.reset();
            bi.second->pnext.reset();
            bi.second.reset();
        }
    }
}

TEST(checkpoints_tests, CheckSync3)
{
    for (int i = 0; i < 2; i++) {
        Checkpoints::BlockToCheckpointCache checkpointsCache;

        const bool cache = !!i;

        BlockIndexMapType blockIndex;
        for (int i = 0; i < 200; i++) {
            CBlockIndexSmartPtr pindexNew = InsertBlockIndex(i, blockIndex);
            if (i > 0) {
                pindexNew->pprev = InsertBlockIndex(i - 1, blockIndex);
                pindexNew->pnext = InsertBlockIndex(i + 1, blockIndex);
            }
            pindexNew->nHeight = i;
        }

        MapCheckpoints checkpointData({});

        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(0).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(1).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(5).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(9).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(10).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(11).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(15).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(20).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(25).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(30).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(35).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(40).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(45).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(99).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(100).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(101).get(), cache,
                                           checkpointData, checkpointsCache));
        EXPECT_TRUE(Checkpoints::CheckSync(uint256(std::rand()), blockIndex.at(150).get(), cache,
                                           checkpointData, checkpointsCache));

        for (auto&& bi : blockIndex) {
            bi.second->pprev.reset();
            bi.second->pnext.reset();
            bi.second.reset();
        }
    }
}

#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "block.h"
#include "blockindex.h"
#include "blockindexlrucache.h"
#include "hash.h"
#include "mocks/mtxdb.h"
#include "ntp1/ntp1transaction.h"

#include <cstdlib>

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

int64_t BlockTimeFromHeight(const int height)
{
    // we use this function to create a block hash that's deterministically derived from the block
    // times. Very useful for tests
    return static_cast<int64_t>(height) * 10;
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

struct BlockIndexLRUTests : public ::testing::Test
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
        bi.nTime     = BlockTimeFromHeight(height);

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

    boost::shared_ptr<mTxDB> MakeUnimplementedDBMock() const
    {
        return boost::make_shared<::testing::StrictMock<mTxDB>>();
    }
};

TEST_F(BlockIndexLRUTests, basic)
{
    auto dbMock = MakeDBMock(10);

    for (int j = 0; j < 200; j++) {
        if (j > 0) {
            insertNewBlockIndex(HashFromHeight(j), HashFromHeight(j - 1), HashFromHeight(j + 1), j);
        } else {
            insertNewBlockIndex(HashFromHeight(j), 0, HashFromHeight(j + 1), j);
        }
    }

    using CacheType = BlockIndexLRUCache<uint64_t>;

    CacheType::RetrieverFunc retriever =
        [](const ITxDB& txdb, const uint256& hash) -> boost::optional<CacheType::BICacheEntry> {
        const boost::optional<CBlockIndex> bi = txdb.ReadBlockIndex(hash);
        if (!bi) {
            return boost::none;
        }
        CacheType::BICacheEntry result;
        result.hash     = bi->blockHash;
        result.prevHash = bi->hashPrev;
        result.value    = bi->GetBlockTime();
        return result;
    };

    CacheType cache(3, retriever);
    ASSERT_TRUE(cache.empty());
    ASSERT_FALSE(cache.getOrderedFront());
    ASSERT_FALSE(cache.getOrderedBack());

    {
        // get a value that'll be pulled from the db
        const int  height = 10;
        const auto val    = cache.get(*dbMock, HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 1u);
    }

    {
        // get a value that doesn't exist even in the db
        auto val = cache.get(*dbMock, HashFromHeight(10000));
        ASSERT_TRUE(!val);
        EXPECT_EQ(cache.size(), 1u);
    }

    {
        // get a value that'll NOT be pulled from the db
        const int  height = 10;
        const auto val    = cache.get(*MakeUnimplementedDBMock(), HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        EXPECT_EQ(cache.size(), 1u);
    }

    {
        // get the value strictly from the cache
        const int  height = 10;
        const auto val    = cache.getFromCache(HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 1u);
    }

    {
        // get a value that'll be pulled from the db
        const int  height = 15;
        const auto val    = cache.get(*dbMock, HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(10));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 2u);
    }

    {
        // get a value that'll NOT be pulled from the db
        const int  height = 15;
        const auto val    = cache.get(*MakeUnimplementedDBMock(), HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        EXPECT_EQ(cache.size(), 2u);
    }

    {
        // get the value strictly from the cache
        const int  height = 15;
        const auto val    = cache.getFromCache(HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(10));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 2u);
    }

    {
        // get a value that'll be pulled from the db
        const int  height = 25;
        const auto val    = cache.get(*dbMock, HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(10));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get a value that'll NOT be pulled from the db
        const int  height = 25;
        const auto val    = cache.get(*MakeUnimplementedDBMock(), HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get the value strictly from the cache
        const int  height = 25;
        const auto val    = cache.getFromCache(HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(10));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get a value that'll be pulled from the db
        const int  height = 35;
        const auto val    = cache.get(*dbMock, HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(15));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get a value that'll NOT be pulled from the db
        const int  height = 35;
        const auto val    = cache.get(*MakeUnimplementedDBMock(), HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get the value strictly from the cache
        const int  height = 35;
        const auto val    = cache.getFromCache(HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(15));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get a value that'll be pulled from the db
        const int  height = 15;
        const auto val    = cache.get(*dbMock, HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(25));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get a value that'll NOT be pulled from the db
        const int  height = 15;
        const auto val    = cache.get(*MakeUnimplementedDBMock(), HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get the value strictly from the cache
        const int  height = 15;
        const auto val    = cache.getFromCache(HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(25));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get a value that'll be pulled from the db
        const int  height = 35;
        const auto val    = cache.get(*dbMock, HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(25));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get a value that'll NOT be pulled from the db
        const int  height = 35;
        const auto val    = cache.get(*MakeUnimplementedDBMock(), HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        // get the value strictly from the cache
        const int  height = 35;
        const auto val    = cache.getFromCache(HashFromHeight(height));
        ASSERT_TRUE(val);
        EXPECT_EQ(val->hash, HashFromHeight(height));
        EXPECT_EQ(val->prevHash, HashFromHeight(height - 1));
        EXPECT_EQ(val->value, BlockTimeFromHeight(height));
        ASSERT_TRUE(cache.getOrderedFront());
        ASSERT_TRUE(cache.getOrderedBack());
        EXPECT_EQ(cache.getOrderedFront()->hash, HashFromHeight(25));
        EXPECT_EQ(cache.getOrderedBack()->hash, HashFromHeight(height));
        EXPECT_EQ(cache.size(), 3u);
    }

    {
        EXPECT_EQ(cache.size(), 3u);
        cache.popOne();
        EXPECT_EQ(cache.size(), 2u);
        cache.clear();
        EXPECT_EQ(cache.size(), 0u);
        EXPECT_TRUE(cache.empty());
    }
}

#include "googletest/googletest/include/gtest/gtest.h"

#include <boost/optional/optional_io.hpp>
#include <iostream>
#include <sstream>

#include "dbcache/hierarchicaldb.h"

int INDEX_MULTI = 0;
int INDEX_MAIN  = 1;

TEST(dbcache, testLowLevelDBUnique)
{
    auto db = HierarchicalDB::Make("Root");

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val1"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(g, std::vector<std::string>({"FromRootKey1Val1"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val2"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1"),
                  std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(g, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val3"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey2", "FromRootKey2Val1"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("FromRootKey2"), std::vector<std::string>({"FromRootKey2Val1"}));

        const auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        const auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey2");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey2", "FromRootKey2Val2"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("FromRootKey2"),
                  std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        const auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        const auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey2");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
    }

    ASSERT_TRUE(db->unique_set(INDEX_MAIN, "FromRootKey1", "FromRootKey1Val4"));
    {
        auto d = db->unique_get(INDEX_MAIN, "FromRootKey1", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "FromRootKey1Val4");

        d = db->unique_get(INDEX_MAIN, "FromRootKey1", 1u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "romRootKey1Val4");

        d = db->unique_get(INDEX_MAIN, "FromRootKey1", 2u, 3);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "omR");
    }

    ASSERT_TRUE(db->unique_set(INDEX_MAIN, "FromRootKey2", "FromRootKey2Val3"));
    {
        auto d = db->unique_get(INDEX_MAIN, "FromRootKey2", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "FromRootKey2Val3");

        d = db->unique_get(INDEX_MAIN, "FromRootKey2", 1u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "romRootKey2Val3");

        d = db->unique_get(INDEX_MAIN, "FromRootKey2", 2u, 3);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "omR");
    }

    {
        auto d = db->exists(INDEX_MULTI, "FromRootKey2");
        ASSERT_TRUE(d);
    }

    {
        auto d = db->exists(INDEX_MAIN, "FromRootKey2");
        ASSERT_TRUE(d);
    }

    ASSERT_TRUE(db->erase(INDEX_MAIN, "FromRootKey2"));
    {
        auto d = db->unique_get(INDEX_MAIN, "FromRootKey2", 0u, boost::none);
        ASSERT_TRUE(!d.is_initialized());
    }
}

TEST(dbcache, testLowLevelDBTx)
{
    auto db = HierarchicalDB::Make("Root");

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val1"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(g, std::vector<std::string>({"FromRootKey1Val1"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val2"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1"),
                  std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(g, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val3"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "TheBigK", "FromRootKey2Val1"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1"}));

        const auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        const auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "TheBigK", "FromRootKey2Val2"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        const auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        const auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
    }

    ASSERT_TRUE(db->unique_set(INDEX_MAIN, "FromRootKey1", "FromRootKey1Val4"));
    {
        auto d = db->unique_get(INDEX_MAIN, "FromRootKey1", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "FromRootKey1Val4");

        d = db->unique_get(INDEX_MAIN, "FromRootKey1", 1u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "romRootKey1Val4");

        d = db->unique_get(INDEX_MAIN, "FromRootKey1", 2u, 3);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "omR");
    }

    ASSERT_TRUE(db->unique_set(INDEX_MAIN, "TheBigK", "FromRootKey2Val3"));
    {
        auto d = db->unique_get(INDEX_MAIN, "TheBigK", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "FromRootKey2Val3");

        d = db->unique_get(INDEX_MAIN, "TheBigK", 1u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "romRootKey2Val3");

        d = db->unique_get(INDEX_MAIN, "TheBigK", 2u, 3);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "omR");
    }

    {
        auto d = db->exists(INDEX_MULTI, "TheBigK");
        ASSERT_TRUE(d);
    }

    {
        auto d = db->exists(INDEX_MAIN, "TheBigK");
        ASSERT_TRUE(d);
    }

    auto tx1 = db->startDBTransaction("tx1");
    auto tx2 = db->startDBTransaction("tx2");

    ASSERT_TRUE(tx1->unique_set(INDEX_MAIN, "TheBigK", "TheBigKVal1"));
    {
        auto d = db->unique_get(INDEX_MAIN, "TheBigK", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "FromRootKey2Val3");

        d = tx1->unique_get(INDEX_MAIN, "TheBigK", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "TheBigKVal1");

        d = tx2->unique_get(INDEX_MAIN, "TheBigK", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "FromRootKey2Val3");
    }

    ASSERT_TRUE(tx2->unique_set(INDEX_MAIN, "TheBigK", "TheBigKVal2"));
    {
        auto d = db->unique_get(INDEX_MAIN, "TheBigK", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "FromRootKey2Val3");

        d = tx1->unique_get(INDEX_MAIN, "TheBigK", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "TheBigKVal1");

        d = tx2->unique_get(INDEX_MAIN, "TheBigK", 0u, boost::none);
        ASSERT_TRUE(d.is_initialized());
        ASSERT_EQ(*d, "TheBigKVal2");
    }
}

TEST(dbcache, testLowLevelDBTxMulti)
{
    auto db = HierarchicalDB::Make("Root");

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val1"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(g, std::vector<std::string>({"FromRootKey1Val1"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val2"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1"),
                  std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(g, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "FromRootKey1", "FromRootKey1Val3"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 1u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));

        const auto g = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "TheBigK", "FromRootKey2Val1"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1"}));

        const auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        const auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "TheBigK", "FromRootKey2Val2"));
    {
        const auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        const auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        const auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
    }

    auto tx1 = db->startDBTransaction("tx1");
    auto tx2 = db->startDBTransaction("tx2");

    ASSERT_TRUE(tx1->multi_append(INDEX_MULTI, "AnotherKey", "FromRootAndTx1Val1"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx2->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g1 = tx2->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx2->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
    }

    ASSERT_TRUE(tx2->multi_append(INDEX_MULTI, "TheBigK", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx2->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4"}));

        g1 = tx2->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx2->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4"}));
    }

    ASSERT_TRUE(tx2->multi_append(INDEX_MULTI, "TheBigK", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx2->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        g1 = tx2->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx2->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));
    }

    auto tx11 = tx1->startDBTransaction("tx11");

    ASSERT_TRUE(tx11->multi_append(INDEX_MULTI, "TheBigK", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        d = tx2->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        g1 = tx2->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx2->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));
    }

    auto tx12 = tx1->startDBTransaction("tx12");

    ASSERT_TRUE(tx12->multi_append(INDEX_MULTI, "TheBigK", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx2->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        g1 = tx2->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx2->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));
    }

    const auto commitTx1Res = tx1->commit();
    ASSERT_TRUE(commitTx1Res.isErr());
    ASSERT_EQ(commitTx1Res.UNWRAP_ERR(), HierarchicalDB::CommitError::UncommittedChildren);

    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx2->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        g1 = tx2->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx2->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));
    }

    ASSERT_TRUE(tx2->commit().isOk());

    // test isolation after we commit a transaction
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    auto tx3 = db->startDBTransaction("tx3");

    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        g3 = tx3->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    ASSERT_TRUE(db->multi_append(INDEX_MULTI, "TheBigK", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));
        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5"}));
        g3 = tx3->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    ASSERT_TRUE(tx3->multi_append(INDEX_MULTI, "TheBigK", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    auto tx31 = tx3->startDBTransaction("tx31");

    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));
        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g3 = tx3->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx31->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx31->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx31->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));
        g3 = tx31->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    auto tx32 = tx3->startDBTransaction("tx32");

    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        d = tx31->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx31->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx31->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        d = tx32->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx32->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx32->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    ASSERT_TRUE(tx31->multi_append(INDEX_MULTI, "TheBigK", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"));
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        d = tx31->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        g1 = tx31->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx31->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        d = tx32->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx32->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx32->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    ASSERT_TRUE(tx31->commit().isOk());
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        d = tx32->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        g1 = tx32->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx32->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    ASSERT_TRUE(tx32->commit().isOk());
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx12->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx12->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx12->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        g3 = tx12->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    ASSERT_TRUE(tx12->commit().isOk());
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        d = tx1->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));
        ASSERT_EQ(d.at("AnotherKey"), std::vector<std::string>({"FromRootAndTx1Val1"}));

        g1 = tx1->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx1->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        g3 = tx1->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx11->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"), std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                             "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g1 = tx11->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx11->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({"FromRootKey2Val1", "FromRootKey2Val2",
                                                "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val6"}));

        g3 = tx11->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));
    }

    tx11->cancel();
    ASSERT_TRUE(tx1->commit().isOk());
    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val7"}));

        auto g3 = db->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({"FromRootAndTx1Val1"}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        g3 = tx3->multi_getAllWithKey(INDEX_MULTI, "AnotherKey");
        ASSERT_EQ(g3, std::vector<std::string>({}));
    }

    auto tx4  = db->startDBTransaction("tx4");
    auto tx41 = tx4->startDBTransaction("tx41");

    ASSERT_TRUE(tx41->erase(INDEX_MULTI, "TheBigK"));

    ASSERT_TRUE(tx41->commit().isOk());
    ASSERT_TRUE(tx4->commit().isOk());

    {
        auto d = db->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_TRUE(d.find("TheBigK") == d.end());

        auto g1 = db->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        auto g2 = db->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2, std::vector<std::string>({}));

        d = tx3->multi_getAll(INDEX_MULTI);
        ASSERT_EQ(d.size(), 2u);
        ASSERT_EQ(d.at("FromRootKey1").size(), 3u);
        ASSERT_EQ(d.at("FromRootKey1"), std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2",
                                                                  "FromRootKey1Val3"}));
        ASSERT_EQ(d.at("TheBigK"),
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));

        g1 = tx3->multi_getAllWithKey(INDEX_MULTI, "FromRootKey1");
        ASSERT_EQ(
            g1, std::vector<std::string>({"FromRootKey1Val1", "FromRootKey1Val2", "FromRootKey1Val3"}));

        g2 = tx3->multi_getAllWithKey(INDEX_MULTI, "TheBigK");
        ASSERT_EQ(g2.size(), 6u);
        ASSERT_EQ(g2,
                  std::vector<std::string>(
                      {"FromRootKey2Val1", "FromRootKey2Val2", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val4",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val5", "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val8",
                       "FromRootAndTx1Tx2Tx11Tx12Tx3Tx31Val9"}));
    }

    const auto tx3CommitRes = tx3->commit();
    ASSERT_TRUE(tx3CommitRes.isErr());
    ASSERT_EQ(tx3CommitRes.UNWRAP_ERR(), HierarchicalDB::CommitError::Conflict);
}

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include "googletest/googletest/include/gtest/gtest.h"

#include "util.h"

static void
ResetArgs(const std::string& strArg)
{
    std::vector<std::string> vecArg;
    boost::split(vecArg, strArg, boost::is_space(), boost::token_compress_on);

    // Insert dummy executable name:
    vecArg.insert(vecArg.begin(), "testbitcoin");

    // Convert to char*:
    std::vector<const char*> vecChar;
    BOOST_FOREACH(std::string& s, vecArg)
        vecChar.push_back(s.c_str());

    ParseParameters(vecChar.size(), &vecChar[0]);
}

TEST(getarg_tests, boolarg)
{
    ResetArgs("-foo");
    EXPECT_TRUE(GetBoolArg("-foo"));
    EXPECT_TRUE(GetBoolArg("-foo", false));
    EXPECT_TRUE(GetBoolArg("-foo", true));

    EXPECT_TRUE(!GetBoolArg("-fo"));
    EXPECT_TRUE(!GetBoolArg("-fo", false));
    EXPECT_TRUE(GetBoolArg("-fo", true));

    EXPECT_TRUE(!GetBoolArg("-fooo"));
    EXPECT_TRUE(!GetBoolArg("-fooo", false));
    EXPECT_TRUE(GetBoolArg("-fooo", true));

    ResetArgs("-foo=0");
    EXPECT_TRUE(!GetBoolArg("-foo"));
    EXPECT_TRUE(!GetBoolArg("-foo", false));
    EXPECT_TRUE(!GetBoolArg("-foo", true));

    ResetArgs("-foo=1");
    EXPECT_TRUE(GetBoolArg("-foo"));
    EXPECT_TRUE(GetBoolArg("-foo", false));
    EXPECT_TRUE(GetBoolArg("-foo", true));

    // New 0.6 feature: auto-map -nosomething to !-something:
    ResetArgs("-nofoo");
    EXPECT_TRUE(!GetBoolArg("-foo"));
    EXPECT_TRUE(!GetBoolArg("-foo", false));
    EXPECT_TRUE(!GetBoolArg("-foo", true));

    ResetArgs("-nofoo=1");
    EXPECT_TRUE(!GetBoolArg("-foo"));
    EXPECT_TRUE(!GetBoolArg("-foo", false));
    EXPECT_TRUE(!GetBoolArg("-foo", true));

    ResetArgs("-foo -nofoo");  // -foo should win
    EXPECT_TRUE(GetBoolArg("-foo"));
    EXPECT_TRUE(GetBoolArg("-foo", false));
    EXPECT_TRUE(GetBoolArg("-foo", true));

    ResetArgs("-foo=1 -nofoo=1");  // -foo should win
    EXPECT_TRUE(GetBoolArg("-foo"));
    EXPECT_TRUE(GetBoolArg("-foo", false));
    EXPECT_TRUE(GetBoolArg("-foo", true));

    ResetArgs("-foo=0 -nofoo=0");  // -foo should win
    EXPECT_TRUE(!GetBoolArg("-foo"));
    EXPECT_TRUE(!GetBoolArg("-foo", false));
    EXPECT_TRUE(!GetBoolArg("-foo", true));

    // New 0.6 feature: treat -- same as -:
    ResetArgs("--foo=1");
    EXPECT_TRUE(GetBoolArg("-foo"));
    EXPECT_TRUE(GetBoolArg("-foo", false));
    EXPECT_TRUE(GetBoolArg("-foo", true));

    ResetArgs("--nofoo=1");
    EXPECT_TRUE(!GetBoolArg("-foo"));
    EXPECT_TRUE(!GetBoolArg("-foo", false));
    EXPECT_TRUE(!GetBoolArg("-foo", true));

}

TEST(getarg_tests, stringarg)
{
    ResetArgs("");
    EXPECT_EQ(GetArg("-foo", ""), "");
    EXPECT_EQ(GetArg("-foo", "eleven"), "eleven");

    ResetArgs("-foo -bar");
    EXPECT_EQ(GetArg("-foo", ""), "");
    EXPECT_EQ(GetArg("-foo", "eleven"), "");

    ResetArgs("-foo=");
    EXPECT_EQ(GetArg("-foo", ""), "");
    EXPECT_EQ(GetArg("-foo", "eleven"), "");

    ResetArgs("-foo=11");
    EXPECT_EQ(GetArg("-foo", ""), "11");
    EXPECT_EQ(GetArg("-foo", "eleven"), "11");

    ResetArgs("-foo=eleven");
    EXPECT_EQ(GetArg("-foo", ""), "eleven");
    EXPECT_EQ(GetArg("-foo", "eleven"), "eleven");

}

TEST(getarg_tests, intarg)
{
    ResetArgs("");
    EXPECT_EQ(GetArg("-foo", 11), 11);
    EXPECT_EQ(GetArg("-foo", 0), 0);

    ResetArgs("-foo -bar");
    EXPECT_EQ(GetArg("-foo", 11), 0);
    EXPECT_EQ(GetArg("-bar", 11), 0);

    ResetArgs("-foo=11 -bar=12");
    EXPECT_EQ(GetArg("-foo", 0), 11);
    EXPECT_EQ(GetArg("-bar", 11), 12);

    ResetArgs("-foo=NaN -bar=NotANumber");
    EXPECT_EQ(GetArg("-foo", 1), 0);
    EXPECT_EQ(GetArg("-bar", 11), 0);
}

TEST(getarg_tests, doubledash)
{
    ResetArgs("--foo");
    EXPECT_EQ(GetBoolArg("-foo"), true);

    ResetArgs("--foo=verbose --bar=1");
    EXPECT_EQ(GetArg("-foo", ""), "verbose");
    EXPECT_EQ(GetArg("-bar", 0), 1);
}

TEST(getarg_tests, boolargno)
{
    ResetArgs("-nofoo");
    EXPECT_TRUE(!GetBoolArg("-foo"));
    EXPECT_TRUE(!GetBoolArg("-foo", true));
    EXPECT_TRUE(!GetBoolArg("-foo", false));

    ResetArgs("-nofoo=1");
    EXPECT_TRUE(!GetBoolArg("-foo"));
    EXPECT_TRUE(!GetBoolArg("-foo", true));
    EXPECT_TRUE(!GetBoolArg("-foo", false));

    ResetArgs("-nofoo=0");
    EXPECT_TRUE(GetBoolArg("-foo"));
    EXPECT_TRUE(GetBoolArg("-foo", true));
    EXPECT_TRUE(GetBoolArg("-foo", false));

    ResetArgs("-foo --nofoo");
    EXPECT_TRUE(GetBoolArg("-foo"));

    ResetArgs("-nofoo -foo"); // foo always wins:
    EXPECT_TRUE(GetBoolArg("-foo"));
}

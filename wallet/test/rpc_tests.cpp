#include "googletest/googletest/include/gtest/gtest.h"
#include <boost/foreach.hpp>

#include "base58.h"
#include "util.h"
#include "bitcoinrpc.h"

using namespace std;
using namespace json_spirit;

TEST(rpc_tests, ValueFromAmount)
{
    EXPECT_EQ(ValueFromAmount(399999999).get_real(), 3.99999999);
}

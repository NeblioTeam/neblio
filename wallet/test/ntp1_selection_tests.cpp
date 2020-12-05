#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "mocks/mntp1wallet.h"
#include "ntp1/ntp1wallet.h"

using namespace ::testing;

struct NTP1TokenSelectionTest : public ::testing::Test
{
    NTP1WalletMockPtr wallet;

    void SetUp() override
    {

        wallet = boost::make_shared<NiceMock<mNTP1Wallet>>();
        ON_CALL(*wallet, getTokenName(_)).WillByDefault(Return("MyName!"));
    }
    void TearDown() override {}
};

TEST_F(NTP1TokenSelectionTest, basic) { EXPECT_EQ(wallet->getTokenName(""), "MyName!"); }

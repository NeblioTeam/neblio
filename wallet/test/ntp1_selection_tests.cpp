#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "mocks/mntp1wallet.h"
#include "ntp1/ntp1wallet.h"

using namespace ::testing;

struct NTP1TokenSelectionTest : public ::testing::Test
{
    NTP1WalletMockPtr wallet;

    std::map<std::string, NTP1Int>                    walletBalances;
    std::unordered_map<NTP1OutPoint, NTP1Transaction> walletOutputs;

    void SetUp() override
    {
        wallet = boost::make_shared<NiceMock<mNTP1Wallet>>();
        ON_CALL(*wallet, getTokenName(_)).WillByDefault(Return("MyName!"));
        ON_CALL(*wallet, getBalancesMap())
            .WillByDefault(
                Invoke([&]() -> const std::map<std::string, NTP1Int>& { return walletBalances; }));
        ON_CALL(*wallet, getWalletOutputsWithTokens())
            .WillByDefault(Invoke([&]() -> const std::unordered_map<NTP1OutPoint, NTP1Transaction>& {
                return walletOutputs;
            }));
    }
    void TearDown() override {}
};

TEST_F(NTP1TokenSelectionTest, basic)
{
    NTP1OutPoint s(123, 1);
    CTransaction tx;
    //    tx.vout.push_back(CTxOut(100*COIN, ));

    EXPECT_EQ(wallet->getTokenName(""), "MyName!");
    //
}

#include "gmock/gmock.h"

#include "ntp1/intp1wallet.h"

struct mNTP1Wallet : public INTP1Wallet
{
    MOCK_METHOD(void, update, (), (override));
    MOCK_METHOD(std::string, getTokenName, (const std::string& tokenID), (const, override));
    MOCK_METHOD(NTP1Int, getTokenBalance, (const std::string& tokenID), (const, override));
    MOCK_METHOD(std::string, getTokenNameFromIndex, (int index), (const, override));
    MOCK_METHOD(std::string, getTokenID, (int index), (const, override));
    MOCK_METHOD(std::string, getTokenIssuanceTxid, (int index), (const, override));
    MOCK_METHOD(std::string, getTokenDescription, (int index), (const, override));
    MOCK_METHOD(NTP1Int, getTokenBalance, (int index), (const, override));
    MOCK_METHOD(std::string, getAndCacheTokenIcon, (int index), (override));
    MOCK_METHOD(int64_t, getNumberOfTokens, (), (const, override));
    MOCK_METHOD((const std::map<std::string, NTP1Int>&), getBalancesMap, (), (const, override));
    MOCK_METHOD((const std::unordered_map<NTP1OutPoint, NTP1Transaction>&), getWalletOutputsWithTokens,
                (), (const, override));
    MOCK_METHOD(void, clear, (), (override));
    MOCK_METHOD(void, setMinMaxConfirmations, (int minConfs, int maxConfs), (override));
    MOCK_METHOD((std::map<std::string, NTP1Int>), getBalances, (), (const, override));
    MOCK_METHOD(bool, getRetrieveFullMetadata, (), (const, override));
    MOCK_METHOD(void, setRetrieveFullMetadata, (bool), (override));
    MOCK_METHOD(void, exportToFile, (const boost::filesystem::path& filePath), (const, override));
    MOCK_METHOD(void, importFromFile, (const boost::filesystem::path& filePath), (override));
    MOCK_METHOD(void, setTokenIcon, (const std::string& tokenID, const std::string& iconData),
                (override));
};

using NTP1WalletMockPtr = boost::shared_ptr<::testing::NiceMock<mNTP1Wallet>>;

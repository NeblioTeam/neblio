#include "googletest/googletest/include/gtest/gtest.h"

#include "base58.h"
#include "block.h"
#include "stakemaker.h"
#include "test/mocks/mtxdb.h"
#include "wallet.h"

class PoS_CollectInputsTestFixture : public ::testing::Test
{
protected:
    static CScript AddressToScriptPubKey(const CBitcoinAddress& address)
    {
        CScript result;
        result.SetDestination(address.Get());
        return result;
    }

    static CScript KeyToP2PK(const CKey& key)
    {
        return CScript() << key.GetPubKey() << OP_CHECKSIG; // P2PK
    }

    static CScript KeyToP2PKH(const CKey& key)
    {
        return AddressToScriptPubKey(CBitcoinAddress(key.GetPubKey().GetID()));
    }

public:
    CAmount balance = 0;
    CKey    key1, key2;
    CKey    stakePayee;

    std::set<std::pair<const CWalletTx*, unsigned int>> availableCoins;

    virtual void SetUp()
    {
        SelectParams(NetworkType::Regtest);

        key1.MakeNewKey(true);
        key2.MakeNewKey(true);

        add_coin(50 * COIN, KeyToP2PKH(key1), GetAdjustedTime() - 24 * 60 * 60 * 10);
        add_coin(25 * COIN, KeyToP2PKH(key1), GetAdjustedTime() - 24 * 60 * 60 * 10);
        add_coin(25 * COIN, KeyToP2PK(key2), GetAdjustedTime() - 24 * 60 * 60 * 10);
        add_coin(10 * COIN, KeyToP2PK(key2), GetAdjustedTime() - 24 * 60 * 60 * 10);
        add_coin(5 * COIN, KeyToP2PK(key2), GetAdjustedTime() - 24 * 60 * 60 * 10);

        CAmount nValueRet = 0;

        boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
        EXPECT_CALL(*dbMock, GetBestChainHeight())
            .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

        ASSERT_TRUE(wallet.SelectCoinsMinConf(*dbMock, 115 * COIN, GetAdjustedTime(), 1, 6, vCoins,
                                              availableCoins, nValueRet, false));
        EXPECT_EQ(nValueRet, 115 * COIN);

        balance = nValueRet;

        stakePayee.MakeNewKey(true);
    }
    virtual void TearDown() {}

    typedef std::set<std::pair<const CWalletTx*, unsigned int>> CoinSet;

    CWallet              wallet;
    std::vector<COutput> vCoins;

    void empty_wallet(void)
    {
        for (COutput output : vCoins)
            delete output.tx;
        vCoins.clear();
    }

    void add_coin(int64_t nValue, const CScript& scriptPubKey, int64_t nTime, int nDepth = 6 * 24,
                  bool fIsFromMe = false, int nInput = 0)
    {
        static int   i;
        CTransaction tx;
        tx.nLockTime = i++; // so all transactions get different hashes
        tx.nTime     = nTime;
        tx.vout.resize(nInput + 1);
        tx.vout[nInput].nValue       = nValue;
        tx.vout[nInput].scriptPubKey = scriptPubKey;
        CWalletTx* wtx               = new CWalletTx(&wallet, tx);
        if (fIsFromMe) {
            // IsFromMe() returns (GetDebit() > 0), and GetDebit() is 0 if vin.empty(),
            // so stop vin being empty, and cache a non-zero Debit to fake out IsFromMe()
            wtx->vin.resize(1);
            wtx->c_DebitCached = 1;
        }
        COutput output(wtx, nInput, nDepth);
        vCoins.push_back(output);
    }
};

TEST(PoS_tests, kernel_scriptPubKey_basic_p2pkh)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript;
    kernelScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get()); // P2PKH

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(*dbMock, kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEYHASH);

    boost::optional<CScript> calcResult = StakeMaker::CalculateScriptPubKeyForStakeOutput(
        *dbMock, StakeMaker::DefaultKeyGetter(keyStore), kernelScript);
    ASSERT_NE(calcResult, boost::none);

    EXPECT_EQ(calcResult, CScript() << key.GetPubKey() << OP_CHECKSIG);
}

TEST(PoS_tests, kernel_scriptPubKey_basic_p2pk)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript = CScript() << key.GetPubKey() << OP_CHECKSIG; // P2PK

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(*dbMock, kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEY);

    boost::optional<CScript> calcResult = StakeMaker::CalculateScriptPubKeyForStakeOutput(
        *dbMock, StakeMaker::DefaultKeyGetter(keyStore), kernelScript);
    ASSERT_NE(calcResult, boost::none);

    EXPECT_EQ(calcResult, CScript() << key.GetPubKey() << OP_CHECKSIG);
}

TEST(PoS_tests, kernel_scriptPubKey_basic_p2cs)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey keyOwner;
    CKey keyStaker;
    keyOwner.MakeNewKey(true);
    keyStaker.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(keyStaker));

    // create the kernel script
    CScript kernelScript =
        GetScriptForStakeDelegation(keyStaker.GetPubKey().GetID(), keyOwner.GetPubKey().GetID()); // P2CS

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(*dbMock, kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_COLDSTAKE);

    boost::optional<CScript> calcResult = StakeMaker::CalculateScriptPubKeyForStakeOutput(
        *dbMock, StakeMaker::DefaultKeyGetter(keyStore), kernelScript);
    ASSERT_NE(calcResult, boost::none);

    EXPECT_EQ(calcResult, kernelScript);
}

TEST(PoS_tests, kernel_scriptPubKey_basic_p2cs__staker_key_does_not_exist_in_keystore)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey keyOwner;
    CKey keyStaker;
    keyOwner.MakeNewKey(true);
    keyStaker.MakeNewKey(true);

    // add the key to key store
    // EXPECT_TRUE(keyStore.AddKey(keyStaker));

    // create the kernel script
    CScript kernelScript =
        GetScriptForStakeDelegation(keyStaker.GetPubKey().GetID(), keyOwner.GetPubKey().GetID()); // P2CS

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(*dbMock, kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_COLDSTAKE);

    boost::optional<CScript> calcResult = StakeMaker::CalculateScriptPubKeyForStakeOutput(
        *dbMock, StakeMaker::DefaultKeyGetter(keyStore), kernelScript);
    ASSERT_EQ(calcResult, boost::none);
}

TEST(PoS_tests, kernel_scriptPubKey_p2pkh__key_does_not_exist_in_keystore)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    // EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript;
    kernelScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get());

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(*dbMock, kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEYHASH);

    boost::optional<CScript> calcResult = StakeMaker::CalculateScriptPubKeyForStakeOutput(
        *dbMock, StakeMaker::DefaultKeyGetter(keyStore), kernelScript);
    ASSERT_EQ(calcResult, boost::none);
}

TEST(PoS_tests, kernel_scriptPubKey_p2pk__key_does_not_exist_in_keystore)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    // EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript = CScript() << key.GetPubKey() << OP_CHECKSIG; // P2PK

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(*dbMock, kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEY);

    boost::optional<CScript> calcResult = StakeMaker::CalculateScriptPubKeyForStakeOutput(
        *dbMock, StakeMaker::DefaultKeyGetter(keyStore), kernelScript);
    ASSERT_EQ(calcResult, boost::none);
}

TEST(PoS_tests, kernel_scriptPubKey_unsolvable)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    // We add something to P2PK to make it invalid
    CScript kernelScript = CScript() << key.GetPubKey() << OP_CHECKSIG << OP_HASH160;

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_FALSE(Solver(*dbMock, kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_NONSTANDARD);

    boost::optional<CScript> calcResult = StakeMaker::CalculateScriptPubKeyForStakeOutput(
        *dbMock, StakeMaker::DefaultKeyGetter(keyStore), kernelScript);
    ASSERT_EQ(calcResult, boost::none);
}

TEST(PoS_tests, output_creation_with_split)
{
    SelectParams(NetworkType::Regtest);

    // make key
    CKey key;
    key.MakeNewKey(true);

    // create the kernel script
    CScript outputScript;
    outputScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get()); // P2PKH

    StakeKernelData data;

    CAmount amount = 10 * COIN + 500;

    std::vector<CTxOut> outputs = StakeMaker::MakeStakeOutputs(outputScript, amount, true);
    ASSERT_EQ(outputs.size(), 3u); // marker + output + output
    EXPECT_EQ(outputs[0].nValue, 0);
    EXPECT_TRUE(outputs[0].scriptPubKey.empty());
    EXPECT_EQ(outputs[0].scriptPubKey, CScript());
    EXPECT_EQ(outputs[1].nValue + outputs[2].nValue, amount);
    EXPECT_EQ(outputs[1].scriptPubKey, outputScript);
    EXPECT_EQ(outputs[2].scriptPubKey, outputScript);

    // both outputs are greather than zero
    EXPECT_GT(outputs[1].nValue, 0);
    EXPECT_GT(outputs[2].nValue, 0);
}

TEST(PoS_tests, output_creation_no_split)
{
    SelectParams(NetworkType::Regtest);

    // make key
    CKey key;
    key.MakeNewKey(true);

    // create the kernel script
    CScript outputScript;
    outputScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get()); // P2PKH

    StakeKernelData data;

    CAmount amount = 10 * COIN + 500;

    std::vector<CTxOut> outputs = StakeMaker::MakeStakeOutputs(outputScript, amount, false);
    ASSERT_EQ(outputs.size(), 2u); // marker + output
    EXPECT_EQ(outputs[0].nValue, 0);
    EXPECT_TRUE(outputs[0].scriptPubKey.empty());
    EXPECT_EQ(outputs[0].scriptPubKey, CScript());
    EXPECT_EQ(outputs[1].nValue, amount);
    EXPECT_EQ(outputs[1].scriptPubKey, outputScript);
}

TEST_F(PoS_CollectInputsTestFixture, collecting_inputs_no_split)
{
    EXPECT_EQ(vCoins[0].tx->vout[0].scriptPubKey, KeyToP2PKH(key1));

    const unsigned coinIndex   = 0;
    const unsigned outputIndex = 0;

    StakeKernelData kernelData;
    kernelData.key.MakeNewKey(true);
    kernelData.credit                  = vCoins[coinIndex].tx->vout[outputIndex].nValue;
    kernelData.kernelTx                = vCoins[coinIndex].tx;
    kernelData.kernelInput             = CTxIn(vCoins[coinIndex].tx->GetHash(), 0);
    kernelData.stakeTxTime             = GetAdjustedTime() - 60 * 60 * 24 * 5; // 5 days
    kernelData.kernelBlockTime         = kernelData.stakeTxTime - 60;
    kernelData.kernelScriptPubKey      = vCoins[coinIndex].tx->vout[outputIndex].scriptPubKey;
    kernelData.stakeOutputScriptPubKey = CScript()
                                         << stakePayee.GetPubKey() << OP_CHECKSIG << OP_HASH160;

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    CoinStakeInputsResult inputsResult = StakeMaker::CollectInputsForStake(
        *dbMock, kernelData, availableCoins, GetAdjustedTime(), false, balance, 0);
    EXPECT_EQ(inputsResult.inputs.size(), 2u);
    EXPECT_EQ(inputsResult.inputsPrevouts.size(), 2u);

    // now do it again with reserve, ensure the

    inputsResult = StakeMaker::CollectInputsForStake(*dbMock, kernelData, availableCoins,
                                                     GetAdjustedTime(), false, balance, balance);

    // only the kernel will go through (since it's checked elsewhere when finding the stake)
    EXPECT_EQ(inputsResult.inputs.size(), 1u);
    EXPECT_EQ(inputsResult.inputsPrevouts.size(), 1u);
}

TEST_F(PoS_CollectInputsTestFixture, collecting_inputs_with_split)
{
    EXPECT_EQ(vCoins[0].tx->vout[0].scriptPubKey, KeyToP2PKH(key1));

    const unsigned coinIndex   = 0;
    const unsigned outputIndex = 0;

    StakeKernelData kernelData;
    kernelData.key.MakeNewKey(true);
    kernelData.credit                  = vCoins[coinIndex].tx->vout[outputIndex].nValue;
    kernelData.kernelTx                = vCoins[coinIndex].tx;
    kernelData.kernelInput             = CTxIn(vCoins[coinIndex].tx->GetHash(), 0);
    kernelData.stakeTxTime             = GetAdjustedTime() - 60 * 60 * 24 * 5; // 5 days
    kernelData.kernelBlockTime         = kernelData.stakeTxTime - 60;
    kernelData.kernelScriptPubKey      = vCoins[coinIndex].tx->vout[outputIndex].scriptPubKey;
    kernelData.stakeOutputScriptPubKey = CScript()
                                         << stakePayee.GetPubKey() << OP_CHECKSIG << OP_HASH160;

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    CoinStakeInputsResult inputsResult = StakeMaker::CollectInputsForStake(
        *dbMock, kernelData, availableCoins, GetAdjustedTime(), true, balance, 0);
    EXPECT_EQ(inputsResult.inputs.size(), 1u);
    EXPECT_EQ(inputsResult.inputsPrevouts.size(), 1u);
}

TEST_F(PoS_CollectInputsTestFixture, collecting_inputs_max_inputs)
{
    ASSERT_EQ(Params().NetType(), NetworkType::Regtest);
    EXPECT_EQ(Params().MaxInputsInStake(), 10u);
    EXPECT_EQ(vCoins[0].tx->vout[0].scriptPubKey, KeyToP2PKH(key1));

    empty_wallet();

    // create many inputs and add them
    for (int i = 0; i < 20; i++) {
        add_coin(50 * COIN, KeyToP2PKH(key1), GetAdjustedTime() - 24 * 60 * 60 * 10);
    }

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    CAmount nValueRet = 0;
    ASSERT_TRUE(wallet.SelectCoinsMinConf(*dbMock, 20 * 50 * COIN, GetAdjustedTime(), 1, 6, vCoins,
                                          availableCoins, nValueRet, false));
    balance = nValueRet;

    const unsigned coinIndex   = 0;
    const unsigned outputIndex = 0;

    StakeKernelData kernelData;
    kernelData.key.MakeNewKey(true);
    kernelData.credit                  = vCoins[coinIndex].tx->vout[outputIndex].nValue;
    kernelData.kernelTx                = vCoins[coinIndex].tx;
    kernelData.kernelInput             = CTxIn(vCoins[coinIndex].tx->GetHash(), 0);
    kernelData.stakeTxTime             = GetAdjustedTime() - 60 * 60 * 24 * 5; // 5 days
    kernelData.kernelBlockTime         = kernelData.stakeTxTime - 60;
    kernelData.kernelScriptPubKey      = vCoins[coinIndex].tx->vout[outputIndex].scriptPubKey;
    kernelData.stakeOutputScriptPubKey = CScript()
                                         << stakePayee.GetPubKey() << OP_CHECKSIG << OP_HASH160;

    CoinStakeInputsResult inputsResult = StakeMaker::CollectInputsForStake(
        *dbMock, kernelData, availableCoins, GetAdjustedTime(), false, balance, 0);

    // we cannot have more than 10 inputs as per Params().MaxInputsInStake()
    EXPECT_EQ(inputsResult.inputs.size(), 10u);
    EXPECT_EQ(inputsResult.inputsPrevouts.size(), 10u);
}

TEST_F(PoS_CollectInputsTestFixture, collecting_inputs_max_value)
{
    ASSERT_EQ(Params().NetType(), NetworkType::Regtest);
    EXPECT_EQ(Params().MaxInputsInStake(), 10u);
    EXPECT_EQ(vCoins[0].tx->vout[0].scriptPubKey, KeyToP2PKH(key1));

    empty_wallet();

    // create many inputs and add them
    for (int i = 0; i < 20; i++) {
        add_coin(500 * COIN, KeyToP2PKH(key1), GetAdjustedTime() - 24 * 60 * 60 * 10);
    }

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    CAmount nValueRet = 0;
    ASSERT_TRUE(wallet.SelectCoinsMinConf(*dbMock, (20 * 500) * COIN, GetAdjustedTime(), 1, 6, vCoins,
                                          availableCoins, nValueRet, false));
    balance = nValueRet;

    const unsigned coinIndex   = 0;
    const unsigned outputIndex = 0;

    StakeKernelData kernelData;
    kernelData.key.MakeNewKey(true);
    kernelData.credit                  = vCoins[coinIndex].tx->vout[outputIndex].nValue;
    kernelData.kernelTx                = vCoins[coinIndex].tx;
    kernelData.kernelInput             = CTxIn(vCoins[coinIndex].tx->GetHash(), 0);
    kernelData.stakeTxTime             = GetAdjustedTime() - 60 * 60 * 24 * 5; // 5 days
    kernelData.kernelBlockTime         = kernelData.stakeTxTime - 60;
    kernelData.kernelScriptPubKey      = vCoins[coinIndex].tx->vout[outputIndex].scriptPubKey;
    kernelData.stakeOutputScriptPubKey = CScript()
                                         << stakePayee.GetPubKey() << OP_CHECKSIG << OP_HASH160;

    CoinStakeInputsResult inputsResult = StakeMaker::CollectInputsForStake(
        *dbMock, kernelData, availableCoins, GetAdjustedTime(), false, balance, 0);

    // we cannot have more than 2 inputs, since the max is 1000
    EXPECT_EQ(inputsResult.inputs.size(), 2u);
    EXPECT_EQ(inputsResult.inputsPrevouts.size(), 2u);
}

TEST_F(PoS_CollectInputsTestFixture, collecting_inputs_max_too_small_age)
{
    ASSERT_EQ(Params().NetType(), NetworkType::Regtest);
    EXPECT_EQ(Params().MaxInputsInStake(), 10u);
    EXPECT_EQ(vCoins[0].tx->vout[0].scriptPubKey, KeyToP2PKH(key1));

    empty_wallet();

    // create many inputs and add them
    for (int i = 0; i < 20; i++) {
        add_coin(500 * COIN, KeyToP2PKH(key1), GetAdjustedTime());
    }

    boost::shared_ptr<mTxDB> dbMock = boost::make_shared<mTxDB>();
    EXPECT_CALL(*dbMock, GetBestChainHeight())
        .WillRepeatedly(testing::Return(boost::make_optional<int>(0)));

    CAmount nValueRet = 0;
    ASSERT_TRUE(wallet.SelectCoinsMinConf(*dbMock, (20 * 500) * COIN, GetAdjustedTime(), 1, 6, vCoins,
                                          availableCoins, nValueRet, false));
    balance = nValueRet;

    const unsigned coinIndex   = 0;
    const unsigned outputIndex = 0;

    StakeKernelData kernelData;
    kernelData.key.MakeNewKey(true);
    kernelData.credit                  = vCoins[coinIndex].tx->vout[outputIndex].nValue;
    kernelData.kernelTx                = vCoins[coinIndex].tx;
    kernelData.kernelInput             = CTxIn(vCoins[coinIndex].tx->GetHash(), 0);
    kernelData.stakeTxTime             = GetAdjustedTime();
    kernelData.kernelBlockTime         = kernelData.stakeTxTime - 60;
    kernelData.kernelScriptPubKey      = vCoins[coinIndex].tx->vout[outputIndex].scriptPubKey;
    kernelData.stakeOutputScriptPubKey = CScript()
                                         << stakePayee.GetPubKey() << OP_CHECKSIG << OP_HASH160;

    CoinStakeInputsResult inputsResult = StakeMaker::CollectInputsForStake(
        *dbMock, kernelData, availableCoins, GetAdjustedTime(), false, balance, 0);

    // only the kernel will go in, because all other UTXOs' nTime is equal to current tx's time
    EXPECT_EQ(inputsResult.inputs.size(), 1u);
    EXPECT_EQ(inputsResult.inputsPrevouts.size(), 1u);

    // now we call again, but we change the transaction time to make inputs feasible, and we'll get 2
    // again (the max that we can get due to max value in a stake)
    inputsResult = StakeMaker::CollectInputsForStake(
        *dbMock, kernelData, availableCoins, GetAdjustedTime() + Params().StakeMinAge(*dbMock) + 60 * 60,
        false, balance, 0);

    EXPECT_EQ(inputsResult.inputs.size(), 2u);
    EXPECT_EQ(inputsResult.inputsPrevouts.size(), 2u);
}

extern bool Sign1(const CKeyID& address, const CKeyStore& keystore, uint256 hash, int nHashType,
                  CScript& scriptSigRet);

TEST(PoS_tests, scripts_p2cs_scriptSig_test)
{
    SelectParams(NetworkType::Regtest);

    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(key));

    // hypothetical signature
    const CScript sigScript = [&key, &keyStore]() -> CScript {
        CScript sigScript;

        CHashWriter ss(SER_GETHASH, 0);
        ss << std::string("abc");
        EXPECT_TRUE(Sign1(key.GetPubKey().GetID(), keyStore, ss.GetHash(), SIGHASH_ALL, sigScript));

        return sigScript;
    }();

    const std::vector<uint8_t> pubKey = key.GetPubKey().Raw();

    {
        CScript sig = sigScript;

        // this should pass
        CScript script = sig << (int)OP_TRUE << pubKey;

        const auto pubKeyReturned = script.GetPubKeyOfP2CSScriptSig();
        EXPECT_EQ(pubKeyReturned, pubKey);
    }
    {
        CScript sig = sigScript;

        // OP_FALSE means it's a delegation revocation, so no
        CScript script = sig << (int)OP_FALSE << pubKey;

        const auto pubKeyReturned = script.GetPubKeyOfP2CSScriptSig();
        EXPECT_EQ(pubKeyReturned, boost::none);
    }
    {
        CScript sig = sigScript;

        // empty script should fail
        CScript script = CScript();

        const auto pubKeyReturned = script.GetPubKeyOfP2CSScriptSig();
        EXPECT_EQ(pubKeyReturned, boost::none);
    }
    {
        CScript sig = sigScript;

        // only sig should fail
        CScript script = sig;

        const auto pubKeyReturned = script.GetPubKeyOfP2CSScriptSig();
        EXPECT_EQ(pubKeyReturned, boost::none);
    }
    {
        CScript sig = sigScript;

        // missing public key should fail
        CScript script = sig << (int)OP_TRUE;

        const auto pubKeyReturned = script.GetPubKeyOfP2CSScriptSig();
        EXPECT_EQ(pubKeyReturned, boost::none);
    }
    {
        CScript sig = sigScript;

        // anything after pubkey should make it fail
        CScript script = sig << (int)OP_TRUE << pubKey << OP_FALSE;

        const auto pubKeyReturned = script.GetPubKeyOfP2CSScriptSig();
        EXPECT_EQ(pubKeyReturned, boost::none);
    }
    {
        CScript sig = sigScript;

        // anything after pubkey should make it fail
        CScript script = sig << (int)OP_TRUE << pubKey << 0x0;

        const auto pubKeyReturned = script.GetPubKeyOfP2CSScriptSig();
        EXPECT_EQ(pubKeyReturned, boost::none);
    }
}

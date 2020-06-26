#include "googletest/googletest/include/gtest/gtest.h"

#include "base58.h"
#include "stakemaker.h"

TEST(pos_tests, kernel_scriptPubKey_basic_p2pkh)
{
    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript;
    kernelScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get()); // P2PKH

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEYHASH);

    boost::optional<KernelScriptPubKeyResult> calcResult =
        StakeMaker::CalculateScriptPubKeyForStakeOutput(keyStore, kernelScript);
    ASSERT_NE(calcResult, boost::none);

    EXPECT_EQ(calcResult->key.GetPubKey(), key.GetPubKey());
    EXPECT_EQ(calcResult->scriptPubKey, CScript() << key.GetPubKey() << OP_CHECKSIG);
}

TEST(pos_tests, kernel_scriptPubKey_basic_p2pk)
{
    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript = CScript() << key.GetPubKey() << OP_CHECKSIG; // P2PK

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEY);

    boost::optional<KernelScriptPubKeyResult> calcResult =
        StakeMaker::CalculateScriptPubKeyForStakeOutput(keyStore, kernelScript);
    ASSERT_NE(calcResult, boost::none);

    EXPECT_EQ(calcResult->key.GetPubKey(), key.GetPubKey());
    EXPECT_EQ(calcResult->scriptPubKey, CScript() << key.GetPubKey() << OP_CHECKSIG);
}

TEST(pos_tests, kernel_scriptPubKey_p2pkh_key_does_not_exist_in_keystore)
{
    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    // EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript;
    kernelScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get());

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEYHASH);

    boost::optional<KernelScriptPubKeyResult> calcResult =
        StakeMaker::CalculateScriptPubKeyForStakeOutput(keyStore, kernelScript);
    ASSERT_EQ(calcResult, boost::none);
}

TEST(pos_tests, kernel_scriptPubKey_p2pk_key_does_not_exist_in_keystore)
{
    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    // EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    CScript kernelScript = CScript() << key.GetPubKey() << OP_CHECKSIG; // P2PK

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_TRUE(Solver(kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_PUBKEY);

    boost::optional<KernelScriptPubKeyResult> calcResult =
        StakeMaker::CalculateScriptPubKeyForStakeOutput(keyStore, kernelScript);
    ASSERT_EQ(calcResult, boost::none);
}

TEST(pos_tests, kernel_scriptPubKey_unsolvable)
{
    CBasicKeyStore keyStore;

    // make key
    CKey key;
    key.MakeNewKey(true);

    // add the key to key store
    EXPECT_TRUE(keyStore.AddKey(key));

    // create the kernel script
    // We add something to P2PK to make it invalid
    CScript kernelScript = CScript() << key.GetPubKey() << OP_CHECKSIG << OP_HASH160;

    // solve for the kernel script to ensure it's sane
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    EXPECT_FALSE(Solver(kernelScript, whichType, vSolutions));
    EXPECT_EQ(whichType, txnouttype::TX_NONSTANDARD);

    boost::optional<KernelScriptPubKeyResult> calcResult =
        StakeMaker::CalculateScriptPubKeyForStakeOutput(keyStore, kernelScript);
    ASSERT_EQ(calcResult, boost::none);
}

TEST(pos_tests, output_creation_with_split)
{
    // make key
    CKey key;
    key.MakeNewKey(true);

    // create the kernel script
    CScript outputScript;
    outputScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get()); // P2PKH

    StakeKernelData data;

    CAmount amount = 10 * COIN + 500;

    std::vector<CTxOut> outputs = StakeMaker::MakeStakeOutputs(outputScript, amount, true);
    ASSERT_EQ(outputs.size(), 3); // marker + output + output
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

TEST(pos_tests, output_creation_no_split)
{
    // make key
    CKey key;
    key.MakeNewKey(true);

    // create the kernel script
    CScript outputScript;
    outputScript.SetDestination(CBitcoinAddress(key.GetPubKey().GetID()).Get()); // P2PKH

    StakeKernelData data;

    CAmount amount = 10 * COIN + 500;

    std::vector<CTxOut> outputs = StakeMaker::MakeStakeOutputs(outputScript, amount, false);
    ASSERT_EQ(outputs.size(), 2); // marker + output
    EXPECT_EQ(outputs[0].nValue, 0);
    EXPECT_TRUE(outputs[0].scriptPubKey.empty());
    EXPECT_EQ(outputs[0].scriptPubKey, CScript());
    EXPECT_EQ(outputs[1].nValue, amount);
    EXPECT_EQ(outputs[1].scriptPubKey, outputScript);
}

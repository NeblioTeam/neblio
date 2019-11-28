#include "googletest/googletest/include/gtest/gtest.h"
#include "json/json_spirit_writer_template.h"
#include <map>
#include <string>

#include "NetworkForks.h"
#include "main.h"
#include "wallet.h"

using namespace std;
using namespace json_spirit;

// In script_tests.cpp
extern Array   read_json(const std::string& filename);
extern CScript ParseScript(string s);

TEST(transaction_tests, tx_valid)
{
    // Read tests from test/data/tx_valid.json
    // Format is an array of arrays
    // Inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptPubKey], [input 2], ...],"],
    // serializedTransaction, enforceP2SH
    // ... where all scripts are stringified scripts.
    Array tests = read_json("tx_valid.json");

    BOOST_FOREACH (Value& tv, tests) {
        Array  test    = tv.get_array();
        string strTest = write_string(tv, false);
        if (test[0].type() == array_type) {
            if (test.size() != 3 || test[1].type() != str_type || test[2].type() != bool_type) {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            map<COutPoint, CScript> mapprevOutScriptPubKeys;
            Array                   inputs = test[0].get_array();
            bool                    fValid = true;
            BOOST_FOREACH (Value& input, inputs) {
                if (input.type() != array_type) {
                    fValid = false;
                    break;
                }
                Array vinput = input.get_array();
                if (vinput.size() != 3) {
                    fValid = false;
                    break;
                }

                mapprevOutScriptPubKeys[COutPoint(uint256(vinput[0].get_str()), vinput[1].get_int())] =
                    ParseScript(vinput[2].get_str());
            }
            if (!fValid) {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            string       transaction = test[1].get_str();
            CDataStream  stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
            CTransaction tx;
            stream >> tx;

            EXPECT_TRUE(tx.CheckTransaction()) << strTest;

            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                if (!mapprevOutScriptPubKeys.count(tx.vin[i].prevout)) {
                    ADD_FAILURE() << "Bad test: " << strTest;
                    break;
                }
                EXPECT_TRUE(VerifyScript(tx.vin[i].scriptSig, mapprevOutScriptPubKeys[tx.vin[i].prevout],
                                         tx, i, test[2].get_bool(), false, 0))
                    << strTest;
            }
        }
    }
}

TEST(transaction_tests, tx_invalid)
{
    // Read tests from test/data/tx_invalid.json
    // Format is an array of arrays
    // Inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptPubKey], [input 2], ...],"],
    // serializedTransaction, enforceP2SH
    // ... where all scripts are stringified scripts.
    Array tests = read_json("tx_invalid.json");

    BOOST_FOREACH (Value& tv, tests) {
        Array  test    = tv.get_array();
        string strTest = write_string(tv, false);
        if (test[0].type() == array_type) {
            if (test.size() != 3 || test[1].type() != str_type || test[2].type() != bool_type) {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            map<COutPoint, CScript> mapprevOutScriptPubKeys;
            Array                   inputs = test[0].get_array();
            bool                    fValid = true;
            BOOST_FOREACH (Value& input, inputs) {
                if (input.type() != array_type) {
                    fValid = false;
                    break;
                }
                Array vinput = input.get_array();
                if (vinput.size() != 3) {
                    fValid = false;
                    break;
                }

                mapprevOutScriptPubKeys[COutPoint(uint256(vinput[0].get_str()), vinput[1].get_int())] =
                    ParseScript(vinput[2].get_str());
            }
            if (!fValid) {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            string       transaction = test[1].get_str();
            CDataStream  stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
            CTransaction tx;
            stream >> tx;

            fValid = tx.CheckTransaction();

            for (unsigned int i = 0; i < tx.vin.size() && fValid; i++) {
                if (!mapprevOutScriptPubKeys.count(tx.vin[i].prevout)) {
                    ADD_FAILURE() << "Bad test: " << strTest;
                    break;
                }
                fValid = VerifyScript(tx.vin[i].scriptSig, mapprevOutScriptPubKeys[tx.vin[i].prevout],
                                      tx, i, test[2].get_bool(), true, 0);
            }

            EXPECT_TRUE(!fValid) << strTest;
        }
    }
}

TEST(transaction_tests, basic_transaction_tests)
{
    // Random real transaction (e2769b09e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436)
    string transaction = "010000004d73435a01d3db1c519251eeecc76b3c68e550988290aa1d7c63d6b396b0ad069ebe98"
                         "7335000000006b483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f5"
                         "3a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d"
                         "980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffff"
                         "ff02b592849f300000001976a91401854abf39d84762ebeeb332e3116be49f1c4dad88ac00e876"
                         "48170000001976a91438f54f511df07214284de94d7cffda10b38004d988ac00000000";
    CDataStream  stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    EXPECT_TRUE(tx.CheckTransaction()) << "Simple deserialized transaction should be valid.";

    // Check that duplicate txins fail
    tx.vin.push_back(tx.vin[0]);
    EXPECT_TRUE(!tx.CheckTransaction()) << "Transaction with duplicate txins should be invalid.";
}

// Function that allows us to convert CScript to bytearray (so it can be pushed into another CScript)
template <typename T>
std::vector<unsigned char> ToByteVector(const T& in)
{
    return std::vector<unsigned char>(in.begin(), in.end());
}

//
// Helper: create two dummy transactions, each with
// two outputs.  The first has 11 and 50 CENT outputs
// paid to a TX_PUBKEY, the second 21 and 22 CENT outputs
// paid to a TX_PUBKEYHASH.
//
static std::vector<CTransaction> SetupDummyInputs(CBasicKeyStore& keystoreRet, MapPrevTx& inputsRet)
{
    std::vector<CTransaction> dummyTransactions;
    dummyTransactions.resize(5);

    // Add some keys to the keystore:
    CKey key[4];
    for (int i = 0; i < 4; i++) {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    // Create some dummy input transactions
    dummyTransactions[0].vout.resize(2);
    dummyTransactions[0].vout[0].nValue = 11 * CENT;
    dummyTransactions[0].vout[0].scriptPubKey << key[0].GetPubKey() << OP_CHECKSIG;
    dummyTransactions[0].vout[1].nValue = 50 * CENT;
    dummyTransactions[0].vout[1].scriptPubKey << key[1].GetPubKey() << OP_CHECKSIG;
    inputsRet[dummyTransactions[0].GetHash()] = make_pair(CTxIndex(), dummyTransactions[0]);

    dummyTransactions[1].vout.resize(2);
    dummyTransactions[1].vout[0].nValue = 21 * CENT;
    dummyTransactions[1].vout[0].scriptPubKey.SetDestination(key[2].GetPubKey().GetID());
    dummyTransactions[1].vout[1].nValue = 22 * CENT;
    dummyTransactions[1].vout[1].scriptPubKey.SetDestination(key[3].GetPubKey().GetID());
    inputsRet[dummyTransactions[1].GetHash()] = make_pair(CTxIndex(), dummyTransactions[1]);

    // Add some additional dummy transactions to test flexibility of AreInputsStandard
    // It is important to remember that AreInputsStandard doesn't actually evaluate the redeemScript
    // itself but checks whether the format fits a preset template, in this case, less than 16 SigOps
    CScript redeemScript;
    dummyTransactions[2].vout.resize(1);
    dummyTransactions[2].vout[0].nValue       = 23 * CENT;
    redeemScript                              = ParseScript("0 PICK 20 EQUALVERIFY DEPTH 3 EQUAL");
    dummyTransactions[2].vout[0].scriptPubKey = GetScriptForDestination(redeemScript.GetID());
    inputsRet[dummyTransactions[2].GetHash()] = make_pair(CTxIndex(), dummyTransactions[2]);

    dummyTransactions[3].vout.resize(1);
    dummyTransactions[3].vout[0].nValue = 23 * CENT;
    // Create scripthash with large number of sig ops but below limit (15 sig ops)
    redeemScript = ParseScript("CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG "
                               "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG");
    dummyTransactions[3].vout[0].scriptPubKey = GetScriptForDestination(redeemScript.GetID());
    inputsRet[dummyTransactions[3].GetHash()] = make_pair(CTxIndex(), dummyTransactions[3]);

    dummyTransactions[4].vout.resize(1);
    dummyTransactions[4].vout[0].nValue = 23 * CENT;
    // Create scripthash with too many sig ops ( > 15)
    redeemScript =
        ParseScript("CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG "
                    "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG");
    dummyTransactions[4].vout[0].scriptPubKey = GetScriptForDestination(redeemScript.GetID());
    inputsRet[dummyTransactions[4].GetHash()] = make_pair(CTxIndex(), dummyTransactions[4]);

    return dummyTransactions;
}

TEST(transaction_tests, test_Get)
{
    CBasicKeyStore            keystore;
    MapPrevTx                 dummyInputs;
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, dummyInputs);

    CTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n    = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[1].prevout.n    = 0;
    t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[2].prevout.n    = 1;
    t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vout.resize(2);
    t1.vout[0].nValue = 90 * CENT;
    t1.vout[0].scriptPubKey << OP_1;

    EXPECT_TRUE(t1.AreInputsStandard(dummyInputs));
    EXPECT_EQ(t1.GetValueIn(dummyInputs), (50 + 21 + 22) * CENT);

    // Adding extra junk to the scriptSig should make it non-standard:
    t1.vin[0].scriptSig << OP_11;
    EXPECT_TRUE(!t1.AreInputsStandard(dummyInputs));

    // ... as should not having enough:
    t1.vin[0].scriptSig = CScript();
    EXPECT_TRUE(!t1.AreInputsStandard(dummyInputs));

    // Expanded this test to include ScriptHash scripts
    // Keep in mind that AreInputsStandard is ran in conjunction with IsStandardTx (on MainNet)
    // IsStandardTx prevents transactions that have non PushData scriptsigs and scriptsigs > 500 bytes
    // Therefore we do not need to test non Pushdata scriptsigs or scriptsigs > 500 bytes
    CTransaction t2;
    t2.vout.resize(1);
    t2.vout[0].nValue = 20 * CENT;
    t2.vout[0].scriptPubKey << OP_1;

    t2.vin.resize(1);
    t2.vin[0].prevout.hash = dummyTransactions[2].GetHash();
    t2.vin[0].prevout.n    = 0;
    t2.vin[0].scriptSig = ParseScript("22 21 20"); // AreInputsStandard will return true and the script
                                                   // sig is true when combined with redeemScript
    t2.vin[0].scriptSig << ToByteVector(
        ParseScript("0 PICK 20 EQUALVERIFY DEPTH 3 EQUAL")); // Push the redeemScript bytes
    EXPECT_TRUE(t2.AreInputsStandard(dummyInputs));

    t2.vin[0].scriptSig = ParseScript(
        "22 21"); // AreInputsStandard will still return true, as we do not evaluate in AreInputsStandard
    t2.vin[0].scriptSig << ToByteVector(
        ParseScript("0 PICK 20 EQUALVERIFY DEPTH 3 EQUAL")); // Push the redeemScript bytes
    EXPECT_TRUE(t2.AreInputsStandard(dummyInputs));

    t2.vin[0].prevout.hash = dummyTransactions[3].GetHash();
    t2.vin[0].scriptSig =
        ParseScript("1"); // Create a scriptsig that will be true as it is below limit of sig ops
    t2.vin[0].scriptSig << ToByteVector(ParseScript(
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG "
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG")); // Push the redeemScript bytes
    EXPECT_TRUE(t2.AreInputsStandard(dummyInputs));

    // Create a scriptsig that be at exactly 500 bytes (limit set by IsStandardTx). AreInputsStandard
    // will return true
    t2.vin[0].scriptSig =
        ParseScript("1 "
                    "'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz' DROP");
    t2.vin[0].scriptSig << ToByteVector(ParseScript(
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG "
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG")); // Push the redeemScript bytes
    EXPECT_EQ(t2.vin[0].scriptSig.size(), 500u);          // Confirm that this is 500 bytes
    EXPECT_TRUE(t2.AreInputsStandard(dummyInputs));

    // size 501 should pass, but IsStandardTx() should fail
    t2.vin[0].scriptSig =
        ParseScript("1 "
                    "'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
                    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz' DROP");
    t2.vin[0].scriptSig << ToByteVector(ParseScript(
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG "
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG")); // Push the redeemScript bytes
    EXPECT_EQ(t2.vin[0].scriptSig.size(), 501u);          // Confirm that this is 501 bytes
    EXPECT_TRUE(t2.AreInputsStandard(dummyInputs));
    std::string reason;
    EXPECT_FALSE(IsStandardTx(t2, reason));

    t2.vin[0].prevout.hash = dummyTransactions[4].GetHash();
    t2.vin[0].scriptSig    = ParseScript(
        "1"); // Create a scriptsig that will make AreInputsStandard return false due to too many sig ops
    t2.vin[0].scriptSig << ToByteVector(ParseScript(
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG "
        "CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG CHECKSIG")); // Push the redeemScript bytes
    EXPECT_FALSE(t2.AreInputsStandard(dummyInputs));
}

TEST(transaction_tests, test_GetThrow)
{
    CBasicKeyStore            keystore;
    MapPrevTx                 dummyInputs;
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, dummyInputs);

    MapPrevTx missingInputs;

    CTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n    = 0;
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();

    t1.vin[1].prevout.n    = 0;
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();

    t1.vin[2].prevout.n = 1;
    t1.vout.resize(2);
    t1.vout[0].nValue = 90 * CENT;
    t1.vout[0].scriptPubKey << OP_1;

    EXPECT_THROW(t1.AreInputsStandard(missingInputs), runtime_error);
    EXPECT_THROW(t1.GetValueIn(missingInputs), runtime_error);
}

void test_op_return_size(int currentHeight, int isTestnet, unsigned int expected_size)
{
    nBestHeight = currentHeight;
    fTestNet    = isTestnet;

    unsigned int allowedSize = DataSize();

    EXPECT_EQ(allowedSize, expected_size);

    LOCK(cs_main);
    CBasicKeyStore            keystore;
    MapPrevTx                 dummyInputs;
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, dummyInputs);
    {
        CTransaction t;
        t.vin.resize(1);
        t.vin[0].prevout.hash = dummyTransactions[0].GetHash();
        t.vin[0].prevout.n    = 1;
        t.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
        t.vout.resize(1);
        t.vout[0].nValue = 90 * CENT;
        CKey key;
        key.MakeNewKey(true);
        t.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

        std::string reason;
        EXPECT_TRUE(IsStandardTx(t, reason)) << reason;

        t.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("");
        EXPECT_TRUE(IsStandardTx(t, reason)) << reason;

        // exactly the allowed data size
        t.vout[0].scriptPubKey = CScript()
                                 << OP_RETURN << ParseHex(GeneratePseudoRandomHex(2 * allowedSize));
        EXPECT_TRUE(IsStandardTx(t, reason)) << reason;

        // 81 bytes (1-byte over the limit)
        t.vout[0].scriptPubKey = CScript() << OP_RETURN
                                           << ParseHex(GeneratePseudoRandomHex(2 * (allowedSize + 1)));
        EXPECT_FALSE(IsStandardTx(t, reason)) << reason;

        // Only one TX_NULL_DATA permitted in all cases
        t.vout.resize(2);
        t.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

        t.vout[1].scriptPubKey = CScript()
                                 << OP_RETURN << ParseHex(GeneratePseudoRandomHex(2 * allowedSize));
        EXPECT_TRUE(IsStandardTx(t, reason)) << reason;

        t.vout.resize(2);
        t.vout[0].scriptPubKey = CScript()
                                 << OP_RETURN << ParseHex(GeneratePseudoRandomHex(2 * allowedSize));
        t.vout[1].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

        EXPECT_TRUE(IsStandardTx(t, reason)) << reason;

        t.vout[0].scriptPubKey = CScript()
                                 << OP_RETURN << ParseHex(GeneratePseudoRandomHex(2 * allowedSize));
        t.vout[1].scriptPubKey = CScript()
                                 << OP_RETURN << ParseHex(GeneratePseudoRandomHex(2 * allowedSize));
        EXPECT_FALSE(IsStandardTx(t, reason)) << reason;

        t.vout[0].scriptPubKey = CScript()
                                 << OP_RETURN << ParseHex(GeneratePseudoRandomHex(2 * allowedSize));
        t.vout[1].scriptPubKey = CScript() << OP_RETURN;
        EXPECT_FALSE(IsStandardTx(t, reason)) << reason;

        t.vout[0].scriptPubKey = CScript() << OP_RETURN;
        t.vout[1].scriptPubKey = CScript() << OP_RETURN;
        EXPECT_FALSE(IsStandardTx(t, reason)) << reason;
    }
}

TEST(transaction_tests, op_return_size_mainnet_before_hf)
{
    int blocknum = TestnetForks.getFirstBlockOfFork(NetworkFork::NETFORK__3_TACHYON);
    test_op_return_size(blocknum - 1, false, 80);
}

TEST(transaction_tests, op_return_size_mainnet_after_hf)
{
    int blocknum = TestnetForks.getFirstBlockOfFork(NetworkFork::NETFORK__3_TACHYON);
    test_op_return_size(blocknum, false, 80);
}

TEST(transaction_tests, op_return_size_testnet_before_hf)
{
    int blocknum = TestnetForks.getFirstBlockOfFork(NetworkFork::NETFORK__3_TACHYON);
    test_op_return_size(blocknum - 1, true, 80);
}

TEST(transaction_tests, op_return_size_testnet_after_hf)
{
    int blocknum = TestnetForks.getFirstBlockOfFork(NetworkFork::NETFORK__3_TACHYON);
    test_op_return_size(blocknum, true, 4096);
}

#include <map>
#include <string>
#include "googletest/googletest/include/gtest/gtest.h"
#include "json/json_spirit_writer_template.h"

#include "main.h"
#include "wallet.h"

using namespace std;
using namespace json_spirit;

// In script_tests.cpp
extern Array read_json(const std::string& filename);
extern CScript ParseScript(string s);

TEST(transaction_tests, tx_valid)
{
    // Read tests from test/data/tx_valid.json
    // Format is an array of arrays
    // Inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptPubKey], [input 2], ...],"], serializedTransaction, enforceP2SH
    // ... where all scripts are stringified scripts.
    Array tests = read_json("tx_valid.json");

    BOOST_FOREACH(Value& tv, tests)
    {
        Array test = tv.get_array();
        string strTest = write_string(tv, false);
        if (test[0].type() == array_type)
        {
            if (test.size() != 3 || test[1].type() != str_type || test[2].type() != bool_type)
            {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            map<COutPoint, CScript> mapprevOutScriptPubKeys;
            Array inputs = test[0].get_array();
            bool fValid = true;
            BOOST_FOREACH(Value& input, inputs)
            {
                if (input.type() != array_type)
                {
                    fValid = false;
                    break;
                }
                Array vinput = input.get_array();
                if (vinput.size() != 3)
                {
                    fValid = false;
                    break;
                }

                mapprevOutScriptPubKeys[COutPoint(uint256(vinput[0].get_str()), vinput[1].get_int())] = ParseScript(vinput[2].get_str());
            }
            if (!fValid)
            {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            string transaction = test[1].get_str();
            CDataStream stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
            CTransaction tx;
            stream >> tx;

            EXPECT_TRUE(tx.CheckTransaction()) << strTest;

            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                if (!mapprevOutScriptPubKeys.count(tx.vin[i].prevout))
                {
                    ADD_FAILURE() << "Bad test: " << strTest;
                    break;
                }
                EXPECT_TRUE(VerifyScript(tx.vin[i].scriptSig, mapprevOutScriptPubKeys[tx.vin[i].prevout], tx, i, test[2].get_bool(), false, 0)) << strTest;
            }
        }
    }
}

TEST(transaction_tests, tx_invalid)
{
    // Read tests from test/data/tx_invalid.json
    // Format is an array of arrays
    // Inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptPubKey], [input 2], ...],"], serializedTransaction, enforceP2SH
    // ... where all scripts are stringified scripts.
    Array tests = read_json("tx_invalid.json");

    BOOST_FOREACH(Value& tv, tests)
    {
        Array test = tv.get_array();
        string strTest = write_string(tv, false);
        if (test[0].type() == array_type)
        {
            if (test.size() != 3 || test[1].type() != str_type || test[2].type() != bool_type)
            {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            map<COutPoint, CScript> mapprevOutScriptPubKeys;
            Array inputs = test[0].get_array();
            bool fValid = true;
            BOOST_FOREACH(Value& input, inputs)
            {
                if (input.type() != array_type)
                {
                    fValid = false;
                    break;
                }
                Array vinput = input.get_array();
                if (vinput.size() != 3)
                {
                    fValid = false;
                    break;
                }

                mapprevOutScriptPubKeys[COutPoint(uint256(vinput[0].get_str()), vinput[1].get_int())] = ParseScript(vinput[2].get_str());
            }
            if (!fValid)
            {
                ADD_FAILURE() << "Bad test: " << strTest;
                continue;
            }

            string transaction = test[1].get_str();
            CDataStream stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
            CTransaction tx;
            stream >> tx;

            fValid = tx.CheckTransaction();

            for (unsigned int i = 0; i < tx.vin.size() && fValid; i++)
            {
                if (!mapprevOutScriptPubKeys.count(tx.vin[i].prevout))
                {
                    ADD_FAILURE() << "Bad test: " << strTest;
                    break;
                }
                fValid = VerifyScript(tx.vin[i].scriptSig, mapprevOutScriptPubKeys[tx.vin[i].prevout], tx, i, test[2].get_bool(), true, 0);
            }

            EXPECT_TRUE(!fValid) << strTest;
        }
    }
}

TEST(transaction_tests, basic_transaction_tests)
{
    // Random real transaction (e2769b09e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436)
    string transaction = "010000004d73435a01d3db1c519251eeecc76b3c68e550988290aa1d7c63d6b396b0ad069ebe987335000000006b483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff02b592849f300000001976a91401854abf39d84762ebeeb332e3116be49f1c4dad88ac00e87648170000001976a91438f54f511df07214284de94d7cffda10b38004d988ac00000000";
    CDataStream stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    EXPECT_TRUE(tx.CheckTransaction()) << "Simple deserialized transaction should be valid.";

    // Check that duplicate txins fail
    tx.vin.push_back(tx.vin[0]);
    EXPECT_TRUE(!tx.CheckTransaction()) << "Transaction with duplicate txins should be invalid.";
}

//
// Helper: create two dummy transactions, each with
// two outputs.  The first has 11 and 50 CENT outputs
// paid to a TX_PUBKEY, the second 21 and 22 CENT outputs
// paid to a TX_PUBKEYHASH.
//
static std::vector<CTransaction>
SetupDummyInputs(CBasicKeyStore& keystoreRet, MapPrevTx& inputsRet)
{
    std::vector<CTransaction> dummyTransactions;
    dummyTransactions.resize(2);

    // Add some keys to the keystore:
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    // Create some dummy input transactions
    dummyTransactions[0].vout.resize(2);
    dummyTransactions[0].vout[0].nValue = 11*CENT;
    dummyTransactions[0].vout[0].scriptPubKey << key[0].GetPubKey() << OP_CHECKSIG;
    dummyTransactions[0].vout[1].nValue = 50*CENT;
    dummyTransactions[0].vout[1].scriptPubKey << key[1].GetPubKey() << OP_CHECKSIG;
    inputsRet[dummyTransactions[0].GetHash()] = make_pair(CTxIndex(), dummyTransactions[0]);

    dummyTransactions[1].vout.resize(2);
    dummyTransactions[1].vout[0].nValue = 21*CENT;
    dummyTransactions[1].vout[0].scriptPubKey.SetDestination(key[2].GetPubKey().GetID());
    dummyTransactions[1].vout[1].nValue = 22*CENT;
    dummyTransactions[1].vout[1].scriptPubKey.SetDestination(key[3].GetPubKey().GetID());
    inputsRet[dummyTransactions[1].GetHash()] = make_pair(CTxIndex(), dummyTransactions[1]);

    return dummyTransactions;
}

TEST(transaction_tests, test_Get)
{
    CBasicKeyStore keystore;
    MapPrevTx dummyInputs;
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, dummyInputs);

    CTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[1].prevout.n = 0;
    t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[2].prevout.n = 1;
    t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vout.resize(2);
    t1.vout[0].nValue = 90*CENT;
    t1.vout[0].scriptPubKey << OP_1;

    EXPECT_TRUE(t1.AreInputsStandard(dummyInputs));
    EXPECT_EQ(t1.GetValueIn(dummyInputs), (50+21+22)*CENT);

    // Adding extra junk to the scriptSig should make it non-standard:
    t1.vin[0].scriptSig << OP_11;
    EXPECT_TRUE(!t1.AreInputsStandard(dummyInputs));

    // ... as should not having enough:
    t1.vin[0].scriptSig = CScript();
    EXPECT_TRUE(!t1.AreInputsStandard(dummyInputs));
}

TEST(transaction_tests, test_GetThrow)
{
    CBasicKeyStore keystore;
    MapPrevTx dummyInputs;
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, dummyInputs);

    MapPrevTx missingInputs;

    CTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 0;
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();;
    t1.vin[1].prevout.n = 0;
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();;
    t1.vin[2].prevout.n = 1;
    t1.vout.resize(2);
    t1.vout[0].nValue = 90*CENT;
    t1.vout[0].scriptPubKey << OP_1;

    EXPECT_THROW(t1.AreInputsStandard(missingInputs), runtime_error);
    EXPECT_THROW(t1.GetValueIn(missingInputs), runtime_error);
}

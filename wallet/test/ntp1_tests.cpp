#include "googletest/googletest/include/gtest/gtest.h"

#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1/ntp1sendtokensdata.h"
#include "ntp1/ntp1sendtokensonerecipientdata.h"
#include "ntp1/ntp1tokenmetadata.h"
#include "ntp1/ntp1tokentxdata.h"
#include "ntp1/ntp1transaction.h"
#include "ntp1/ntp1txin.h"
#include "ntp1/ntp1txout.h"
#include "ntp1/ntp1wallet.h"

TEST(ntp1_tests, parse_NTP1TxIn_from_json)
{
    std::string tx_str =
        "{\"txid\":\"4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90\",\"vout\":1,"
        "\"scriptSig\":{\"asm\":"
        "\"3045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916"
        "a85c10202e7bad783d756db9d33571cde646113f68aa99a7f01 "
        "03f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\",\"hex\":"
        "\"483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e9"
        "16a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5"
        "223c2da74938a725ed7b0244a8\"},\"sequence\":4294967295,\"previousOutput\":{\"asm\":\"OP_DUP "
        "OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY "
        "OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,"
        "\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[{"
        "\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":997000,\"issueTxid\":"
        "\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,"
        "\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"value\":10000,\"fixed\":true}";
    NTP1TxIn tx_good, tx_nogood;
    EXPECT_NO_THROW(tx_good.importJsonData(tx_str));
    EXPECT_ANY_THROW(tx_nogood.importJsonData(tx_str.substr(0, tx_str.size() - 1)));
    EXPECT_EQ(tx_good.getOutPoint().getHash().ToString(),
              "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getOutPoint().getIndex(), (unsigned int)1);
    EXPECT_EQ(tx_good.getScriptSigHex(), "483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a34"
                                         "5c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d3357"
                                         "1cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8"
                                         "e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getSequence(), (uint64_t)4294967295);
    EXPECT_EQ(tx_good.getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getToken(0).getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getToken(0).getDivisibility(), (unsigned)7);
    EXPECT_EQ(tx_good.getToken(0).getAmount(), (unsigned)997000);
    EXPECT_EQ(tx_good.getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
}

TEST(ntp1_tests, parse_NTP1TxOut_from_json)
{
    std::string tx_str = "{\"value\":10000,\"n\":0,\"scriptPubKey\":{\"asm\":\"OP_DUP OP_HASH160 "
                         "930b31797c0e6f0d4239909b044aaadfde371995 OP_EQUALVERIFY "
                         "OP_CHECKSIG\",\"hex\":\"76a914930b31797c0e6f0d4239909b044aaadfde37199588ac\","
                         "\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":["
                         "\"NZKTvgBXGBFDde73TizBmxVPzNauT1ivxV\"]},\"tokens\":[{\"tokenId\":"
                         "\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":1000,\"issueTxid\":"
                         "\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\","
                         "\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}"
                         "],\"used\":false,\"blockheight\":159366}";
    NTP1TxOut tx_good, tx_nogood;
    EXPECT_NO_THROW(tx_good.importJsonData(tx_str));
    EXPECT_ANY_THROW(tx_nogood.importJsonData(tx_str.substr(0, tx_str.size() - 1)));
    EXPECT_EQ(tx_good.getValue(), 10000);
    EXPECT_EQ(tx_good.getScriptPubKeyHex(), "76a914930b31797c0e6f0d4239909b044aaadfde37199588ac");
    EXPECT_EQ(tx_good.getAddress(), "NZKTvgBXGBFDde73TizBmxVPzNauT1ivxV");
    EXPECT_EQ(tx_good.getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getToken(0).getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getToken(0).getDivisibility(), (unsigned)7);
    EXPECT_EQ(tx_good.getToken(0).getAmount(), (unsigned)1000);
    EXPECT_EQ(tx_good.getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
}

TEST(ntp1_tests, parse_NTP1TokenData)
{
    std::string token_str =
        "{\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":512345,\"issueTxid\":"
        "\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,"
        "\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}";
    NTP1TokenTxData token_good, token_nogood;
    EXPECT_NO_THROW(token_good.importJsonData(token_str));
    EXPECT_ANY_THROW(token_nogood.importJsonData(token_str.substr(0, token_str.size() - 1)));
    EXPECT_EQ(token_good.getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(token_good.getDivisibility(), (unsigned)7);
    EXPECT_EQ(token_good.getAmount(), (unsigned)512345);
    EXPECT_EQ(token_good.getLockStatus(), true);
    EXPECT_EQ(token_good.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(token_good.getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");

    // test database saving
    json_spirit::Value v = token_good.exportDatabaseJsonData();
    NTP1TokenTxData    m;
    m.importDatabaseJsonData(v);
    EXPECT_EQ(m, token_good);
}

TEST(ntp1_tests, parse_NTP1Transaction)
{
    std::string tx_str =
        "{\"hex\":"
        "\"010000001c4d9e5a0290bed89598211fb5feac9c80f475dc16486ca352d6a40c867a9f6106a3e3334f010000006b4"
        "83045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a"
        "85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223"
        "c2da74938a725ed7b0244a8ffffffff90bed89598211fb5feac9c80f475dc16486ca352d6a40c867a9f6106a3e3334f"
        "030000006a47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768ded"
        "be3baaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6fd"
        "d4e8e5223c2da74938a725ed7b0244a8ffffffff0410270000000000001976a914930b31797c0e6f0d4239909b044aa"
        "adfde37199588ac10270000000000001976a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac102700000000"
        "00000d6a0b4e54011500201301403e4390a1883b000000001976a9149e719d5db5e01bb357188f7ab25e336a9c2de11"
        "288ac00000000\",\"txid\":\"1ec95ea69b16385da91a87af7c60bc625d4b96416a45f5f95f47e4d07c19aebc\","
        "\"version\":1,\"locktime\":0,\"vin\":[{\"txid\":"
        "\"4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90\",\"vout\":1,\"scriptSig\":{"
        "\"asm\":"
        "\"3045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916"
        "a85c10202e7bad783d756db9d33571cde646113f68aa99a7f01 "
        "03f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\",\"hex\":"
        "\"483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e9"
        "16a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5"
        "223c2da74938a725ed7b0244a8\"},\"sequence\":4294967295,\"previousOutput\":{\"asm\":\"OP_DUP "
        "OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY "
        "OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,"
        "\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[{"
        "\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":997000,\"issueTxid\":"
        "\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,"
        "\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"value\":10000,\"fixed\":true},{"
        "\"txid\":\"4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90\",\"vout\":3,"
        "\"scriptSig\":{\"asm\":"
        "\"304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaefaf"
        "d65fd9d67e583cad6c96fdeb542d50f40022fef07d87a5501 "
        "03f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\",\"hex\":"
        "\"47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaef"
        "afd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6fdd4e8e522"
        "3c2da74938a725ed7b0244a8\"},\"sequence\":4294967295,\"previousOutput\":{\"asm\":\"OP_DUP "
        "OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY "
        "OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,"
        "\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[],"
        "\"value\":998840000,\"fixed\":true}],\"vout\":[{\"value\":10000,\"n\":0,\"scriptPubKey\":{"
        "\"asm\":\"OP_DUP OP_HASH160 930b31797c0e6f0d4239909b044aaadfde371995 OP_EQUALVERIFY "
        "OP_CHECKSIG\",\"hex\":\"76a914930b31797c0e6f0d4239909b044aaadfde37199588ac\",\"reqSigs\":1,"
        "\"type\":\"pubkeyhash\",\"addresses\":[\"NZKTvgBXGBFDde73TizBmxVPzNauT1ivxV\"]},\"tokens\":[{"
        "\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":1000,\"issueTxid\":"
        "\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,"
        "\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"used\":false,\"blockheight\":"
        "159366},{\"value\":10000,\"n\":1,\"scriptPubKey\":{\"asm\":\"OP_DUP OP_HASH160 "
        "9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY "
        "OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,"
        "\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[{"
        "\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":996000,\"issueTxid\":"
        "\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,"
        "\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"used\":true,\"blockheight\":"
        "159366,\"usedBlockheight\":-1,\"usedTxid\":"
        "\"800ab6605c9d55730615617d9147aacde311433f497e9acf9d3dab143058045a\"},{\"value\":10000,\"n\":2,"
        "\"scriptPubKey\":{\"asm\":\"OP_RETURN "
        "4e54011500201301403e43\",\"hex\":\"6a0b4e54011500201301403e43\",\"type\":\"nulldata\"},"
        "\"tokens\":[],\"used\":false,\"blockheight\":159366},{\"value\":998810000,\"n\":3,"
        "\"scriptPubKey\":{\"asm\":\"OP_DUP OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 "
        "OP_EQUALVERIFY "
        "OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,"
        "\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[],"
        "\"used\":true,\"blockheight\":159366,\"usedBlockheight\":-1,\"usedTxid\":"
        "\"9343e14519eaef287519d02d3baecf3126de462d74dea7ddd479ff2abe365b02\"}],\"blocktime\":"
        "1520323876000,\"blockheight\":159366,\"totalsent\":998840000,\"fee\":10000,\"blockhash\":"
        "\"c1cba88c61842c1c760bbfeb0fb977e9d0b266208084a09af0bf2e27cfad29fd\",\"time\":1520323876000,"
        "\"confirmations\":26948}";
    NTP1Transaction tx_good, tx_nogood;
    EXPECT_NO_THROW(tx_good.importJsonData(tx_str));
    EXPECT_ANY_THROW(tx_nogood.importJsonData(tx_str.substr(0, tx_str.size() - 1)));
    std::string hex = tx_good.getHex();
    std::transform(hex.begin(), hex.end(), hex.begin(), ::tolower);
    EXPECT_EQ(hex, "010000001c4d9e5a0290bed89598211fb5feac9c80f475dc16486ca352d6a40c867a9f6106a3e3334f01"
                   "0000006b483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b"
                   "02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b4"
                   "2b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8ffffffff90bed89598211fb5feac9c"
                   "80f475dc16486ca352d6a40c867a9f6106a3e3334f030000006a47304402200f4123e57d950f434605a8"
                   "45efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaefafd65fd9d67e583cad6c96f"
                   "deb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a7"
                   "25ed7b0244a8ffffffff0410270000000000001976a914930b31797c0e6f0d4239909b044aaadfde3719"
                   "9588ac10270000000000001976a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac1027000000"
                   "0000000d6a0b4e54011500201301403e4390a1883b000000001976a9149e719d5db5e01bb357188f7ab2"
                   "5e336a9c2de11288ac00000000");
    EXPECT_EQ(tx_good.getLockTime(), (uint64_t)0);
    EXPECT_EQ(tx_good.getTime(), (uint64_t)1520323876000);
    EXPECT_EQ(tx_good.getTxHash().ToString(),
              "1ec95ea69b16385da91a87af7c60bc625d4b96416a45f5f95f47e4d07c19aebc");

    EXPECT_EQ(tx_good.getTxInCount(), (unsigned long)2);
    EXPECT_EQ(tx_good.getTxIn(0).getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxIn(1).getNumOfTokens(), (unsigned long)0);

    EXPECT_EQ(tx_good.getTxIn(0).getOutPoint().getHash().ToString(),
              "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getTxIn(0).getOutPoint().getIndex(), (unsigned)1);
    EXPECT_EQ(tx_good.getTxIn(0).getScriptSigHex(),
              "483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fc"
              "a8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162"
              "e6fdd4e8e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getTxIn(0).getSequence(), (uint64_t)4294967295);

    EXPECT_EQ(tx_good.getTxIn(1).getOutPoint().getHash().ToString(),
              "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getTxIn(1).getOutPoint().getIndex(), (unsigned)3);
    EXPECT_EQ(tx_good.getTxIn(1).getScriptSigHex(),
              "47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3b"
              "aaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6"
              "fdd4e8e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getTxIn(1).getSequence(), (uint64_t)4294967295);

    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getAmount(), (uint64_t)997000);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getDivisibility(), (uint64_t)7);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getTokenIdBase58(),
              "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");

    EXPECT_EQ(tx_good.getTxOutCount(), (unsigned long)4);
    EXPECT_EQ(tx_good.getTxOut(0).getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxOut(1).getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxOut(2).getNumOfTokens(), (unsigned long)0);
    EXPECT_EQ(tx_good.getTxOut(3).getNumOfTokens(), (unsigned long)0);

    EXPECT_EQ(tx_good.getTxOut(0).getScriptPubKeyHex(),
              "76a914930b31797c0e6f0d4239909b044aaadfde37199588ac");
    EXPECT_EQ(tx_good.getTxOut(0).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getAmount(), (uint64_t)1000);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getDivisibility(), (uint64_t)7);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getTokenIdBase58(),
              "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getTxOut(0).getAddress(), "NZKTvgBXGBFDde73TizBmxVPzNauT1ivxV");
    EXPECT_EQ(tx_good.getTxOut(0).getType(), NTP1TxOut::OutputType::NormalOutput);

    EXPECT_EQ(tx_good.getTxOut(1).getScriptPubKeyHex(),
              "76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac");
    EXPECT_EQ(tx_good.getTxOut(1).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getAmount(), (uint64_t)996000);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getDivisibility(), (uint64_t)7);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getTokenIdBase58(),
              "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getTxOut(1).getAddress(), "NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D");
    EXPECT_EQ(tx_good.getTxOut(1).getType(), NTP1TxOut::OutputType::NormalOutput);

    EXPECT_EQ(tx_good.getTxOut(2).getScriptPubKeyHex(), "6a0b4e54011500201301403e43");
    EXPECT_EQ(tx_good.getTxOut(2).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(2).getNumOfTokens(), 0u);
    EXPECT_EQ(tx_good.getTxOut(2).getType(), NTP1TxOut::OutputType::OPReturn);

    json_spirit::Value v = tx_good.exportDatabaseJsonData();
    NTP1Transaction    t;
    t.importDatabaseJsonData(v);
    EXPECT_EQ(t, tx_good);
}

TEST(ntp1_tests, token_meta_data)
{
    std::string metadata = "{\"tokenId\":\"La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx\",\"divisibility\":7,"
                           "\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\",\"someUtxo\":"
                           "\"0001e26e6b5e98d1b4e96987f1e010f351f4fa2d6d0bacfafc7f501787bbf4f6:8\","
                           "\"numOfHolders\":8281,\"totalSupply\":1607509562,\"numOfTransfers\":1679,"
                           "\"numOfIssuance\":1,\"numOfBurns\":1,\"firstBlock\":159746,\"issuanceTxid\":"
                           "\"578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56\","
                           "\"issueAddress\":\"NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW\","
                           "\"metadataOfIssuence\":{\"data\":{\"tokenName\":\"TRIF\",\"description\":"
                           "\"Trifid\",\"issuer\":\"TrifidTeam\",\"urls\":[{\"name\":\"icon\",\"url\":"
                           "\"https://ntp1-icons.ams3.digitaloceanspaces.com/"
                           "24d000d478d72d6a43b89bbabbe41a45acde9ba0.png\",\"mimeType\":\"image/"
                           "png\"}],\"userData\":{\"meta\":[]}}},\"sha2Issue\":"
                           "\"c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113\"}";
    NTP1TokenMetaData tokenMetaData;
    EXPECT_NO_THROW(tokenMetaData.importRestfulAPIJsonData(metadata));

    EXPECT_EQ(tokenMetaData.getTokenIdBase58(), "La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx");
    EXPECT_EQ(tokenMetaData.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tokenMetaData.getLockStatus(), true);
    EXPECT_EQ(tokenMetaData.getDivisibility(), (unsigned)7);
    EXPECT_EQ(tokenMetaData.getFirstBlock(), (unsigned)159746);
    EXPECT_EQ(tokenMetaData.getNumOfBurns(), (unsigned)1);
    EXPECT_EQ(tokenMetaData.getNumOfIssuance(), (unsigned)1);
    EXPECT_EQ(tokenMetaData.getNumOfTransfers(), (unsigned)1679);
    EXPECT_EQ(tokenMetaData.getNumOfHolders(), (unsigned)8281);
    EXPECT_EQ(tokenMetaData.getIconImageType(), "image/png");
    EXPECT_EQ(
        tokenMetaData.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/24d000d478d72d6a43b89bbabbe41a45acde9ba0.png");
    EXPECT_EQ(tokenMetaData.getIssuanceTxId().ToString(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssuanceTxIdHex(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssueAddress().ToString(), "NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW");
    EXPECT_EQ(tokenMetaData.getSha2Issue(),
              "c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113");
    EXPECT_EQ(tokenMetaData.getTokenDescription(), "Trifid");
    EXPECT_EQ(tokenMetaData.getTokenName(), "TRIF");
    EXPECT_EQ(tokenMetaData.getTokenIssuer(), "TrifidTeam");
    EXPECT_EQ(tokenMetaData.getTotalSupply(), (unsigned)1607509562);
}

TEST(ntp1_tests, token_meta_data_without_url)
{
    std::string metadata =
        "{\"tokenId\":\"La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx\",\"divisibility\":7,\"lockStatus\":true,"
        "\"aggregationPolicy\":\"aggregatable\",\"someUtxo\":"
        "\"0001e26e6b5e98d1b4e96987f1e010f351f4fa2d6d0bacfafc7f501787bbf4f6:8\",\"numOfHolders\":8281,"
        "\"totalSupply\":1607509562,\"numOfTransfers\":1679,\"numOfIssuance\":1,\"numOfBurns\":1,"
        "\"firstBlock\":159746,\"issuanceTxid\":"
        "\"578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56\",\"issueAddress\":"
        "\"NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW\",\"metadataOfIssuence\":{\"data\":{\"tokenName\":"
        "\"TRIF\",\"description\":\"Trifid\",\"issuer\":\"TrifidTeam\",\"userData\":{\"meta\":[]}}},"
        "\"sha2Issue\":\"c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113\"}";
    NTP1TokenMetaData tokenMetaData;
    EXPECT_NO_THROW(tokenMetaData.importRestfulAPIJsonData(metadata));

    EXPECT_EQ(tokenMetaData.getTokenIdBase58(), "La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx");
    EXPECT_EQ(tokenMetaData.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tokenMetaData.getLockStatus(), true);
    EXPECT_EQ(tokenMetaData.getDivisibility(), (unsigned)7);
    EXPECT_EQ(tokenMetaData.getFirstBlock(), (unsigned)159746);
    EXPECT_EQ(tokenMetaData.getNumOfBurns(), (unsigned)1);
    EXPECT_EQ(tokenMetaData.getNumOfIssuance(), (unsigned)1);
    EXPECT_EQ(tokenMetaData.getNumOfTransfers(), (unsigned)1679);
    EXPECT_EQ(tokenMetaData.getNumOfHolders(), (unsigned)8281);
    EXPECT_EQ(tokenMetaData.getIconImageType(), "");
    EXPECT_EQ(tokenMetaData.getIconURL(), "");
    EXPECT_EQ(tokenMetaData.getIssuanceTxId().ToString(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssuanceTxIdHex(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssueAddress().ToString(), "NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW");
    EXPECT_EQ(tokenMetaData.getSha2Issue(),
              "c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113");
    EXPECT_EQ(tokenMetaData.getTokenDescription(), "Trifid");
    EXPECT_EQ(tokenMetaData.getTokenName(), "TRIF");
    EXPECT_EQ(tokenMetaData.getTokenIssuer(), "TrifidTeam");
    EXPECT_EQ(tokenMetaData.getTotalSupply(), (unsigned)1607509562);

    // test database saving
    json_spirit::Value v = tokenMetaData.exportDatabaseJsonData();
    NTP1TokenMetaData  m;
    m.importDatabaseJsonData(v);
    EXPECT_EQ(m, tokenMetaData);
}

TEST(ntp1_tests, wallet_tests)
{
    NTP1Wallet                      wallet1;
    typedef boost::filesystem::path Path;
    EXPECT_NO_THROW(wallet1.importFromFile(Path(TEST_ROOT_PATH) / Path("/data/NTP1Wallet.json")));
    Path tempWalletPath = Path(TEST_ROOT_PATH) / Path("/data/tmp.json");
    EXPECT_NO_THROW(wallet1.exportToFile(tempWalletPath));
    NTP1Wallet wallet2;
    EXPECT_NO_THROW(wallet2.importFromFile(tempWalletPath));
    EXPECT_NO_THROW(boost::filesystem::remove(Path(TEST_ROOT_PATH) / Path("/data/tmp.json")));
}

TEST(ntp1_tests, send_tests)
{
    // set values in object and create json output
    NTP1SendTokensOneRecipientData r1, r2;
    NTP1SendTokensData             s;
    s.setFee(10);
    r1.amount      = 1;
    r1.destination = "dest1";
    r1.tokenId     = "tokenid1";
    r2.amount      = 2;
    r2.destination = "dest2";
    r2.tokenId     = "tokenid2";
    s.addRecipient(r1);
    s.addRecipient(r2);
    s.addTokenSourceAddress("a");
    s.addTokenSourceAddress("b");
    s.addTokenSourceAddress("c");
    std::string json_out = s.exportToAPIFormat();

    // reparse the json output and test it
    json_spirit::Value testVal;
    json_spirit::read_or_throw(json_out, testVal);
    // test flags
    json_spirit::Object flags = NTP1Tools::GetObjectField(testVal.get_obj(), "flags");
    EXPECT_EQ(flags.size(), 1u);
    EXPECT_EQ(NTP1Tools::GetBoolField(flags, "splitChange"), true);
    // test fee
    uint64_t fee = NTP1Tools::GetUint64Field(testVal.get_obj(), "fee");
    // test "from" field
    EXPECT_EQ(fee, 10u);
    json_spirit::Array from = NTP1Tools::GetArrayField(testVal.get_obj(), "from");
    EXPECT_EQ(from.size(), 3u);
    EXPECT_EQ(from[0].get_str(), "a");
    EXPECT_EQ(from[1].get_str(), "b");
    EXPECT_EQ(from[2].get_str(), "c");
    // test "to" field
    json_spirit::Array to = NTP1Tools::GetArrayField(testVal.get_obj(), "to");
    EXPECT_EQ(NTP1Tools::GetStrField(to[0].get_obj(), "address"), "dest1");
    EXPECT_EQ(NTP1Tools::GetStrField(to[0].get_obj(), "tokenId"), "tokenid1");
    EXPECT_EQ(NTP1Tools::GetUint64Field(to[0].get_obj(), "amount"), 1u);
    EXPECT_EQ(NTP1Tools::GetStrField(to[1].get_obj(), "address"), "dest2");
    EXPECT_EQ(NTP1Tools::GetStrField(to[1].get_obj(), "tokenId"), "tokenid2");
    EXPECT_EQ(NTP1Tools::GetUint64Field(to[1].get_obj(), "amount"), 2u);
}

TEST(ntp1_tests, amount_to_int)
{
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("69892a92"), 999901700);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("c007b60b6f687a"), 8478457292922);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("40ef54"), 38290000);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("201f"), 1000000000000000);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("60b0b460"), 723782);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("5545e1"), 871340);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("c007b60b6f687a"), 8478457292922);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("11"), 17);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("2012"), 100);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("4bb3c1"), 479320);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("68c7e5b3"), 9207387000);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("8029990f1a"), 8723709100);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("a09c47f7b1a1"), 839027891720);
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("c0a60eea1aa8fd"), 182582987368701);

    EXPECT_ANY_THROW(NTP1AmountHexToNumber<int64_t>("x"));
    EXPECT_ANY_THROW(NTP1AmountHexToNumber<int64_t>(" "));
    EXPECT_ANY_THROW(NTP1AmountHexToNumber<int64_t>("ssdsdmwdmo"));
    EXPECT_ANY_THROW(NTP1AmountHexToNumber<int64_t>("999999999999999999999999999999999999999999999999"));

    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(999901700), "69892a92");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(8478457292922), "c007b60b6f687a");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(38290000), "40ef54");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(1000000000000000), "201f");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(723782), "60b0b460");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(871340), "5545e1");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(8478457292922), "c007b60b6f687a");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(17), "11");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(100), "2012");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(479320), "4bb3c1");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(9207387000), "68c7e5b3");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(8723709100), "8029990f1a");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(839027891720), "a09c47f7b1a1");
    EXPECT_EQ(NumberToNTP1Amount<uint64_t>(182582987368701), "c0a60eea1aa8fd");
}

TEST(ntp1_tests, script_transfer)
{
    // transfer some tokens
    const std::string                    toParse_transfer = "4e5401150069892a92";
    std::shared_ptr<NTP1Script>          script           = NTP1Script::ParseScript(toParse_transfer);
    std::shared_ptr<NTP1Script_Transfer> script_transfer =
        std::dynamic_pointer_cast<NTP1Script_Transfer>(script);
    EXPECT_EQ(script_transfer->getHeader(), boost::algorithm::unhex(toParse_transfer.substr(0, 6)));
    EXPECT_EQ(script_transfer->getHexMetadata().size(), (unsigned)0);
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getOpCodeBin()), "15");
    EXPECT_EQ(script_transfer->getTxType(), NTP1Script::TxType::TxType_Transfer);

    EXPECT_EQ(script_transfer->getTransferInstructionsCount(), (unsigned)1);
    EXPECT_EQ(script_transfer->getTransferInstruction(0).amount, (uint64_t)999901700);
    EXPECT_EQ(script_transfer->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(0).outputIndex, 0);
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(0).rawAmount), "69892A92");
    EXPECT_EQ(script_transfer->getTransferInstruction(0).firstRawByte, 0);
}

TEST(ntp1_tests, script_issuance)
{
    {
        // issue NIBBL
        // txid: 66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80
        std::string toParse_issuance =
            "4e5401014e4942424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
            "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(toParse_issuance);
        std::shared_ptr<NTP1Script_Issuance> script_issuance =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(script);
        EXPECT_NE(script_issuance.get(), nullptr);
        EXPECT_EQ(script_issuance->getAggregationPolicy(),
                  NTP1Script::IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable);
        EXPECT_EQ(script_issuance->getAmount(), (uint64_t)1000000000);
        EXPECT_EQ(script_issuance->getDivisibility(), 7);
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getHeader()), "4E5401");
        EXPECT_EQ(script_issuance->getHexMetadata(), "AB10C04E20E0AEC73D58C8FBF2A9C26A6DC3ED666C7B80FEF2"
                                                     "15620C817703B1E5D8B1870211CE7CDF50718B4789245FB80F"
                                                     "5899");
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getOpCodeBin()), "01");
        EXPECT_EQ(script_issuance->getTokenSymbol(), "NIBBL");
        EXPECT_EQ(script_issuance->getTxType(), NTP1Script::TxType::TxType_Issuance);

        EXPECT_EQ(script_issuance->getTransferInstructionsCount(), (unsigned)1);
        EXPECT_EQ(script_issuance->getTransferInstruction(0).amount, (uint64_t)1000000000);
        EXPECT_EQ(script_issuance->getTransferInstruction(0).skipInput, false);
        EXPECT_EQ(script_issuance->getTransferInstruction(0).outputIndex, 0);
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getTransferInstruction(0).rawAmount), "2019");
        EXPECT_EQ(script_issuance->getTransferInstruction(0).firstRawByte, 0);
    }
}

TEST(ntp1_tests, script_burn)
{
    // burn some tokens
    std::string                      toParse_burn = "4e5401251f2013";
    std::shared_ptr<NTP1Script>      script       = NTP1Script::ParseScript(toParse_burn);
    std::shared_ptr<NTP1Script_Burn> script_burn  = std::dynamic_pointer_cast<NTP1Script_Burn>(script);
    EXPECT_EQ(script_burn->getHeader(), boost::algorithm::unhex(toParse_burn.substr(0, 6)));
    EXPECT_EQ(script_burn->getHexMetadata().size(), (unsigned)0);
    EXPECT_EQ(boost::algorithm::hex(script_burn->getOpCodeBin()), "25");
    EXPECT_EQ(script_burn->getTxType(), NTP1Script::TxType::TxType_Burn);

    EXPECT_EQ(script_burn->getTransferInstructionsCount(), (unsigned)1);
    EXPECT_EQ(script_burn->getTransferInstruction(0).amount, (uint64_t)1000);
    EXPECT_EQ(script_burn->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_burn->getTransferInstruction(0).outputIndex, 31);
    EXPECT_EQ(boost::algorithm::hex(script_burn->getTransferInstruction(0).rawAmount), "2013");
    EXPECT_EQ(script_burn->getTransferInstruction(0).firstRawByte, 31);
}

TEST(ntp1_tests, script_get_amount_size)
{
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("11"))[0]),
              (unsigned)1);
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("2012"))[0]),
              (unsigned)2);
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("4bb3c1"))[0]),
              (unsigned)3);
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("68c7e5b3"))[0]),
              (unsigned)4);
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("8029990f1a"))[0]),
              (unsigned)5);
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("a09c47f7b1a1"))[0]),
              (unsigned)6);
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("c0a60eea1aa8fd"))[0]),
              (unsigned)7);
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction)
{
    {
        // issuance
        string transaction =
            "010000001af29a5a012081139a3e0d764e9fb415bf1601c5bc24eba093c3f6a735aaa9d81d27d55dc5010000006"
            "b483045022100ea2baf384bb518ed939a1dfc02df634be2186c5e35d79a09fc7c1f1379987bc102200e286cc382"
            "9fbe574bda0cacfe8e918755574685bcb8af8a67b2d24f0087122d012103bd4c76349aae4b81011eddce127f36c"
            "ffd6b7beaf84c80d5d4e6cf06e5c8596cffffffff0310270000000000001976a9144e2a50f7e8c58ff9a0175f95"
            "616a1657b49a06a888ac1027000000000000456a434e5401014e4942424cab10c04e20e0aec73d58c8fbf2a9c26"
            "a6dc3ed666c7b80fef215620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0e073eb"
            "0b000000001976a9144e2a50f7e8c58ff9a0175f95616a1657b49a06a888ac00000000";
        CDataStream  stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        stream >> tx;
        EXPECT_EQ(tx.GetHash().ToString(),
                  "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
        EXPECT_TRUE(tx.CheckTransaction());

        std::string opReturnArg;
        EXPECT_TRUE(IsTxNTP1(&tx, &opReturnArg));
        std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(opReturnArg);
        std::shared_ptr<NTP1Script_Issuance> script_issuance =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(script);
        EXPECT_NE(script_issuance.get(), nullptr);
        EXPECT_EQ(script_issuance->getAggregationPolicy(),
                  NTP1Script::IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable);
        EXPECT_EQ(script_issuance->getAmount(), (uint64_t)1000000000);
        EXPECT_EQ(script_issuance->getDivisibility(), 7);
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getHeader()), "4E5401");
        EXPECT_EQ(script_issuance->getHexMetadata(), "AB10C04E20E0AEC73D58C8FBF2A9C26A6DC3ED666C7B80FEF2"
                                                     "15620C817703B1E5D8B1870211CE7CDF50718B4789245FB80F"
                                                     "5899");
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getOpCodeBin()), "01");
        EXPECT_EQ(script_issuance->getTokenSymbol(), "NIBBL");
        EXPECT_EQ(script_issuance->getTxType(), NTP1Script::TxType::TxType_Issuance);

        EXPECT_EQ(script_issuance->getTransferInstructionsCount(), (unsigned)1);
        EXPECT_EQ(script_issuance->getTransferInstruction(0).amount, (uint64_t)1000000000);
        EXPECT_EQ(script_issuance->getTransferInstruction(0).skipInput, false);
        EXPECT_EQ(script_issuance->getTransferInstruction(0).outputIndex, 0);
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getTransferInstruction(0).rawAmount), "2019");
        EXPECT_EQ(script_issuance->getTransferInstruction(0).firstRawByte, 0);
        EXPECT_EQ(tx.GetHash().ToString(),
                  "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
        ////////////////////////////////////////////////////

        NTP1Transaction ntp1tx;
        EXPECT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx));
        EXPECT_EQ(ntp1tx.getTxInCount(), (unsigned)1);
        EXPECT_EQ(ntp1tx.getTxIn(0).getNumOfTokens(), (unsigned)0);
        EXPECT_EQ(ntp1tx.getTxOutCount(), (unsigned)3);
        EXPECT_EQ(ntp1tx.getTxOut(0).getNumOfTokens(), (unsigned)1);
        EXPECT_EQ(ntp1tx.getTxOut(1).getNumOfTokens(), (unsigned)0);
        EXPECT_EQ(ntp1tx.getTxOut(2).getNumOfTokens(), (unsigned)0);
        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), (unsigned)1000000000);
        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getDivisibility(), (unsigned)7);
        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getIssueTxId().ToString(),
                  "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
        EXPECT_TRUE(ntp1tx.getTxOut(0).getToken(0).getLockStatus());
        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getTokenSymbol(), "NIBBL");
        EXPECT_EQ(ntp1tx.getTxHash().ToString(),
                  "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");

        // TODO
        //        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getTokenIdBase58(), "");
    }
    {
        // transfer
        string transaction =
            "01000000f554a35a0247b394148396ef78de65f4792e57bb93f9322e0a42f923e52d39530915a96617010000006"
            "a47304402200cfdd8969cb137ee5a1dde2bed954ab8ae88fb4703125e4ec103b2f21787fa27022079ffc6a52a62"
            "d1eb8aa78f831faa038fde8549ec446cb0f7427db77c7d3ddb59012103331393f9487ef4b318ae79972f3ccc84b"
            "15d0718d7e05720c454404e67d51d1affffffff120665b0c7b62c2b7e3b3d3bacd9947eefc5b80d42b9a15a2c84"
            "bd1f40811411030000006a47304402205c7d97ee153e83c54f5c61221acec7b8b60786fca80e59e45ecbf1736a7"
            "f459a02207147d019151d91143d6289d37d49b3ad63952830dc42a79bf262ebaee5da6040012103331393f9487e"
            "f4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a91471877"
            "06893521dd4d61a843d241c5f52f32d7e6188ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b11ab"
            "9211705770db88ac10270000000000000e6a0c4e5401150020120169895242409c0000000000001976a9143f7eb"
            "8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
        CDataStream  stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        stream >> tx;
        EXPECT_TRUE(tx.CheckTransaction());

        std::string opReturnArg;
        EXPECT_TRUE(IsTxNTP1(&tx, &opReturnArg));
        std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(opReturnArg);
        std::shared_ptr<NTP1Script_Transfer> script_transfer =
            std::dynamic_pointer_cast<NTP1Script_Transfer>(script);
        EXPECT_EQ(script_transfer->getHeader(), boost::algorithm::unhex(opReturnArg.substr(0, 6)));
        EXPECT_EQ(script_transfer->getHexMetadata().size(), (unsigned)0);
        EXPECT_EQ(boost::algorithm::hex(script_transfer->getOpCodeBin()), "15");
        EXPECT_EQ(script_transfer->getTxType(), NTP1Script::TxType::TxType_Transfer);

        EXPECT_EQ(script_transfer->getTransferInstructionsCount(), (unsigned)2);

        EXPECT_EQ(script_transfer->getTransferInstruction(0).amount, (uint64_t)100);
        EXPECT_EQ(script_transfer->getTransferInstruction(0).skipInput, false);
        EXPECT_EQ(script_transfer->getTransferInstruction(0).outputIndex, 0);
        EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(0).rawAmount), "2012");
        EXPECT_EQ(script_transfer->getTransferInstruction(0).firstRawByte, 0);

        EXPECT_EQ(script_transfer->getTransferInstruction(1).amount, (uint64_t)999965200);
        EXPECT_EQ(script_transfer->getTransferInstruction(1).skipInput, false);
        EXPECT_EQ(script_transfer->getTransferInstruction(1).outputIndex, 1);
        EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(1).rawAmount),
                  "69895242");
        EXPECT_EQ(script_transfer->getTransferInstruction(1).firstRawByte, 1);
        EXPECT_EQ(tx.GetHash().ToString(),
                  "006bd375946e903aa20aced1b411d61d14175488650e1deab3cb5ff8f354467d");

        ////////////////////////////////////////////

        NTP1Transaction ntp1tx;
        EXPECT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx));
        EXPECT_EQ(ntp1tx.getTxInCount(), (unsigned)2);
        // inputs are unknown, so no more tests
        EXPECT_EQ(ntp1tx.getTxOutCount(), (unsigned)4);
        EXPECT_EQ(ntp1tx.getTxOut(0).getNumOfTokens(), (unsigned)1);
        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), (uint64_t)100);
        EXPECT_EQ(ntp1tx.getTxOut(1).getNumOfTokens(), (unsigned)1);
        EXPECT_EQ(ntp1tx.getTxOut(1).getToken(0).getAmount(), (uint64_t)999965200);
        EXPECT_EQ(ntp1tx.getTxOut(2).getNumOfTokens(), (unsigned)0);
        EXPECT_EQ(ntp1tx.getTxOut(3).getNumOfTokens(), (unsigned)0);
        EXPECT_EQ(ntp1tx.getTxHash().ToString(),
                  "006bd375946e903aa20aced1b411d61d14175488650e1deab3cb5ff8f354467d");
    }
    {
        // burn
        string transaction =
            "01000000e49d535b04e935973056fce6856f04bdcf6f9f6c8759e495c5f9bc19d5688fe9cecc3c56c0010000006"
            "a47304402204dfb2ba718f2aea4ccf967734c976335c2a06e8c8b418e36d79287b8414517f802202128769cf45c"
            "2d4e3606020751255fb51f79147bb2ddfc350437e29a2a3f4622012103331393f9487ef4b318ae79972f3ccc84b"
            "15d0718d7e05720c454404e67d51d1affffffff05dbb77b0d5990f177f9f7a7d36657ec886653f3dec7441621d8"
            "1e9c55494803030000006b483045022100cffec58852a662899b2deefcb7891eab14a764701aed6f4a616515403"
            "91de81a02200505a50a19afb9af55c9ccb53536a745b0165c664e741cd8f210fdbb59ddd69a012103331393f948"
            "7ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffffdf712745b40af1feb73f3a0d9cffe"
            "f4101033f20c3e827344098a2f338cf3201030000006a4730440220044608b1c6ddb10a8899810d00673c81ed93"
            "03f7f72675439719e5ae54478b1d02205c4a94a74c866b6c27b56b1acb27c808bf308e36d0596158bb9fa6b23fd"
            "3bca9012103331393f9487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff98128fe8"
            "5f695f77584a02d20569674c557056e689c342a18ecd3803f2c31703030000006a473044022013a58a0ac64af79"
            "f6d2ddefe07f86afc3866d9e1eaae173d6f83872e28c913a9022069f5f056641e8818b6d9a4bfd8909f9a47e9c5"
            "811e1a2d7e8490cd94a0fddcb8012103331393f9487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d"
            "51d1affffffff0310270000000000001976a9147f5aff9c5ec060a45b8405a7b4f65fce5909773e88ac10270000"
            "000000000a6a084e540125000a1f1410270000000000001976a9143f7eb8c3da2cbe606fd5d46b11ab921170577"
            "0db88ac00000000";
        CDataStream  stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        stream >> tx;
        EXPECT_TRUE(tx.CheckTransaction());

        std::string opReturnArg;
        EXPECT_TRUE(IsTxNTP1(&tx, &opReturnArg));
        std::shared_ptr<NTP1Script>      scriptPtr = NTP1Script::ParseScript(opReturnArg);
        std::shared_ptr<NTP1Script_Burn> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Burn>(scriptPtr);
        EXPECT_NE(scriptPtr.get(), nullptr);
        EXPECT_NE(scriptPtrD.get(), nullptr);
        EXPECT_EQ(scriptPtrD->getTxType(), NTP1Script::TxType::TxType_Burn);

        NTP1Transaction ntp1tx;
        EXPECT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx));
        EXPECT_EQ(ntp1tx.getTxInCount(), (unsigned)4);
        // inputs are unknown, so no more tests
        EXPECT_EQ(ntp1tx.getTxOutCount(), (unsigned)3);
        // TODO: This transaction was not broadcast successfully
        //        EXPECT_EQ(ntp1tx.getTxOut(0).getNumOfTokens(), (unsigned)1);
        //        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), (uint64_t)10);
        //        EXPECT_EQ(ntp1tx.getTxOut(1).getNumOfTokens(), (unsigned)0);
        //        EXPECT_EQ(ntp1tx.getTxOut(2).getNumOfTokens(), (unsigned)1);
        //        EXPECT_EQ(ntp1tx.getTxOut(2).getToken(0).getAmount(), (uint64_t)999897350);
        EXPECT_EQ(ntp1tx.getTxHash().ToString(), tx.GetHash().ToString());
    }
    //    {
    //        // burn with transfer
    //        string transaction =
    //            "0100000048b1535b04e935973056fce6856f04bdcf6f9f6c8759e495c5f9bc19d5688fe9cecc3c56c0010000006"
    //            "b483045022100b004a3201d922e25579d2feba02dad95df573e5dee5efb6cc4c761348e08c580022003a860417f"
    //            "0de670b3a08df43d244aa16661f5218830bf5eb73b938050c8112a012103331393f9487ef4b318ae79972f3ccc8"
    //            "4b15d0718d7e05720c454404e67d51d1affffffff05dbb77b0d5990f177f9f7a7d36657ec886653f3dec7441621"
    //            "d81e9c55494803030000006a473044022039b3c6719b340f77a178e781a2f3bc6be0dcc78ea03ec413cc6527dff"
    //            "6abf96902204abf71cc27430089bed9c7692cd67af66ece912d67b60f401da0b2a4bfa5bc38012103331393f948"
    //            "7ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffffdf712745b40af1feb73f3a0d9cffe"
    //            "f4101033f20c3e827344098a2f338cf3201030000006a47304402201a8fdafcb5d0528eee7d3abf02ec4f9acdd6"
    //            "90c26ded4ec87d36f803daa1abd00220470b9225d0f5acb7af807af1cb43638f134492bc07d8d60f17295f9096d"
    //            "25296012103331393f9487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff98128fe8"
    //            "5f695f77584a02d20569674c557056e689c342a18ecd3803f2c31703030000006a47304402206b521b8663386ab"
    //            "faa861150ea9a1f444edf78e22682aef95791d2817177661a0220553f5c2bf0cd67053fbc3058f122522f7cb94f"
    //            "9c2bea157d4df16d1dca9f9e4a012103331393f9487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d"
    //            "51d1affffffff0310270000000000001976a9147f5aff9c5ec060a45b8405a7b4f65fce5909773e88ac10270000"
    //            "000000000a6a084e540125000a1f1410270000000000001976a9143f7eb8c3da2cbe606fd5d46b11ab921170577"
    //            "0db88ac00000000";
    //        CDataStream  stream(ParseHex(transaction), SER_NETWORK, PROTOCOL_VERSION);
    //        CTransaction tx;
    //        stream >> tx;
    //        EXPECT_TRUE(tx.CheckTransaction());

    //        std::string opReturnArg;
    //        EXPECT_TRUE(IsTxNTP1(&tx, &opReturnArg));
    //        std::shared_ptr<NTP1Script>      scriptPtr = NTP1Script::ParseScript(opReturnArg);
    //        std::shared_ptr<NTP1Script_Burn> scriptPtrD =
    //            std::dynamic_pointer_cast<NTP1Script_Burn>(scriptPtr);
    //        EXPECT_NE(scriptPtr.get(), nullptr);
    //        EXPECT_NE(scriptPtrD.get(), nullptr);
    //        EXPECT_EQ(scriptPtrD->getTxType(), NTP1Script::TxType::TxType_Burn);

    //        NTP1Transaction ntp1tx;
    //        EXPECT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx));
    //        EXPECT_EQ(ntp1tx.getTxInCount(), (unsigned)4);
    //        // inputs are unknown, so no more tests
    //        EXPECT_EQ(ntp1tx.getTxOutCount(), (unsigned)3);
    //        // TODO
    //        //        EXPECT_EQ(ntp1tx.getTxOut(0).getNumOfTokens(), (unsigned)1);
    //        //        EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), (uint64_t)10);
    //        //        EXPECT_EQ(ntp1tx.getTxOut(1).getNumOfTokens(), (unsigned)0);
    //        //        EXPECT_EQ(ntp1tx.getTxOut(2).getNumOfTokens(), (unsigned)1);
    //        //        EXPECT_EQ(ntp1tx.getTxOut(2).getToken(0).getAmount(), (uint64_t)999897350);
    //        EXPECT_EQ(ntp1tx.getTxHash().ToString(),
    //                  "008d329611fcbdb82b4adb097c29f1d6a56707bfb232c8c124390756e80a9e44");
    //    }
}

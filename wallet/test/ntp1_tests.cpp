#include "googletest/googletest/include/gtest/gtest.h"

#include "curltools.h"
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
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

const std::string TempNTP1File("ntp1txout.bin");

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
    EXPECT_EQ(tx_good.getSequence(), static_cast<uint64_t>(4294967295));
    EXPECT_EQ(tx_good.getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getToken(0).getTokenId(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getToken(0).getDivisibility(), static_cast<unsigned>(7));
    EXPECT_EQ(tx_good.getToken(0).getAmount(), static_cast<unsigned>(997000));
    EXPECT_EQ(tx_good.getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getToken(0).getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
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
    EXPECT_EQ(tx_good.tokenCount(), (unsigned long)1);
    EXPECT_EQ(tx_good.getToken(0).getTokenId(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getToken(0).getDivisibility(), static_cast<unsigned>(7));
    EXPECT_EQ(tx_good.getToken(0).getAmount(), static_cast<unsigned>(1000));
    EXPECT_EQ(tx_good.getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getToken(0).getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
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
    EXPECT_EQ(token_good.getTokenId(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(token_good.getDivisibility(), static_cast<unsigned>(7));
    EXPECT_EQ(token_good.getAmount(), static_cast<unsigned>(512345));
    EXPECT_EQ(token_good.getLockStatus(), true);
    EXPECT_EQ(token_good.getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
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
    EXPECT_EQ(tx_good.getLockTime(), static_cast<uint64_t>(0));
    EXPECT_EQ(tx_good.getTime(), static_cast<uint64_t>(1520323876000));
    EXPECT_EQ(tx_good.getTxHash().ToString(),
              "1ec95ea69b16385da91a87af7c60bc625d4b96416a45f5f95f47e4d07c19aebc");

    EXPECT_EQ(tx_good.getTxInCount(), (unsigned long)2);
    EXPECT_EQ(tx_good.getTxIn(0).getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxIn(1).getNumOfTokens(), (unsigned long)0);

    EXPECT_EQ(tx_good.getTxIn(0).getOutPoint().getHash().ToString(),
              "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getTxIn(0).getOutPoint().getIndex(), static_cast<unsigned>(1));
    EXPECT_EQ(tx_good.getTxIn(0).getScriptSigHex(),
              "483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fc"
              "a8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162"
              "e6fdd4e8e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getTxIn(0).getSequence(), static_cast<uint64_t>(4294967295));

    EXPECT_EQ(tx_good.getTxIn(1).getOutPoint().getHash().ToString(),
              "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getTxIn(1).getOutPoint().getIndex(), static_cast<unsigned>(3));
    EXPECT_EQ(tx_good.getTxIn(1).getScriptSigHex(),
              "47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3b"
              "aaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6"
              "fdd4e8e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getTxIn(1).getSequence(), static_cast<uint64_t>(4294967295));

    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getAmount(), static_cast<uint64_t>(997000));
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getDivisibility(), static_cast<uint64_t>(7));
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getTokenId(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");

    EXPECT_EQ(tx_good.getTxOutCount(), (unsigned long)4);
    EXPECT_EQ(tx_good.getTxOut(0).tokenCount(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxOut(1).tokenCount(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxOut(2).tokenCount(), (unsigned long)0);
    EXPECT_EQ(tx_good.getTxOut(3).tokenCount(), (unsigned long)0);

    EXPECT_EQ(tx_good.getTxOut(0).getScriptPubKeyHex(),
              "76a914930b31797c0e6f0d4239909b044aaadfde37199588ac");
    EXPECT_EQ(tx_good.getTxOut(0).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getAmount(), static_cast<uint64_t>(1000));
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getDivisibility(), static_cast<uint64_t>(7));
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getTokenId(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getTxOut(0).getAddress(), "NZKTvgBXGBFDde73TizBmxVPzNauT1ivxV");
    EXPECT_EQ(tx_good.getTxOut(0).getType(), NTP1TxOut::OutputType::NormalOutput);

    EXPECT_EQ(tx_good.getTxOut(1).getScriptPubKeyHex(),
              "76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac");
    EXPECT_EQ(tx_good.getTxOut(1).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getAmount(), static_cast<uint64_t>(996000));
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getDivisibility(), static_cast<uint64_t>(7));
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getIssueTxId().ToString(),
              "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getTokenId(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getTxOut(1).getAddress(), "NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D");
    EXPECT_EQ(tx_good.getTxOut(1).getType(), NTP1TxOut::OutputType::NormalOutput);

    EXPECT_EQ(tx_good.getTxOut(2).getScriptPubKeyHex(), "6a0b4e54011500201301403e43");
    EXPECT_EQ(tx_good.getTxOut(2).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(2).tokenCount(), 0u);
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

    EXPECT_EQ(tokenMetaData.getTokenId(), "La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx");
    EXPECT_EQ(tokenMetaData.getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
    EXPECT_EQ(tokenMetaData.getLockStatus(), true);
    EXPECT_EQ(tokenMetaData.getDivisibility(), static_cast<unsigned>(7));
    EXPECT_EQ(tokenMetaData.getFirstBlock(), static_cast<unsigned>(159746));
    EXPECT_EQ(tokenMetaData.getNumOfBurns(), static_cast<unsigned>(1));
    EXPECT_EQ(tokenMetaData.getNumOfIssuance(), static_cast<unsigned>(1));
    EXPECT_EQ(tokenMetaData.getNumOfTransfers(), static_cast<unsigned>(1679));
    EXPECT_EQ(tokenMetaData.getNumOfHolders(), static_cast<unsigned>(8281));
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
    EXPECT_EQ(tokenMetaData.getTotalSupply(), static_cast<unsigned>(1607509562));
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

    EXPECT_EQ(tokenMetaData.getTokenId(), "La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx");
    EXPECT_EQ(tokenMetaData.getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
    EXPECT_EQ(tokenMetaData.getLockStatus(), true);
    EXPECT_EQ(tokenMetaData.getDivisibility(), static_cast<unsigned>(7));
    EXPECT_EQ(tokenMetaData.getFirstBlock(), static_cast<unsigned>(159746));
    EXPECT_EQ(tokenMetaData.getNumOfBurns(), static_cast<unsigned>(1));
    EXPECT_EQ(tokenMetaData.getNumOfIssuance(), static_cast<unsigned>(1));
    EXPECT_EQ(tokenMetaData.getNumOfTransfers(), static_cast<unsigned>(1679));
    EXPECT_EQ(tokenMetaData.getNumOfHolders(), static_cast<unsigned>(8281));
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
    EXPECT_EQ(tokenMetaData.getTotalSupply(), static_cast<unsigned>(1607509562));

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
    EXPECT_NO_THROW(wallet1.importFromFile(Path(TEST_ROOT_PATH) / Path("/data/NTP1DataCache.json")));
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
    EXPECT_EQ(NTP1AmountHexToNumber<int64_t>("2011"), 10);
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

    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(999901700), "69892a92");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(8478457292922), "c007b60b6f687a");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(38290000), "40ef54");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(1000000000000000), "201f");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(723782), "60b0b460");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(871340), "5545e1");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(8478457292922), "c007b60b6f687a");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(17), "11");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(100), "2012");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(479320), "4bb3c1");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(9207387000), "68c7e5b3");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(8723709100), "8029990f1a");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(839027891720), "a09c47f7b1a1");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(182582987368701), "c0a60eea1aa8fd");

    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(999999999997990), "c38d7ea4c67826");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(276413656646664), "c0fb6591d0c408");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(9731165496688), "c008d9b6a9a570");
    EXPECT_EQ(NumberToHexNTP1Amount<uint64_t>(943721684679640), "c35a4f53c83bd8");
}

TEST(ntp1_tests, script_transfer)
{
    // transfer some tokens
    const std::string                    toParse_transfer = "4e5401150069892a92";
    std::shared_ptr<NTP1Script>          script           = NTP1Script::ParseScript(toParse_transfer);
    std::shared_ptr<NTP1Script_Transfer> script_transfer =
        std::dynamic_pointer_cast<NTP1Script_Transfer>(script);
    EXPECT_EQ(script_transfer->getHeader(), boost::algorithm::unhex(toParse_transfer.substr(0, 6)));
    EXPECT_EQ(script_transfer->getHexMetadata().size(), static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getOpCodeBin()), "15");
    EXPECT_EQ(script_transfer->getTxType(), NTP1Script::TxType::TxType_Transfer);

    EXPECT_EQ(script_transfer->getTransferInstructionsCount(), static_cast<unsigned>(1));
    EXPECT_EQ(script_transfer->getTransferInstruction(0).amount, static_cast<uint64_t>(999901700));
    EXPECT_EQ(script_transfer->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(0).outputIndex, static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(0).rawAmount), "69892A92");
    EXPECT_EQ(script_transfer->getTransferInstruction(0).firstRawByte, 0);
}

TEST(ntp1_tests, script_issuance_allowed_chars_in_token_symbol)
{
    std::string                 toParse_issuance;
    std::shared_ptr<NTP1Script> script;
    {
        // NIBBL name 4e4942424c
        toParse_issuance = "4e5401014e4942424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
                           "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        EXPECT_NO_THROW(script = NTP1Script::ParseScript(toParse_issuance));
    }
    {
        // -IBBL name
        toParse_issuance = "4e5401012d4942424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
                           "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), runtime_error);
    }
    {
        // NI~BL name 4e497e424c
        toParse_issuance = "4e5401014e497e424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
                           "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), runtime_error);
    }
    {
        // NIBB. name 4e4942422e
        toParse_issuance = "4e5401014e4942422eab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
                           "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), runtime_error);
    }

    // a string of all invalid characters from the ascii table
    std::string invalid_chars;
    std::string valid_chars;
    // append chars up to ascii 0
    for (char i = 0; i <= 47; i++) {
        if (i == 32) // space is allowed as padding
            continue;
        invalid_chars.push_back(i);
    }
    // from > 9 to < A
    for (char i = 58; i <= 64; i++) {
        invalid_chars.push_back(i);
    }
    // from > Z to < a
    for (char i = 91; i <= 96; i++) {
        invalid_chars.push_back(i);
    }
    // from > z to <= 0xff
    for (int i = 123; i <= 0xff; i++) {
        invalid_chars.push_back(static_cast<char>(i));
    }

    valid_chars.push_back(32); // space
    // append digital chars
    for (char i = 48; i <= 57; i++) {
        valid_chars.push_back(i);
    }

    // append A-Z
    for (char i = 65; i <= 90; i++) {
        valid_chars.push_back(i);
    }

    // append a-z
    for (char i = 97; i <= 122; i++) {
        valid_chars.push_back(i);
    }

    {
        // we'll test all invalid characters by replacing a random character from NIBBL with an invalid
        // character
        std::string to_parse_prefix = "4e540101";
        std::string to_parse_suffix =
            "ab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
            "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        for (const auto& c : invalid_chars) {
            std::string  tokenName                   = "4e4942424c"; // NIBBL
            std::string  tokenNameRaw                = boost::algorithm::unhex(tokenName);
            unsigned int random_char_num_to_replace  = rand() % 5;
            tokenNameRaw[random_char_num_to_replace] = c; // replace one char with an invalid one

            // No exception is thrown before replacing the char with an invalid char
            toParse_issuance = to_parse_prefix + tokenName + to_parse_suffix;
            EXPECT_NO_THROW(script = NTP1Script::ParseScript(toParse_issuance));

            // Exception is thrown after replacing the char with an invalid char
            tokenName = boost::algorithm::hex(tokenNameRaw); // new name with one invalid character
            toParse_issuance = to_parse_prefix + tokenName + to_parse_suffix;
            EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), runtime_error)
                << "The char \"" << boost::algorithm::hex(std::string(1, c))
                << "\" in a token name didn't throw an exception";
        }
    }

    {
        // we'll test now all valid characters by replacing a random character from NIBBL with a valid
        // character
        std::string to_parse_prefix = "4e540101";
        std::string to_parse_suffix =
            "ab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
            "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        for (const auto& c : valid_chars) {
            std::string  tokenName                   = "4e4942424c"; // NIBBL
            std::string  tokenNameRaw                = boost::algorithm::unhex(tokenName);
            unsigned int random_char_num_to_replace  = rand() % 5;
            tokenNameRaw[random_char_num_to_replace] = c; // replace one char with an invalid one

            // No exception is thrown before
            toParse_issuance = to_parse_prefix + tokenName + to_parse_suffix;
            EXPECT_NO_THROW(script = NTP1Script::ParseScript(toParse_issuance));

            // No exception is thrown after putting in another valid char
            tokenName = boost::algorithm::hex(tokenNameRaw); // new name with one invalid character
            toParse_issuance = to_parse_prefix + tokenName + to_parse_suffix;
            EXPECT_NO_THROW(script = NTP1Script::ParseScript(toParse_issuance))
                << "The char \"" << boost::algorithm::hex(std::string(1, c))
                << "\" in a token name didn't throw an exception";
        }
    }

    {
        // ensure that all characters were tested, from 0x0 to 0xff
        std::unordered_set<char> allChars;
        allChars.insert(invalid_chars.begin(), invalid_chars.end());
        allChars.insert(valid_chars.begin(), valid_chars.end());
        EXPECT_EQ(invalid_chars.size() + valid_chars.size(), static_cast<unsigned>(256));
        EXPECT_EQ(allChars.size(), static_cast<unsigned>(256));
    }
}

TEST(ntp1_tests, script_issuance)
{
    {
        // issue NIBBL
        // txid: 66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80
        // 4e5401 01
        // 4e4942424c
        // ab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef215620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0
        std::string toParse_issuance =
            "4e5401014e4942424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
            "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(toParse_issuance);
        std::shared_ptr<NTP1Script_Issuance> script_issuance =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(script);
        EXPECT_NE(script_issuance.get(), nullptr);
        EXPECT_EQ(script_issuance->getAggregationPolicy(),
                  NTP1Script::IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable);
        EXPECT_EQ(script_issuance->getAmount(), static_cast<uint64_t>(1000000000));
        EXPECT_EQ(script_issuance->getDivisibility(), 7);
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getHeader()), "4E5401");
        EXPECT_EQ(script_issuance->getHexMetadata(), "AB10C04E20E0AEC73D58C8FBF2A9C26A6DC3ED666C7B80FEF2"
                                                     "15620C817703B1E5D8B1870211CE7CDF50718B4789245FB80F"
                                                     "5899");
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getOpCodeBin()), "01");
        EXPECT_EQ(script_issuance->getTokenSymbol(), "NIBBL");
        EXPECT_EQ(script_issuance->getTxType(), NTP1Script::TxType::TxType_Issuance);

        EXPECT_EQ(script_issuance->getTransferInstructionsCount(), static_cast<unsigned>(1));
        EXPECT_EQ(script_issuance->getTransferInstruction(0).amount, static_cast<uint64_t>(1000000000));
        EXPECT_EQ(script_issuance->getTransferInstruction(0).skipInput, false);
        EXPECT_EQ(script_issuance->getTransferInstruction(0).outputIndex, static_cast<unsigned>(0));
        EXPECT_EQ(boost::algorithm::hex(script_issuance->getTransferInstruction(0).rawAmount), "2019");
        EXPECT_EQ(script_issuance->getTransferInstruction(0).firstRawByte, 0);
        EXPECT_EQ(script_issuance->getTokenID(
                      "c55dd5271dd8a9aa35a7f6c393a0eb24bcc50116bf15b49f4e760d3e9a138120", 1),
                  "LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    }
}

TEST(ntp1_tests, script_burn)
{
    // burn some tokens
    std::string                      toParse_burn = "4e5401251f2013";
    std::shared_ptr<NTP1Script>      script       = NTP1Script::ParseScript(toParse_burn);
    std::shared_ptr<NTP1Script_Burn> script_burn  = std::dynamic_pointer_cast<NTP1Script_Burn>(script);
    EXPECT_EQ(script_burn->getHeader(), boost::algorithm::unhex(toParse_burn.substr(0, 6)));
    EXPECT_EQ(script_burn->getHexMetadata().size(), static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_burn->getOpCodeBin()), "25");
    EXPECT_EQ(script_burn->getTxType(), NTP1Script::TxType::TxType_Burn);

    EXPECT_EQ(script_burn->getTransferInstructionsCount(), static_cast<unsigned>(1));
    EXPECT_EQ(script_burn->getTransferInstruction(0).amount, static_cast<uint64_t>(1000));
    EXPECT_EQ(script_burn->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_burn->getTransferInstruction(0).outputIndex, static_cast<unsigned>(31));
    EXPECT_EQ(boost::algorithm::hex(script_burn->getTransferInstruction(0).rawAmount), "2013");
    EXPECT_EQ(script_burn->getTransferInstruction(0).firstRawByte, 31);
}

TEST(ntp1_tests, script_get_amount_size)
{
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("11"))[0]),
              static_cast<unsigned>(1));
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("2012"))[0]),
              static_cast<unsigned>(2));
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("4bb3c1"))[0]),
              static_cast<unsigned>(3));
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("68c7e5b3"))[0]),
              static_cast<unsigned>(4));
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("8029990f1a"))[0]),
              static_cast<unsigned>(5));
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("a09c47f7b1a1"))[0]),
              static_cast<unsigned>(6));
    EXPECT_EQ(NTP1Script::CalculateAmountSize(boost::algorithm::unhex(std::string("c0a60eea1aa8fd"))[0]),
              static_cast<unsigned>(7));
}

CTransaction TxFromHex(const std::string& hex)
{
    CDataStream  stream(ParseHex(hex), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    return tx;
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_issuance)
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
    CTransaction tx = TxFromHex(transaction);
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
    EXPECT_EQ(script_issuance->getAmount(), static_cast<uint64_t>(1000000000));
    EXPECT_EQ(script_issuance->getDivisibility(), 7);
    EXPECT_EQ(boost::algorithm::hex(script_issuance->getHeader()), "4E5401");
    EXPECT_EQ(script_issuance->getHexMetadata(), "AB10C04E20E0AEC73D58C8FBF2A9C26A6DC3ED666C7B80FEF2"
                                                 "15620C817703B1E5D8B1870211CE7CDF50718B4789245FB80F"
                                                 "5899");
    EXPECT_EQ(script_issuance->getTokenID(
                  "c55dd5271dd8a9aa35a7f6c393a0eb24bcc50116bf15b49f4e760d3e9a138120", 1),
              "LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");

    EXPECT_EQ(boost::algorithm::hex(script_issuance->getOpCodeBin()), "01");
    EXPECT_EQ(script_issuance->getTokenSymbol(), "NIBBL");
    EXPECT_EQ(script_issuance->getTxType(), NTP1Script::TxType::TxType_Issuance);

    EXPECT_EQ(script_issuance->getTransferInstructionsCount(), static_cast<unsigned>(1));
    EXPECT_EQ(script_issuance->getTransferInstruction(0).amount, static_cast<uint64_t>(1000000000));
    EXPECT_EQ(script_issuance->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_issuance->getTransferInstruction(0).outputIndex, static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_issuance->getTransferInstruction(0).rawAmount), "2019");
    EXPECT_EQ(script_issuance->getTransferInstruction(0).firstRawByte, 0);
    EXPECT_EQ(tx.GetHash().ToString(),
              "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");

    std::string vinA = "0100000089f3995a013458f5fa9bc91103a1dcdd6f1e582bc35e3ca513a924bad1e3098dd540f"
                       "b4e030100000049483045022100e8dedce5f1950a07dbbd60dfb959e21113523b9d98df2fc197"
                       "3e530beafd53b3022047450cd1a16d74f83c2ba769cb0a0ba28c848e440cc915d268218e7b8d0"
                       "e541701ffffffff02306a04c2210000001976a91494229f861ecf642374f132de7cc739f314ee"
                       "4ada88ac008c8647000000001976a9144e2a50f7e8c58ff9a0175f95616a1657b49a06a888ac0"
                       "0000000";
    CTransaction txVinA = TxFromHex(vinA);

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs{{txVinA, NTP1Transaction()}};
    ////////////////////////////////////////////////////

    for (auto&& input : inputs) {
        input.second.readNTP1DataFromTx_minimal(input.first);
    }

    NTP1Transaction ntp1tx;
    EXPECT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx, inputs));
    EXPECT_EQ(ntp1tx.getTxInCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxIn(0).getNumOfTokens(), static_cast<unsigned>(0));
    EXPECT_EQ(ntp1tx.getTxOutCount(), static_cast<unsigned>(3));
    EXPECT_EQ(ntp1tx.getTxOut(0).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(1).tokenCount(), static_cast<unsigned>(0));
    EXPECT_EQ(ntp1tx.getTxOut(2).tokenCount(), static_cast<unsigned>(0));
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), static_cast<unsigned>(1000000000));
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getDivisibility(), static_cast<unsigned>(7));
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getIssueTxId().ToString(),
              "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    EXPECT_TRUE(ntp1tx.getTxOut(0).getToken(0).getLockStatus());
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getTokenSymbol(), "NIBBL");
    EXPECT_EQ(ntp1tx.getTxHash().ToString(),
              "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getTokenId(), "LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");

    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getHash().ToString(),
              "c55dd5271dd8a9aa35a7f6c393a0eb24bcc50116bf15b49f4e760d3e9a138120");
    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getIndex(), static_cast<unsigned>(1));

    /// READ AND WRITE
    ///
    std::remove(TempNTP1File.c_str());
    {
        // will be freed automatically by writeFromDisk
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;
        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
    {
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;
        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_transfer_1)
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
    CTransaction tx = TxFromHex(transaction);
    EXPECT_TRUE(tx.CheckTransaction());

    std::string opReturnArg;
    EXPECT_TRUE(IsTxNTP1(&tx, &opReturnArg));
    std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(opReturnArg);
    std::shared_ptr<NTP1Script_Transfer> script_transfer =
        std::dynamic_pointer_cast<NTP1Script_Transfer>(script);
    EXPECT_EQ(script_transfer->getHeader(), boost::algorithm::unhex(opReturnArg.substr(0, 6)));
    EXPECT_EQ(script_transfer->getHexMetadata().size(), static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getOpCodeBin()), "15");
    EXPECT_EQ(script_transfer->getTxType(), NTP1Script::TxType::TxType_Transfer);

    EXPECT_EQ(script_transfer->getTransferInstructionsCount(), static_cast<unsigned>(2));

    EXPECT_EQ(script_transfer->getTransferInstruction(0).amount, static_cast<uint64_t>(100));
    EXPECT_EQ(script_transfer->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(0).outputIndex, static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(0).rawAmount), "2012");
    EXPECT_EQ(script_transfer->getTransferInstruction(0).firstRawByte, 0);

    EXPECT_EQ(script_transfer->getTransferInstruction(1).amount, static_cast<uint64_t>(999965200));
    EXPECT_EQ(script_transfer->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(1).outputIndex, static_cast<unsigned>(1));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(1).rawAmount), "69895242");
    EXPECT_EQ(script_transfer->getTransferInstruction(1).firstRawByte, 1);
    EXPECT_EQ(tx.GetHash().ToString(),
              "006bd375946e903aa20aced1b411d61d14175488650e1deab3cb5ff8f354467d");

    /// Input 0

    std::string vinA =
        "01000000d654a35a02b5b4c0f4608d996f7ccf68a74c7832b151fed86376defd0be51570f6695ea42a010000006"
        "b483045022100ecbaf16008ccb4c7084d2e28b875adbfcb8cb1012e305ae503b646a075693f340220404eb96ab6"
        "0496c8d52a06a4286e2916e9f70304b1ff3f1663c938cae7f08f01012103331393f9487ef4b318ae79972f3ccc8"
        "4b15d0718d7e05720c454404e67d51d1affffffffebd3fcb84b4019229f8c85e5d0c736f4eb17637cce245369d6"
        "a7b51b56a2026b030000006b483045022100b02f6f5d4c0b2e88d5d7226e0f22ed37938b79e242b0f8d6260b14e"
        "00ab728120220549ecc61e8318105ad9255a54af4ee0966810e42fbf84a9fa7ef7c4892c41c35012103331393f9"
        "487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a9148"
        "6061d16eafa0ea7a6be8875fb5bbc09a5f210a588ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b"
        "11ab9211705770db88ac10270000000000000e6a0c4e5401150020120169895252409c0000000000001976a9143"
        "f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinA = TxFromHex(vinA);
    NTP1Transaction ntp1txVinA;

    NTP1TokenTxData vinA_vout0_token0;
    vinA_vout0_token0.setAggregationPolicy("aggregable");
    vinA_vout0_token0.setAmount(100);
    vinA_vout0_token0.setDivisibility(7);
    vinA_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinA_vout0_token0.setLockStatus(true);
    vinA_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinA_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinA_vout0;
    vinA_vout0.__manualSet(
        10000, "76a91486061d16eafa0ea7a6be8875fb5bbc09a5f210a588ac",
        "OP_DUP OP_HASH160 86061d16eafa0ea7a6be8875fb5bbc09a5f210a5 OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinA_vout0_token0}), "NY8d4F6EZQH1y5KoqvdybTUgfwAMUYW3qF");

    NTP1TokenTxData vinA_vout1_token0;
    vinA_vout1_token0.setAggregationPolicy("aggregable");
    vinA_vout1_token0.setAmount(999965300);
    vinA_vout1_token0.setDivisibility(7);
    vinA_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinA_vout1_token0.setLockStatus(true);
    vinA_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinA_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinA_vout1;
    vinA_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinA_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinA_vout2;
    vinA_vout2.__manualSet(10000, "6a0c4e5401150020120169895252", "OP_RETURN 4e5401150020120169895252",
                           std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinA_vout3;
    vinA_vout3.__manualSet(
        40000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinA.__manualSet(1,
                           uint256("1766a9150953392de523f9420a2e32f993bb572e79f465de78ef96831494b347"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinA_vout0, vinA_vout1, vinA_vout2, vinA_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    /// Input 1

    std::string vinB =
        "010000007c3fa35a0220b900bf90ba01c5e0b560261871c35ec84b2bcf1bb6a4c38f76554a53d9378e010000006"
        "b483045022100fa4e87b9bc64d12b757a1ddd8a680239e8b11c8ca6496a212ffa726b0b79c1c202204e39d5776e"
        "c175580429acdf7f2d80b6ee42c845c64c884f70138811c6931db0012103331393f9487ef4b318ae79972f3ccc8"
        "4b15d0718d7e05720c454404e67d51d1affffffff04946f748b2ec8662ad584e09f6408636effbadf7dc59b168a"
        "50d028aa0f827a010000006b483045022100c04d60412ff55eae4e735ca2022f84d797a72156c26523a55579f5e"
        "8f8e7ffd702201c1e361451496b00ef8d47cd0e11fb2a30aca335583bf53b76fec3951a4d5576012103331393f9"
        "487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a9146"
        "732468b6fe071d7004a5d9bddaacc3a71423acd88ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b"
        "11ab9211705770db88ac10270000000000000e6a0c4e5401150020120169895c3270110100000000001976a9143"
        "f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinB = TxFromHex(vinB);
    NTP1Transaction ntp1txVinB;

    NTP1TokenTxData vinB_vout0_token0;
    vinB_vout0_token0.setAggregationPolicy("aggregable");
    vinB_vout0_token0.setAmount(100);
    vinB_vout0_token0.setDivisibility(7);
    vinB_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinB_vout0_token0.setLockStatus(true);
    vinB_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinB_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinB_vout0;
    vinB_vout0.__manualSet(
        10000, "76a9146732468b6fe071d7004a5d9bddaacc3a71423acd88ac",
        "OP_DUP OP_HASH160 6732468b6fe071d7004a5d9bddaacc3a71423acd OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinB_vout0_token0}), "NVKd1iq2UF8RZThEN4MDGMBcKnFPnuPF6v");

    NTP1TokenTxData vinB_vout1_token0;
    vinB_vout1_token0.setAggregationPolicy("aggregable");
    vinB_vout1_token0.setAmount(999981100);
    vinB_vout1_token0.setDivisibility(7);
    vinB_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinB_vout1_token0.setLockStatus(true);
    vinB_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinB_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinB_vout1;
    vinB_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinB_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinB_vout2;
    vinB_vout2.__manualSet(10000, "6a0c4e5401150020120169895c32", "OP_RETURN 4e5401150020120169895c32",
                           std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinB_vout3;
    vinB_vout3.__manualSet(
        70000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinB.__manualSet(1,
                           uint256("111481401fbd842c5aa1b9420db8c5ef7e94d9ac3b3d3b7e2b2cb6c7b0650612"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinB_vout0, vinB_vout1, vinB_vout2, vinB_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs{std::make_pair(txVinA, ntp1txVinA),
                                                                 std::make_pair(txVinB, ntp1txVinB)};

    ////////////////////////////////////////////

    NTP1Transaction ntp1tx;
    ASSERT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx, inputs));
    EXPECT_EQ(ntp1tx.getTxInCount(), static_cast<unsigned>(2));
    // inputs are unknown, so no more tests
    EXPECT_EQ(ntp1tx.getTxOutCount(), static_cast<unsigned>(4));
    EXPECT_EQ(ntp1tx.getTxOut(0).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), static_cast<uint64_t>(100));
    EXPECT_EQ(ntp1tx.getTxOut(1).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(1).getToken(0).getAmount(), static_cast<uint64_t>(999965200));
    EXPECT_EQ(ntp1tx.getTxOut(2).tokenCount(), static_cast<unsigned>(0));
    EXPECT_EQ(ntp1tx.getTxOut(3).tokenCount(), static_cast<unsigned>(0));
    EXPECT_EQ(ntp1tx.getTxHash().ToString(),
              "006bd375946e903aa20aced1b411d61d14175488650e1deab3cb5ff8f354467d");

    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getHash().ToString(),
              "1766a9150953392de523f9420a2e32f993bb572e79f465de78ef96831494b347");
    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getIndex(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxIn(1).getPrevout().getHash().ToString(),
              "111481401fbd842c5aa1b9420db8c5ef7e94d9ac3b3d3b7e2b2cb6c7b0650612");
    EXPECT_EQ(ntp1tx.getTxIn(1).getPrevout().getIndex(), static_cast<unsigned>(3));

    /// READ AND WRITE
    ///
    {
        // will be freed automatically by writeFromDisk
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;
        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
    {
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;
        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_transfer_2_with_change)
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
    CTransaction tx = TxFromHex(transaction);
    EXPECT_TRUE(tx.CheckTransaction());

    std::string opReturnArg;
    EXPECT_TRUE(IsTxNTP1(&tx, &opReturnArg));
    std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(opReturnArg);
    std::shared_ptr<NTP1Script_Transfer> script_transfer =
        std::dynamic_pointer_cast<NTP1Script_Transfer>(script);
    EXPECT_EQ(script_transfer->getHeader(), boost::algorithm::unhex(opReturnArg.substr(0, 6)));
    EXPECT_EQ(script_transfer->getHexMetadata().size(), static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getOpCodeBin()), "15");
    EXPECT_EQ(script_transfer->getTxType(), NTP1Script::TxType::TxType_Transfer);

    EXPECT_EQ(script_transfer->getTransferInstructionsCount(), static_cast<unsigned>(2));

    EXPECT_EQ(script_transfer->getTransferInstruction(0).amount, static_cast<uint64_t>(100));
    EXPECT_EQ(script_transfer->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(0).outputIndex, static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(0).rawAmount), "2012");
    EXPECT_EQ(script_transfer->getTransferInstruction(0).firstRawByte, 0);

    EXPECT_EQ(script_transfer->getTransferInstruction(1).amount, static_cast<uint64_t>(999965200));
    EXPECT_EQ(script_transfer->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(1).outputIndex, static_cast<unsigned>(1));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(1).rawAmount), "69895242");
    EXPECT_EQ(script_transfer->getTransferInstruction(1).firstRawByte, 1);
    EXPECT_EQ(tx.GetHash().ToString(),
              "006bd375946e903aa20aced1b411d61d14175488650e1deab3cb5ff8f354467d");

    /// Input 0

    std::string vinA =
        "01000000d654a35a02b5b4c0f4608d996f7ccf68a74c7832b151fed86376defd0be51570f6695ea42a010000006"
        "b483045022100ecbaf16008ccb4c7084d2e28b875adbfcb8cb1012e305ae503b646a075693f340220404eb96ab6"
        "0496c8d52a06a4286e2916e9f70304b1ff3f1663c938cae7f08f01012103331393f9487ef4b318ae79972f3ccc8"
        "4b15d0718d7e05720c454404e67d51d1affffffffebd3fcb84b4019229f8c85e5d0c736f4eb17637cce245369d6"
        "a7b51b56a2026b030000006b483045022100b02f6f5d4c0b2e88d5d7226e0f22ed37938b79e242b0f8d6260b14e"
        "00ab728120220549ecc61e8318105ad9255a54af4ee0966810e42fbf84a9fa7ef7c4892c41c35012103331393f9"
        "487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a9148"
        "6061d16eafa0ea7a6be8875fb5bbc09a5f210a588ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b"
        "11ab9211705770db88ac10270000000000000e6a0c4e5401150020120169895252409c0000000000001976a9143"
        "f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinA = TxFromHex(vinA);
    NTP1Transaction ntp1txVinA;

    NTP1TokenTxData vinA_vout0_token0;
    vinA_vout0_token0.setAggregationPolicy("aggregable");
    vinA_vout0_token0.setAmount(100);
    vinA_vout0_token0.setDivisibility(7);
    vinA_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinA_vout0_token0.setLockStatus(true);
    vinA_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinA_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinA_vout0;
    vinA_vout0.__manualSet(
        10000, "76a91486061d16eafa0ea7a6be8875fb5bbc09a5f210a588ac",
        "OP_DUP OP_HASH160 86061d16eafa0ea7a6be8875fb5bbc09a5f210a5 OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinA_vout0_token0}), "NY8d4F6EZQH1y5KoqvdybTUgfwAMUYW3qF");

    NTP1TokenTxData vinA_vout1_token0;
    vinA_vout1_token0.setAggregationPolicy("aggregable");
    vinA_vout1_token0.setAmount(999965500);
    vinA_vout1_token0.setDivisibility(7);
    vinA_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinA_vout1_token0.setLockStatus(true);
    vinA_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinA_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinA_vout1;
    vinA_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinA_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinA_vout2;
    vinA_vout2.__manualSet(10000, "6a0c4e5401150020120169895252", "OP_RETURN 4e5401150020120169895252",
                           std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinA_vout3;
    vinA_vout3.__manualSet(
        40000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinA.__manualSet(1,
                           uint256("1766a9150953392de523f9420a2e32f993bb572e79f465de78ef96831494b347"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinA_vout0, vinA_vout1, vinA_vout2, vinA_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    /// Input 1

    std::string vinB =
        "010000007c3fa35a0220b900bf90ba01c5e0b560261871c35ec84b2bcf1bb6a4c38f76554a53d9378e010000006"
        "b483045022100fa4e87b9bc64d12b757a1ddd8a680239e8b11c8ca6496a212ffa726b0b79c1c202204e39d5776e"
        "c175580429acdf7f2d80b6ee42c845c64c884f70138811c6931db0012103331393f9487ef4b318ae79972f3ccc8"
        "4b15d0718d7e05720c454404e67d51d1affffffff04946f748b2ec8662ad584e09f6408636effbadf7dc59b168a"
        "50d028aa0f827a010000006b483045022100c04d60412ff55eae4e735ca2022f84d797a72156c26523a55579f5e"
        "8f8e7ffd702201c1e361451496b00ef8d47cd0e11fb2a30aca335583bf53b76fec3951a4d5576012103331393f9"
        "487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a9146"
        "732468b6fe071d7004a5d9bddaacc3a71423acd88ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b"
        "11ab9211705770db88ac10270000000000000e6a0c4e5401150020120169895c3270110100000000001976a9143"
        "f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinB = TxFromHex(vinB);
    NTP1Transaction ntp1txVinB;

    NTP1TokenTxData vinB_vout0_token0;
    vinB_vout0_token0.setAggregationPolicy("aggregable");
    vinB_vout0_token0.setAmount(100);
    vinB_vout0_token0.setDivisibility(7);
    vinB_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinB_vout0_token0.setLockStatus(true);
    vinB_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinB_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinB_vout0;
    vinB_vout0.__manualSet(
        10000, "76a9146732468b6fe071d7004a5d9bddaacc3a71423acd88ac",
        "OP_DUP OP_HASH160 6732468b6fe071d7004a5d9bddaacc3a71423acd OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinB_vout0_token0}), "NVKd1iq2UF8RZThEN4MDGMBcKnFPnuPF6v");

    NTP1TokenTxData vinB_vout1_token0;
    vinB_vout1_token0.setAggregationPolicy("aggregable");
    vinB_vout1_token0.setAmount(999981100);
    vinB_vout1_token0.setDivisibility(7);
    vinB_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinB_vout1_token0.setLockStatus(true);
    vinB_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinB_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinB_vout1;
    vinB_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinB_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinB_vout2;
    vinB_vout2.__manualSet(10000, "6a0c4e5401150020120169895c32", "OP_RETURN 4e5401150020120169895c32",
                           std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinB_vout3;
    vinB_vout3.__manualSet(
        70000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinB.__manualSet(1,
                           uint256("111481401fbd842c5aa1b9420db8c5ef7e94d9ac3b3d3b7e2b2cb6c7b0650612"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinB_vout0, vinB_vout1, vinB_vout2, vinB_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs{std::make_pair(txVinB, ntp1txVinB),
                                                                 std::make_pair(txVinA, ntp1txVinA)};

    ////////////////////////////////////////////

    NTP1Transaction ntp1tx;
    ASSERT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx, inputs));
    EXPECT_EQ(ntp1tx.getTxInCount(), static_cast<unsigned>(2));
    // inputs are unknown, so no more tests
    EXPECT_EQ(ntp1tx.getTxOutCount(), static_cast<unsigned>(4));
    EXPECT_EQ(ntp1tx.getTxOut(0).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), static_cast<uint64_t>(100));
    EXPECT_EQ(ntp1tx.getTxOut(1).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(1).getToken(0).getAmount(), static_cast<uint64_t>(999965200));
    EXPECT_EQ(ntp1tx.getTxOut(2).tokenCount(), static_cast<unsigned>(0));
    EXPECT_EQ(ntp1tx.getTxOut(3).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(3).getToken(0).getAmount(), static_cast<uint64_t>(200));
    EXPECT_EQ(ntp1tx.getTxHash().ToString(),
              "006bd375946e903aa20aced1b411d61d14175488650e1deab3cb5ff8f354467d");

    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getHash().ToString(),
              "1766a9150953392de523f9420a2e32f993bb572e79f465de78ef96831494b347");
    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getIndex(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxIn(1).getPrevout().getHash().ToString(),
              "111481401fbd842c5aa1b9420db8c5ef7e94d9ac3b3d3b7e2b2cb6c7b0650612");
    EXPECT_EQ(ntp1tx.getTxIn(1).getPrevout().getIndex(), static_cast<unsigned>(3));

    /// READ AND WRITE
    ///
    {
        // will be freed automatically by writeFromDisk
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;
        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
    {
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;
        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_burn_with_transfer_1)
{
    // burn with transfer
    string transaction =
        "0100000048b1535b04e935973056fce6856f04bdcf6f9f6c8759e495c5f9bc19d5688fe9cecc3c56c0010000006"
        "b483045022100b004a3201d922e25579d2feba02dad95df573e5dee5efb6cc4c761348e08c580022003a860417f"
        "0de670b3a08df43d244aa16661f5218830bf5eb73b938050c8112a012103331393f9487ef4b318ae79972f3ccc8"
        "4b15d0718d7e05720c454404e67d51d1affffffff05dbb77b0d5990f177f9f7a7d36657ec886653f3dec7441621"
        "d81e9c55494803030000006a473044022039b3c6719b340f77a178e781a2f3bc6be0dcc78ea03ec413cc6527dff"
        "6abf96902204abf71cc27430089bed9c7692cd67af66ece912d67b60f401da0b2a4bfa5bc38012103331393f948"
        "7ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffffdf712745b40af1feb73f3a0d9cffe"
        "f4101033f20c3e827344098a2f338cf3201030000006a47304402201a8fdafcb5d0528eee7d3abf02ec4f9acdd6"
        "90c26ded4ec87d36f803daa1abd00220470b9225d0f5acb7af807af1cb43638f134492bc07d8d60f17295f9096d"
        "25296012103331393f9487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff98128fe8"
        "5f695f77584a02d20569674c557056e689c342a18ecd3803f2c31703030000006a47304402206b521b8663386ab"
        "faa861150ea9a1f444edf78e22682aef95791d2817177661a0220553f5c2bf0cd67053fbc3058f122522f7cb94f"
        "9c2bea157d4df16d1dca9f9e4a012103331393f9487ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d"
        "51d1affffffff0310270000000000001976a9147f5aff9c5ec060a45b8405a7b4f65fce5909773e88ac10270000"
        "000000000a6a084e540125000a1f1410270000000000001976a9143f7eb8c3da2cbe606fd5d46b11ab921170577"
        "0db88ac00000000";
    CTransaction tx = TxFromHex(transaction);
    EXPECT_TRUE(tx.CheckTransaction());

    std::string vinA =
        "010000005944185b0226d0e3af9cf2fa36d2cbecd53d54dc68e35489c85fae907e050165dc1a980413010000006"
        "a4730440220024193e8b8fad41d5672e6629b03ae662b61cf39bdf6a74ce3c4dac7cfb302a10220602741901ff9"
        "898c922ad0da52e558361bea8b0bbcdd8a8e5f3810e1e48a63ee012103331393f9487ef4b318ae79972f3ccc84b"
        "15d0718d7e05720c454404e67d51d1affffffffc486eeb847a82fcc78ed0a3cd83c2b7156bc5677b6488e7822f6"
        "0df0ec42accd030000006a473044022052458345ebaa51676f3469174e086a3b4c65c8a0c1cee99cee1b7190835"
        "c456702204f442eccdcbe25aab24b5ddb5ad3098b676699b3df47f5a1651667a23759c058012103331393f9487e"
        "f4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a9144440c"
        "2f1bdc0ce2498f135a3e67434d0e765c57f88ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b11ab"
        "9211705770db88ac10270000000000000f6a0d4e54011500201201802fadc75120830c00000000001976a9143f7"
        "eb8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinA = TxFromHex(vinA);
    NTP1Transaction ntp1txVinA;

    NTP1TokenTxData vinA_vout0_token0;
    vinA_vout0_token0.setAggregationPolicy("aggregable");
    vinA_vout0_token0.setAmount(100);
    vinA_vout0_token0.setDivisibility(7);
    vinA_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinA_vout0_token0.setLockStatus(true);
    vinA_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinA_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinA_vout0;
    vinA_vout0.__manualSet(
        10000, "76a91486061d16eafa0ea7a6be8875fb5bbc09a5f210a588ac",
        "OP_DUP OP_HASH160 86061d16eafa0ea7a6be8875fb5bbc09a5f210a5 OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinA_vout0_token0}), "NS8riWSXkBwWLK1wDDqReNHiLA5pTHeHbg");

    NTP1TokenTxData vinA_vout1_token0;
    vinA_vout1_token0.setAggregationPolicy("aggregable");
    vinA_vout1_token0.setAmount(999897380);
    vinA_vout1_token0.setDivisibility(7);
    vinA_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinA_vout1_token0.setLockStatus(true);
    vinA_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinA_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinA_vout1;
    vinA_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinA_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinA_vout2;
    vinA_vout2.__manualSet(10000, "6a0d4e54011500201201802fadc751",
                           "OP_RETURN 4e54011500201201802fadc751", std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinA_vout3;
    vinA_vout3.__manualSet(
        820000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinA.__manualSet(1,
                           uint256("c0563ccccee98f68d519bcf9c595e459876c9f6fcfbd046f85e6fc56309735e9"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinA_vout0, vinA_vout1, vinA_vout2, vinA_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    std::string vinB =
        "010000005764a35a02da1b4ac76bbcee52883fa9dca66badcf26419cbebad3649bad1716b07bfb9b42010000006"
        "a473044022044cf586c7ff83f70e7826a1f1dca6b94a578fd00202ff0796022821fa5f0accf022003e4a5e7cd13"
        "780eb237074ee0bdb8ca718c61db69f69dcf3eca186a075b185a012103331393f9487ef4b318ae79972f3ccc84b"
        "15d0718d7e05720c454404e67d51d1affffffffe6f3e0c696cbf8478bcaf21ec97df5ae6619499813a4e84f2ff8"
        "02644f4d4bc1030000006a47304402207fa6cd4be5571f207a379a3da6f908b3b4cda3f4e8464219357d99f46f6"
        "5ddb0022037fe0c24e01d0a549313eab5699bf9e8cb098eea89df5aea49e3db377cdb1cf5012103331393f9487e"
        "f4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a91413d4f"
        "e22b1d29a1bbeb3755441003e2380dcf13288ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b11ab"
        "9211705770db88ac10270000000000000e6a0c4e5401150020120169894ba210270000000000001976a9143f7eb"
        "8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinB = TxFromHex(vinB);
    NTP1Transaction ntp1txVinB;

    NTP1TokenTxData vinB_vout0_token0;
    vinB_vout0_token0.setAggregationPolicy("aggregable");
    vinB_vout0_token0.setAmount(100);
    vinB_vout0_token0.setDivisibility(7);
    vinB_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinB_vout0_token0.setLockStatus(true);
    vinB_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinB_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinB_vout0;
    vinB_vout0.__manualSet(
        10000, "76a91486061d16eafa0ea7a6be8875fb5bbc09a5f210a588ac",
        "OP_DUP OP_HASH160 86061d16eafa0ea7a6be8875fb5bbc09a5f210a5 OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinB_vout0_token0}), "NMiqBPp9WkE7bZo3d1CLcVd1orGdapY1VN");

    NTP1TokenTxData vinB_vout1_token0;
    vinB_vout1_token0.setAggregationPolicy("aggregable");
    vinB_vout1_token0.setAmount(999954600);
    vinB_vout1_token0.setDivisibility(7);
    vinB_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinB_vout1_token0.setLockStatus(true);
    vinB_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinB_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinB_vout1;
    vinB_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinB_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinB_vout2;
    vinB_vout2.__manualSet(10000, "6a0c4e5401150020120169894ba2", "OP_RETURN 4e5401150020120169894ba2",
                           std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinB_vout3;
    vinB_vout3.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinB.__manualSet(1,
                           uint256("034849559c1ed8211644c7def3536688ec5766d3a7f7f977f190590d7bb7db05"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinB_vout0, vinB_vout1, vinB_vout2, vinB_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    std::string vinC =
        "010000006066a35a02d5e4c9c957fdfc9a737a22bc0a28d2260476687eb5c9b0b9debc1dd281f84e0f010000006"
        "a47304402206e9241b719ceeeb35803170ff2f502779f3fdab1072481b1a2326618148dc4cf022048f478a43e8a"
        "4301d9a877645f530c430a158e6aa81c4627b5fc21a9bfa8e4ff012103331393f9487ef4b318ae79972f3ccc84b"
        "15d0718d7e05720c454404e67d51d1affffffff47750a9c7c1804b8e3ceaf092038df2f8289e827a10ae8daa547"
        "2f3cae6cc9a2030000006b483045022100e01ba371ca19f4890a381f804e9cf6223e174ad9a558ee3adb1b74d88"
        "d87a62f02205e688a1173dd8eaaf2173ae9958880c3eb3c8122ffd5fb22b793a0c292eea3c9012103331393f948"
        "7ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a914e1a"
        "a4fabe4db6c5f6d262c830571288a746a06df88ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b11"
        "ab9211705770db88ac10270000000000000e6a0c4e5401150020120169894a9210270000000000001976a9143f7"
        "eb8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinC = TxFromHex(vinC);
    NTP1Transaction ntp1txVinC;

    NTP1TokenTxData vinC_vout0_token0;
    vinC_vout0_token0.setAggregationPolicy("aggregable");
    vinC_vout0_token0.setAmount(100);
    vinC_vout0_token0.setDivisibility(7);
    vinC_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinC_vout0_token0.setLockStatus(true);
    vinC_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinC_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinC_vout0;
    vinC_vout0.__manualSet(
        10000, "76a914e1aa4fabe4db6c5f6d262c830571288a746a06df88ac",
        "OP_DUP OP_HASH160 e1aa4fabe4db6c5f6d262c830571288a746a06df OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinC_vout0_token0}), "NgVBFMAt1moZrPS2LX7ud7zJGDd8rtNsvu");

    NTP1TokenTxData vinC_vout1_token0;
    vinC_vout1_token0.setAggregationPolicy("aggregable");
    vinC_vout1_token0.setAmount(999952900);
    vinC_vout1_token0.setDivisibility(7);
    vinC_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinC_vout1_token0.setLockStatus(true);
    vinC_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinC_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinC_vout1;
    vinC_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinC_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinC_vout2;
    vinC_vout2.__manualSet(10000, "6a0c4e5401150020120169894a92", "OP_RETURN 4e5401150020120169894a92",
                           std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinC_vout3;
    vinC_vout3.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinC.__manualSet(1,
                           uint256("0132cf38f3a298403427e8c3203f030141efff9c0d3a3fb7fef10ab4452771df"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinC_vout0, vinC_vout1, vinC_vout2, vinC_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    std::string vinD =
        "01000000c87ba35a028ce101d93f5d2a443f368f91a8eff8c86d13be0779db72d8e6b5d53b051444e4010000006"
        "b483045022100a1b6d0ee3a9b5002735431d8b12ecb2dad7fd6a2b9086f072ca8c787ddd8781802201a7fe4cd73"
        "d994ef4d620cde94292d46cb54744679bd1238cda064adf7a00e1a012103331393f9487ef4b318ae79972f3ccc8"
        "4b15d0718d7e05720c454404e67d51d1afffffffff97366c14d0f369505f6fc90d7643e5c9484ad6c5e4353593c"
        "b4c733b590a843030000006a47304402202b68c6b9ec4b5ae3caeed7c13036c414624a93196bd6a3c7bcab3bb35"
        "0a4136d02206b2c5536f0624f2ed58ecc1e976a55b9124960c1f60939af3c6a232aa0e8d69c012103331393f948"
        "7ef4b318ae79972f3ccc84b15d0718d7e05720c454404e67d51d1affffffff0410270000000000001976a9148a4"
        "b68e051ba56f5ef5fd101e23eabd6c5969c0488ac10270000000000001976a9143f7eb8c3da2cbe606fd5d46b11"
        "ab9211705770db88ac10270000000000000e6a0c4e540115002012016989403210270000000000001976a9143f7"
        "eb8c3da2cbe606fd5d46b11ab9211705770db88ac00000000";
    CTransaction    txVinD = TxFromHex(vinD);
    NTP1Transaction ntp1txVinD;

    NTP1TokenTxData vinD_vout0_token0;
    vinD_vout0_token0.setAggregationPolicy("aggregable");
    vinD_vout0_token0.setAmount(100);
    vinD_vout0_token0.setDivisibility(7);
    vinD_vout0_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinD_vout0_token0.setLockStatus(true);
    vinD_vout0_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinD_vout0_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinD_vout0;
    vinD_vout0.__manualSet(
        10000, "76a9148a4b68e051ba56f5ef5fd101e23eabd6c5969c0488ac",
        "OP_DUP OP_HASH160 8a4b68e051ba56f5ef5fd101e23eabd6c5969c04 OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinD_vout0_token0}), "NYXCmnTDhV8hBx7QB1QrqG6XqSutzxAgQ7");

    NTP1TokenTxData vinD_vout1_token0;
    vinD_vout1_token0.setAggregationPolicy("aggregable");
    vinD_vout1_token0.setAmount(999936300);
    vinD_vout1_token0.setDivisibility(7);
    vinD_vout1_token0.setIssueTxIdHex(
        "66216fa9cc0167568c3e5f8b66e7fe3690072f66a5f41df222327de7af10ff80");
    vinD_vout1_token0.setLockStatus(true);
    vinD_vout1_token0.setTokenId("LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp");
    vinD_vout1_token0.setTokenSymbol("NIBBL");

    NTP1TxOut vinD_vout1;
    vinD_vout1.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>({vinD_vout1_token0}), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    NTP1TxOut vinD_vout2;
    vinD_vout2.__manualSet(10000, "6a0c4e5401150020120169894032", "OP_RETURN 4e5401150020120169894032",
                           std::vector<NTP1TokenTxData>(), "");

    NTP1TxOut vinD_vout3;
    vinD_vout3.__manualSet(
        10000, "76a9143f7eb8c3da2cbe606fd5d46b11ab9211705770db88ac",
        "OP_DUP OP_HASH160 3f7eb8c3da2cbe606fd5d46b11ab9211705770db OP_EQUALVERIFY OP_CHECKSIG",
        std::vector<NTP1TokenTxData>(), "NRhhZd2hzHmtHWGtQLY8Kjnt5tabyeVSxw");

    // inputs are not important
    ntp1txVinD.__manualSet(1,
                           uint256("0317c3f20338cd8ea142c389e65670554c676905d2024a58775f695fe88f1298"),
                           std::vector<unsigned char>(), std::vector<NTP1TxIn>{},
                           std::vector<NTP1TxOut>{vinD_vout0, vinD_vout1, vinD_vout2, vinD_vout3}, 0,
                           1520653825000, NTP1TxType_TRANSFER);

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs{
        std::make_pair(txVinA, ntp1txVinA), std::make_pair(txVinB, ntp1txVinB),
        std::make_pair(txVinC, ntp1txVinC), std::make_pair(txVinD, ntp1txVinD)};

    std::string opReturnArg;
    EXPECT_TRUE(IsTxNTP1(&tx, &opReturnArg));
    std::shared_ptr<NTP1Script>      scriptPtr  = NTP1Script::ParseScript(opReturnArg);
    std::shared_ptr<NTP1Script_Burn> scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Burn>(scriptPtr);
    EXPECT_NE(scriptPtr.get(), nullptr);
    EXPECT_NE(scriptPtrD.get(), nullptr);
    EXPECT_EQ(scriptPtrD->getTxType(), NTP1Script::TxType::TxType_Burn);

    NTP1Transaction ntp1tx;
    EXPECT_NO_THROW(ntp1tx.readNTP1DataFromTx(tx, inputs));
    EXPECT_EQ(ntp1tx.getTxInCount(), static_cast<unsigned>(4));
    // inputs are unknown, so no more tests
    EXPECT_EQ(ntp1tx.getTxOutCount(), static_cast<unsigned>(3));
    EXPECT_EQ(ntp1tx.getTxOut(0).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(0).getToken(0).getAmount(), static_cast<uint64_t>(10));
    EXPECT_EQ(ntp1tx.getTxOut(1).tokenCount(), static_cast<unsigned>(0));
    EXPECT_EQ(ntp1tx.getTxOut(2).tokenCount(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxOut(2).getToken(0).getAmount(), static_cast<unsigned>(999897350));

    EXPECT_EQ(ntp1tx.getTxHash().ToString(),
              "008d329611fcbdb82b4adb097c29f1d6a56707bfb232c8c124390756e80a9e44");

    // inputs
    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getHash().ToString(),
              "c0563ccccee98f68d519bcf9c595e459876c9f6fcfbd046f85e6fc56309735e9");
    EXPECT_EQ(ntp1tx.getTxIn(0).getPrevout().getIndex(), static_cast<unsigned>(1));
    EXPECT_EQ(ntp1tx.getTxIn(1).getPrevout().getHash().ToString(),
              "034849559c1ed8211644c7def3536688ec5766d3a7f7f977f190590d7bb7db05");
    EXPECT_EQ(ntp1tx.getTxIn(1).getPrevout().getIndex(), static_cast<unsigned>(3));
    EXPECT_EQ(ntp1tx.getTxIn(2).getPrevout().getHash().ToString(),
              "0132cf38f3a298403427e8c3203f030141efff9c0d3a3fb7fef10ab4452771df");
    EXPECT_EQ(ntp1tx.getTxIn(2).getPrevout().getIndex(), static_cast<unsigned>(3));
    EXPECT_EQ(ntp1tx.getTxIn(3).getPrevout().getHash().ToString(),
              "0317c3f20338cd8ea142c389e65670554c676905d2024a58775f695fe88f1298");
    EXPECT_EQ(ntp1tx.getTxIn(3).getPrevout().getIndex(), static_cast<unsigned>(3));

    /// READ AND WRITE
    ///
    {
        // will be freed automatically by writeFromDisk
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;

        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
    {
        FILE* fileWrite = fopen(TempNTP1File.c_str(), "ab");
        EXPECT_NE(fileWrite, nullptr);

        unsigned int nFileRet = -1;
        unsigned int nTxPos   = -1;
        EXPECT_TRUE(ntp1tx.writeToDisk(nFileRet, nTxPos, fileWrite));

        FILE* fileRead = fopen(TempNTP1File.c_str(), "rb");
        EXPECT_NE(fileRead, nullptr);

        NTP1Transaction ntp1tx2;
        ntp1tx2.readFromDisk(DiskNTP1TxPos(nFileRet, nTxPos), nullptr, fileRead);
        EXPECT_EQ(ntp1tx, ntp1tx2);
    }
}

std::string GetRawTxURL(const std::string& txid, bool testnet)
{
    if (!testnet) {
        return "https://ntp1node.nebl.io/ins/rawtx/" + txid;
    } else {
        return "https://ntp1node.nebl.io/testnet/ins/rawtx/" + txid;
    }
}

std::size_t GetFileSize(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    return static_cast<size_t>(file.tellg());
}

std::string ReadFileToString(const std::string& filename)
{
    std::fstream fileObject(filename, std::ios::in | std::ios::binary);
    if (!fileObject.good()) {
        throw std::runtime_error("Unable to open file: " + filename);
    }
    std::string data;
    size_t      filesize = GetFileSize(filename);
    if (filesize == 0) {
        throw std::runtime_error("File is empty");
    }

    try {
        data.resize(filesize);
    } catch (std::exception& ex) {
        throw std::runtime_error(
            "Unable to allocate memory to read file: " + filename +
            ". Memory full? Size required to allocate is: " + std::to_string(filesize) +
            ". Exception message: " + std::string(ex.what()));
    } catch (...) {
        throw std::runtime_error("Unable to allocate memory to read file: " + filename +
                                 ". An unknown exception was thrown.");
    }

    if (filesize > 0) {
        fileObject.read(&data.front(), filesize);
        fileObject.close();
        return data;
    } else {
        throw std::runtime_error("Although a file exist, but it has zero bytes size. The file is: " +
                                 filename);
    }
}

json_spirit::Object read_json_obj(const std::string& filename)
{
    namespace fs          = boost::filesystem;
    fs::path testRootPath = TEST_ROOT_PATH;
    fs::path testFile     = testRootPath / "data" / filename;

#ifdef TEST_DATA_DIR
    if (!fs::exists(testFile)) {
        testFile = fs::path(BOOST_PP_STRINGIZE(TEST_DATA_DIR)) / filename;
    }
#endif

    std::string        jsonFileData = ReadFileToString(testFile.string());
    json_spirit::Value v;
    json_spirit::read_or_throw(jsonFileData, v);
    return v.get_obj();
}

std::vector<std::string> read_line_by_line(const std::string& filename)
{
    namespace fs          = boost::filesystem;
    fs::path testRootPath = TEST_ROOT_PATH;
    fs::path testFile     = testRootPath / "data" / filename;

    if (!fs::exists(testFile.string().c_str())) {
        throw std::runtime_error("File doesn't exist");
    }

    ifstream                 ifs(testFile.string().c_str(), ifstream::in);
    std::vector<std::string> result;
    std::string              line;
    while (std::getline(ifs, line)) {
        if (line.empty())
            continue;
        result.push_back(line);
    }
    return result;
}

std::string NTP1Tests_GetTxidListFileName(bool testnet)
{
    if (!testnet) {
        return "ntp1txids_to_test.txt";
    } else {
        return "ntp1txids_to_test_testnet.txt";
    }
}

std::string NTP1Tests_GetRawNeblioTxsFileName(bool testnet)
{
    if (!testnet) {
        return "txs_ntp1tests_raw_neblio_txs.json";
    } else {
        return "txs_ntp1tests_raw_neblio_txs_testnet.json";
    }
}

std::string NTP1Tests_GetNTP1RawTxsFileName(bool testnet)
{
    if (!testnet) {
        return "txs_ntp1tests_ntp1_txs.json";
    } else {
        return "txs_ntp1tests_ntp1_txs_testnet.json";
    }
}

std::string GetRawTxOnline(const std::string& txid, bool testnet)
{
    std::string        rawTxJson = cURLTools::GetFileFromHTTPS(GetRawTxURL(txid, testnet), 10000, 0);
    json_spirit::Value v;
    json_spirit::read_or_throw(rawTxJson, v);
    json_spirit::Object rawTxObj = v.get_obj();
    std::string         rawTx    = NTP1Tools::GetStrField(rawTxObj, "rawtx");
    return rawTx;
}

std::vector<std::pair<CTransaction, NTP1Transaction>> GetNTP1InputsOnline(const CTransaction& tx,
                                                                          bool                testnet)
{
    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs;

    for (int i = 0; i < (int)tx.vin.size(); i++) {
        std::string  inputTxid  = tx.vin[i].prevout.hash.ToString();
        std::string  inputRawTx = GetRawTxOnline(inputTxid, testnet);
        CTransaction inputTx    = TxFromHex(inputRawTx);

        NTP1Transaction inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo(inputTxid, testnet);

        inputs.push_back(std::make_pair(inputTx, inputNTP1Tx));
    }

    return inputs;
}

void TestNTP1TxParsing_onlyRead(const CTransaction& tx, bool testnet)
{
    //    const std::string& txid = tx.GetHash().ToString();

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs;

    for (int i = 0; i < (int)tx.vin.size(); i++) {
        std::string  inputTxid  = tx.vin[i].prevout.hash.ToString();
        std::string  inputRawTx = GetRawTxOnline(inputTxid, testnet);
        CTransaction inputTx    = TxFromHex(inputRawTx);

        NTP1Transaction inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo(inputTxid, testnet);

        inputs.push_back(std::make_pair(inputTx, inputNTP1Tx));
    }

    NTP1Transaction ntp1tx;
    ntp1tx.readNTP1DataFromTx(tx, inputs);
}

void TestNTP1TxParsing(const CTransaction& tx, bool testnet)
{
    const std::string&    txid       = tx.GetHash().ToString();
    const NTP1Transaction ntp1tx_ref = NTP1APICalls::RetrieveData_TransactionInfo(txid, testnet);
    EXPECT_TRUE(tx.CheckTransaction()) << "Failed tx: " << txid;

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs;

    for (int i = 0; i < (int)tx.vin.size(); i++) {
        std::string  inputTxid  = tx.vin[i].prevout.hash.ToString();
        std::string  inputRawTx = GetRawTxOnline(inputTxid, testnet);
        CTransaction inputTx    = TxFromHex(inputRawTx);

        NTP1Transaction inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo(inputTxid, testnet);

        inputs.push_back(std::make_pair(inputTx, inputNTP1Tx));
    }

    NTP1Transaction ntp1tx;
    ntp1tx.readNTP1DataFromTx(tx, inputs);

    EXPECT_EQ(ntp1tx.getTxOutCount(), ntp1tx_ref.getTxOutCount()) << "Failed tx: " << txid;
    for (int i = 0; i < (int)ntp1tx.getTxOutCount(); i++) {
        ASSERT_EQ(ntp1tx.getTxOut(i).tokenCount(), ntp1tx_ref.getTxOut(i).tokenCount())
            << "Failed tx: " << txid << "; Failed at TxOut: " << i;
        EXPECT_EQ(ntp1tx_ref.getTxOut(i).getValue(), ntp1tx.getTxOut(i).getValue());
        EXPECT_EQ(ntp1tx_ref.getTxOut(i).getAddress(), ntp1tx.getTxOut(i).getAddress());
        std::string script1 = ntp1tx_ref.getTxOut(i).getScriptPubKeyHex();
        std::string script2 = ntp1tx.getTxOut(i).getScriptPubKeyHex();
        std::transform(script1.begin(), script1.end(), script1.begin(), ::toupper);
        std::transform(script2.begin(), script2.end(), script2.begin(), ::toupper);
        EXPECT_EQ(script1, script2);
        EXPECT_EQ(ntp1tx_ref.getTxOut(i).getScriptPubKeyAsm(), ntp1tx.getTxOut(i).getScriptPubKeyAsm());
        for (int j = 0; j < (int)ntp1tx.getTxOut(i).tokenCount(); j++) {
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getAmount(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getAmount())
                << "Failed tx: " << txid << "; Failed at TxOut: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getAggregationPolicy(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getAggregationPolicy())
                << "Failed tx: " << txid << "; Failed at TxOut: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getDivisibility(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getDivisibility())
                << "Failed tx: " << txid << "; Failed at TxOut: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getIssueTxId().ToString(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getIssueTxId().ToString())
                << "Failed tx: " << txid << "; Failed at TxOut: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getLockStatus(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getLockStatus())
                << "Failed tx: " << txid << "; Failed at TxOut: " << i << "; at token: " << j;
            //            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getTokenSymbol(),
            //                      ntp1tx_ref.getTxOut(i).getToken(j).getTokenSymbol())
            //                << "Failed tx: " << txid;
        }
    }

    EXPECT_EQ(ntp1tx.getTxInCount(), ntp1tx_ref.getTxInCount());
    for (int i = 0; i < (int)ntp1tx.getTxInCount(); i++) {
        ASSERT_EQ(ntp1tx.getTxIn(i).getNumOfTokens(), ntp1tx_ref.getTxIn(i).getNumOfTokens())
            << "Failed tx: " << txid << "; Failed at TxIn: " << i;
        EXPECT_EQ(ntp1tx_ref.getTxIn(i).getOutPoint(), ntp1tx.getTxIn(i).getOutPoint());
        EXPECT_EQ(ntp1tx_ref.getTxIn(i).getPrevout(), ntp1tx.getTxIn(i).getPrevout());
        std::string sig1 = ntp1tx_ref.getTxIn(i).getScriptSigHex();
        std::string sig2 = ntp1tx.getTxIn(i).getScriptSigHex();
        std::transform(sig1.begin(), sig1.end(), sig1.begin(), ::toupper);
        std::transform(sig2.begin(), sig2.end(), sig2.begin(), ::toupper);
        EXPECT_EQ(sig1, sig2);
        for (int j = 0; j < (int)ntp1tx.getTxIn(i).getNumOfTokens(); j++) {
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getAmount(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getAmount())
                << "Failed tx: " << txid << "; Failed at TxIn: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getAggregationPolicy(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getAggregationPolicy())
                << "Failed tx: " << txid << "; Failed at TxIn: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getDivisibility(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getDivisibility())
                << "Failed tx: " << txid << "; Failed at TxIn: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getIssueTxId(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getIssueTxId())
                << "Failed tx: " << txid << "; Failed at TxIn: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getLockStatus(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getLockStatus())
                << "Failed tx: " << txid << "; Failed at TxIn: " << i << "; at token: " << j;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getTokenSymbol(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getTokenSymbol())
                << "Failed tx: " << txid << "; Failed at TxIn: " << i << "; at token: " << j;
        }
    }

    EXPECT_EQ(ntp1tx.getTxHash(), ntp1tx_ref.getTxHash()) << "Failed tx: " << txid;
}

void TestNTP1TxParsing(const std::string& txid, bool testnet)
{
    std::string  rawTx = GetRawTxOnline(txid, testnet);
    CTransaction tx    = TxFromHex(rawTx);
    TestNTP1TxParsing(tx, testnet);
}

void TestScriptParsing(std::string OpReturnArg)
{
    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScript(OpReturnArg);
    std::string calculatedScript          = boost::algorithm::hex(scriptPtr->calculateScriptBin());
    std::transform(OpReturnArg.begin(), OpReturnArg.end(), OpReturnArg.begin(), ::tolower);
    std::transform(calculatedScript.begin(), calculatedScript.end(), calculatedScript.begin(),
                   ::tolower);
    EXPECT_EQ(calculatedScript, OpReturnArg) << "Calculated script doesn't match input script";
}

void TestSingleNTP1TxParsingLocally(const CTransaction&                                 tx,
                                    const std::unordered_map<std::string, std::string>& nebltxs_map,
                                    const std::unordered_map<std::string, std::string>& ntp1txs_map)
{
    const std::string& txid           = tx.GetHash().ToString();
    const std::string  ntp1tx_ref_str = ntp1txs_map.find(txid)->second;
    NTP1Transaction    ntp1tx_ref;
    ntp1tx_ref.importJsonData(ntp1tx_ref_str);
    EXPECT_TRUE(tx.CheckTransaction()) << "Failed tx: " << txid;

    std::string OpReturnArg;
    EXPECT_TRUE(IsTxNTP1(&tx, &OpReturnArg));

    TestScriptParsing(OpReturnArg);

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs;

    for (int i = 0; i < (int)tx.vin.size(); i++) {
        std::string  inputTxid  = tx.vin[i].prevout.hash.ToString();
        std::string  inputRawTx = nebltxs_map.find(inputTxid)->second;
        CTransaction inputTx    = TxFromHex(inputRawTx);

        const std::string inputNTP1TxStr = ntp1txs_map.find(inputTxid)->second;
        NTP1Transaction   inputNTP1Tx;
        inputNTP1Tx.importJsonData(inputNTP1TxStr);

        inputs.push_back(std::make_pair(inputTx, inputNTP1Tx));
    }

    NTP1Transaction ntp1tx;
    ntp1tx.readNTP1DataFromTx(tx, inputs);

    EXPECT_EQ(ntp1tx.getTxOutCount(), ntp1tx_ref.getTxOutCount()) << "Failed tx: " << txid;
    for (int i = 0; i < (int)ntp1tx.getTxOutCount(); i++) {
        ASSERT_EQ(ntp1tx.getTxOut(i).tokenCount(), ntp1tx_ref.getTxOut(i).tokenCount())
            << "Failed tx: " << txid;
        for (int j = 0; j < (int)ntp1tx.getTxOut(i).tokenCount(); j++) {
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getAmount(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getAmount())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getAggregationPolicy(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getAggregationPolicy())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getDivisibility(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getDivisibility())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getIssueTxId(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getIssueTxId())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getLockStatus(),
                      ntp1tx_ref.getTxOut(i).getToken(j).getLockStatus())
                << "Failed tx: " << txid;
            // skipping testing input token name as it's not available in the NTP1 API

            //            EXPECT_EQ(ntp1tx.getTxOut(i).getToken(j).getTokenSymbol(),
            //                      ntp1tx_ref.getTxOut(i).getToken(j).getTokenSymbol())
            //                << "Failed tx: " << txid;
        }
    }

    EXPECT_EQ(ntp1tx.getTxInCount(), ntp1tx_ref.getTxInCount());
    for (int i = 0; i < (int)ntp1tx.getTxInCount(); i++) {
        ASSERT_EQ(ntp1tx.getTxIn(i).getNumOfTokens(), ntp1tx_ref.getTxIn(i).getNumOfTokens())
            << "Failed tx: " << txid;
        for (int j = 0; j < (int)ntp1tx.getTxIn(i).getNumOfTokens(); j++) {
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getAmount(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getAmount())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getAggregationPolicy(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getAggregationPolicy())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getDivisibility(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getDivisibility())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getIssueTxId(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getIssueTxId())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getLockStatus(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getLockStatus())
                << "Failed tx: " << txid;
            EXPECT_EQ(ntp1tx.getTxIn(i).getToken(j).getTokenSymbol(),
                      ntp1tx_ref.getTxIn(i).getToken(j).getTokenSymbol())
                << "Failed tx: " << txid;
        }
    }

    EXPECT_EQ(ntp1tx.getTxHash(), ntp1tx_ref.getTxHash()) << "Failed tx: " << txid;
}

void TestSingleNTP1TxParsingLocally(const std::string&                                  txid,
                                    const std::unordered_map<std::string, std::string>& nebltxs_map,
                                    const std::unordered_map<std::string, std::string>& ntp1txs_map)
{
    const std::string  rawTx = nebltxs_map.find(txid)->second;
    const CTransaction tx    = TxFromHex(rawTx);

    // Only run test for NTP1v1, exclude NTP1v2 until we support it
    if (IsTxNTP1(&tx)) {
    	TestSingleNTP1TxParsingLocally(tx, nebltxs_map, ntp1txs_map);
    }
}

// list of transactions to be excluded from tests
std::unordered_set<std::string> excluded_txs_testnet = {
    "826e7b74b24e458e39d779b1033567d325b8d93b507282f983e3c4b3f950fca1",
    "c378447562be04c6803fdb9f829c9ba0dda462b269e15bcfc7fac3b3561d2eef",
    "7e71508abef696d6c0427cc85073e0d56da9380f3d333354c7dd9370acd422bc",
    "adb421a497e25375a88848b17b5c632a8d60db3d02dcc61dbecd397e6c1fb1ca",
    "95c6f2b978160ab0d51545a13a7ee7b931713a52bd1c9f12807f4cd77ff7536b"};

std::unordered_set<std::string> excluded_txs_mainnet = {};

void TestNTP1TxParsingLocally(bool testnet)
{
    std::unordered_map<std::string, std::string> ntp1txs_map;
    std::unordered_map<std::string, std::string> nebltxs_map;

    std::vector<std::string> txids = read_line_by_line(NTP1Tests_GetTxidListFileName(testnet));

    // save memory by ensuring the destruction of json objects
    {
        json_spirit::Object nebltxs = read_json_obj(NTP1Tests_GetRawNeblioTxsFileName(testnet));
        json_spirit::Object ntp1txs = read_json_obj(NTP1Tests_GetNTP1RawTxsFileName(testnet));

        for (const auto& el : nebltxs) {
            nebltxs_map[el.name_] = el.value_.get_str();
        }
        for (const auto& el : ntp1txs) {
            ntp1txs_map[el.name_] = boost::algorithm::unhex(el.value_.get_str());
        }
    }

    uint64_t count = 0;
    for (const auto& txid : txids) {
        if (count % 100 == 0)
            std::cout << "Finished testing " << count << " transactions" << std::endl;
        count++;
        if (testnet) {
            if (excluded_txs_testnet.find(txid) != excluded_txs_testnet.end())
                continue;
        } else {
            if (excluded_txs_mainnet.find(txid) != excluded_txs_mainnet.end())
                continue;
        }
        TestSingleNTP1TxParsingLocally(txid, nebltxs_map, ntp1txs_map);
    }
}

void write_json_file(const json_spirit::Object& obj, const std::string& filename)
{
    namespace fs          = boost::filesystem;
    fs::path testRootPath = TEST_ROOT_PATH;
    fs::path testFile     = testRootPath / "data" / filename;

    ofstream os(testFile.string().c_str());

    json_spirit::write_formatted(obj, os);
}

void DownloadData(bool testnet)
{
    std::vector<std::string> txids = read_line_by_line(NTP1Tests_GetTxidListFileName(testnet));

    std::unordered_map<std::string, std::string> rawNeblioTxsMap;
    std::unordered_map<std::string, std::string> ntp1TxsMap;

    json_spirit::Object rawNeblioTxs;
    json_spirit::Object ntp1Txs;

    for (uint64_t i = 0; i < (uint64_t)txids.size(); i++) {
        std::cout << "Downloading tx: " << i << std::endl;

        std::string       rawTx      = GetRawTxOnline(txids[i], testnet);
        CTransaction      tx         = TxFromHex(rawTx);
        const std::string ntp1tx_ref = NTP1APICalls::RetrieveData_TransactionInfo_Str(txids[i], testnet);

        rawNeblioTxsMap[txids[i]] = rawTx;
        ntp1TxsMap[txids[i]]      = boost::algorithm::hex(ntp1tx_ref);

        for (int i = 0; i < (int)tx.vin.size(); i++) {
            std::string inputTxid = tx.vin[i].prevout.hash.ToString();

            std::string inputRawTxJson =
                cURLTools::GetFileFromHTTPS(GetRawTxURL(inputTxid, testnet), 10000, 0);
            json_spirit::Value v;
            json_spirit::read_or_throw(inputRawTxJson, v);
            json_spirit::Object inputRawTxObj = v.get_obj();
            std::string         inputRawTx    = NTP1Tools::GetStrField(inputRawTxObj, "rawtx");

            std::string inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo_Str(inputTxid, testnet);

            rawNeblioTxsMap[inputTxid] = inputRawTx;
            ntp1TxsMap[inputTxid]      = boost::algorithm::hex(inputNTP1Tx);
        }
    }

    for (const auto& el : rawNeblioTxsMap) {
        rawNeblioTxs.push_back(json_spirit::Pair(el.first, el.second));
    }
    for (const auto& el : ntp1TxsMap) {
        ntp1Txs.push_back(json_spirit::Pair(el.first, el.second));
    }

    std::string f1 = NTP1Tests_GetRawNeblioTxsFileName(testnet);
    std::string f2 = NTP1Tests_GetNTP1RawTxsFileName(testnet);

    std::remove(f1.c_str());
    std::remove(f2.c_str());

    write_json_file(rawNeblioTxs, f1);
    write_json_file(ntp1Txs, f2);
}

void DownloadPreMadeData(bool testnet)
{
    typedef boost::filesystem::path Path;

    namespace fs          = boost::filesystem;
    fs::path testRootPath = TEST_ROOT_PATH;

    std::vector<std::string> files;
    files.push_back(NTP1Tests_GetTxidListFileName(testnet));
    files.push_back(NTP1Tests_GetRawNeblioTxsFileName(testnet));
    files.push_back(NTP1Tests_GetNTP1RawTxsFileName(testnet));

    for (uint64_t i = 0; i < (uint64_t)files.size(); i++) {
        std::cout << "Downloading test data file " << files[i] << std::endl;
        EXPECT_NO_THROW(boost::filesystem::remove(Path(TEST_ROOT_PATH) / Path("/data/" + files[i])));
        std::string content = cURLTools::GetFileFromHTTPS(
            "https://neblio-files.ams3.digitaloceanspaces.com/" + files[i], 10000, 0);
        fs::path testFile = testRootPath / "data" / files[i];
        ofstream os(testFile.string().c_str());
        os << content;
        os.close();
    }
}

#ifdef UNITTEST_DOWNLOAD_TX_DATA
TEST(ntp1_tests, download_data_to_files)
{
    DownloadData(false);
    DownloadData(true);
}
#endif

#ifdef UNITTEST_DOWNLOAD_PREMADE_TX_DATA_AND_RUN_PARSE_TESTS
TEST(ntp1_tests, download_premade_data_to_files_and_run_parse_test)
{
    DownloadPreMadeData(true);
    TestNTP1TxParsingLocally(true);
    DownloadPreMadeData(false);
    TestNTP1TxParsingLocally(false);
}
#endif

#ifdef UNITTEST_RUN_NTP_PARSE_TESTS
TEST(ntp1_tests, parsig_ntp1_from_ctransaction_automated)
{
    EXPECT_NO_THROW(TestNTP1TxParsingLocally(false));
    EXPECT_NO_THROW(TestNTP1TxParsingLocally(true));
}
#endif

TEST(ntp1_tests, construct_scripts)
{
    {
        // test transfer instruction string script creator
        NTP1Script::TransferInstruction inst_manual;
        inst_manual.amount      = 1000000000;
        inst_manual.skipInput   = 0;
        inst_manual.outputIndex = 0;
        std::string ti = boost::algorithm::hex(NTP1Script::TransferInstructionToBinScript(inst_manual));
        std::transform(ti.begin(), ti.end(), ti.begin(), ::tolower);
        EXPECT_EQ(ti, "002019");

        // test recreating NIBBL script
        std::string toParse_issuance =
            "4e5401014e4942424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
            "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(toParse_issuance);
        std::shared_ptr<NTP1Script_Issuance> script_issuance =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(script);
        ASSERT_NE(script_issuance.get(), nullptr);
        std::string calculatedScript = boost::algorithm::hex(script_issuance->calculateScriptBin());
        std::transform(calculatedScript.begin(), calculatedScript.end(), calculatedScript.begin(),
                       ::tolower);
        EXPECT_EQ(calculatedScript, toParse_issuance);
    }
    {
        // transfer some tokens
        const std::string                    toParse_transfer = "4e5401150069892a92";
        std::shared_ptr<NTP1Script>          script = NTP1Script::ParseScript(toParse_transfer);
        std::shared_ptr<NTP1Script_Transfer> script_transfer =
            std::dynamic_pointer_cast<NTP1Script_Transfer>(script);
        ASSERT_NE(script_transfer, nullptr);

        std::string calculatedScript = boost::algorithm::hex(script->calculateScriptBin());

        std::transform(calculatedScript.begin(), calculatedScript.end(), calculatedScript.begin(),
                       ::tolower);
        EXPECT_EQ(calculatedScript, toParse_transfer);
    }
    {
        // burn some tokens
        std::string                      toParse_burn = "4e5401251f2013";
        std::shared_ptr<NTP1Script>      script       = NTP1Script::ParseScript(toParse_burn);
        std::shared_ptr<NTP1Script_Burn> script_burn =
            std::dynamic_pointer_cast<NTP1Script_Burn>(script);
        ASSERT_NE(script_burn, nullptr);

        std::string calculatedScript = boost::algorithm::hex(script->calculateScriptBin());

        std::transform(calculatedScript.begin(), calculatedScript.end(), calculatedScript.begin(),
                       ::tolower);
        EXPECT_EQ(calculatedScript, toParse_burn);
    }
}

CTransaction GetTxOnline(const std::string& txid, bool testnet)
{
    std::string rawTx = GetRawTxOnline(txid, testnet);
    return TxFromHex(rawTx);
}

TEST(ntp1_tests, amend_tx_1)
{
    fTestNet = true; // ensure testnet state will be reset on exit
    std::unique_ptr<int, void (*)(int*)> temp(new int, [](int* i) {
        fTestNet = false;
        delete i;
    });

    std::string txidIn1 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    std::string txidIn2 = "a9b000ba0eb6b88ba8daad4b89997b2b457164983a18bbf2bffa0515b0c84348";
    CTxIn       in0(uint256(txidIn1), 2);
    CTxIn       in1(uint256(txidIn2), 0);

    CScript script1;
    CKeyID  keyId;
    EXPECT_TRUE(CBitcoinAddress("TXM3QJ8DR2tSikeAYLY9UF6t3Lkjvt2Dva").GetKeyID(keyId));
    script1.SetDestination(keyId);
    CTxOut out0(96900000, script1);
    CTxOut out1(1000000, script1);

    CTransaction tx;

    tx.vin.push_back(in0);
    tx.vin.push_back(in1);

    tx.vout.push_back(out0);
    tx.vout.push_back(out1);

    auto inputs = GetNTP1InputsOnline(tx, true);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    EXPECT_EQ(tx.vout.size(), static_cast<unsigned>(4));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue, out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e540115032051");

    auto scriptPtr  = NTP1Script::ParseScript(opRetArg);
    auto scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
    EXPECT_NE(scriptPtrD, nullptr);

    EXPECT_EQ(scriptPtrD->getTransferInstructionsCount(), static_cast<unsigned>(1));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).amount, static_cast<unsigned>(50));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).outputIndex, static_cast<unsigned>(3));
}

TEST(ntp1_tests, amend_tx_2)
{
    fTestNet = true; // ensure testnet state will be reset on exit
    std::unique_ptr<int, void (*)(int*)> temp(new int, [](int* i) {
        fTestNet = false;
        delete i;
    });

    std::string txidIn1 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    CTxIn       in0(uint256(txidIn1), 2);

    CScript script1;
    CKeyID  keyId;
    EXPECT_TRUE(CBitcoinAddress("TXM3QJ8DR2tSikeAYLY9UF6t3Lkjvt2Dva").GetKeyID(keyId));
    script1.SetDestination(keyId);
    CTxOut out0(96900000, script1);
    CTxOut out1(1000000, script1);

    CTransaction tx;

    tx.vin.push_back(in0);

    tx.vout.push_back(out0);
    tx.vout.push_back(out1);

    auto inputs = GetNTP1InputsOnline(tx, true);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    EXPECT_EQ(tx.vout.size(), static_cast<unsigned>(2));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1], out1);
    EXPECT_FALSE(TxContainsOpReturn(&tx));
}

TEST(ntp1_tests, amend_tx_3)
{
    fTestNet = true; // ensure testnet state will be reset on exit
    std::unique_ptr<int, void (*)(int*)> temp(new int, [](int* i) {
        fTestNet = false;
        delete i;
    });

    std::string txidIn1 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    std::string txidIn2 = "a9b000ba0eb6b88ba8daad4b89997b2b457164983a18bbf2bffa0515b0c84348";
    CTxIn       in0(uint256(txidIn2), 0); // flipped order
    CTxIn       in1(uint256(txidIn1), 2);

    CScript script1;
    CKeyID  keyId;
    EXPECT_TRUE(CBitcoinAddress("TXM3QJ8DR2tSikeAYLY9UF6t3Lkjvt2Dva").GetKeyID(keyId));
    script1.SetDestination(keyId);
    CTxOut out0(96900000, script1);
    CTxOut out1(1000000, script1);

    CTransaction tx;

    tx.vin.push_back(in0);
    tx.vin.push_back(in1);

    tx.vout.push_back(out0);
    tx.vout.push_back(out1);

    auto inputs = GetNTP1InputsOnline(tx, true);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    EXPECT_EQ(tx.vout.size(), static_cast<unsigned>(4));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue, out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e540115032051");

    auto scriptPtr  = NTP1Script::ParseScript(opRetArg);
    auto scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
    EXPECT_NE(scriptPtrD, nullptr);

    EXPECT_EQ(scriptPtrD->getTransferInstructionsCount(), static_cast<unsigned>(1));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).amount, static_cast<unsigned>(50));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).outputIndex, static_cast<unsigned>(3));
}

TEST(ntp1_tests, amend_tx_4)
{
    fTestNet = true; // ensure testnet state will be reset on exit
    std::unique_ptr<int, void (*)(int*)> temp(new int, [](int* i) {
        fTestNet = false;
        delete i;
    });

    std::string txidIn0 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    std::string txidIn1 = "a9b000ba0eb6b88ba8daad4b89997b2b457164983a18bbf2bffa0515b0c84348";
    std::string txidIn2 = "4ffa03b170ea85c9138e4c8547b36ecfbd97105f57c206fce2703b372641f775";
    CTxIn       in0(uint256(txidIn0), 2);
    CTxIn       in1(uint256(txidIn1), 0);
    CTxIn       in2(uint256(txidIn2), 0);

    CScript script1;
    CKeyID  keyId;
    EXPECT_TRUE(CBitcoinAddress("TXM3QJ8DR2tSikeAYLY9UF6t3Lkjvt2Dva").GetKeyID(keyId));
    script1.SetDestination(keyId);
    CTxOut out0(96900000, script1);
    CTxOut out1(1000000, script1);

    CTransaction tx;

    tx.vin.push_back(in0);
    tx.vin.push_back(in1);
    tx.vin.push_back(in2);

    tx.vout.push_back(out0);
    tx.vout.push_back(out1);

    auto inputs = GetNTP1InputsOnline(tx, true);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    ASSERT_EQ(tx.vout.size(), static_cast<unsigned>(5));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue + tx.vout[4].nValue,
              out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e540115032051042041");

    auto scriptPtr  = NTP1Script::ParseScript(opRetArg);
    auto scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
    EXPECT_NE(scriptPtrD, nullptr);

    EXPECT_EQ(scriptPtrD->getTransferInstructionsCount(), static_cast<unsigned>(2));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).amount, static_cast<unsigned>(50));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).outputIndex, static_cast<unsigned>(3));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).amount, static_cast<unsigned>(40));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).outputIndex, static_cast<unsigned>(4));
}

TEST(ntp1_tests, amend_tx_5)
{
    fTestNet = true; // ensure testnet state will be reset on exit
    std::unique_ptr<int, void (*)(int*)> temp(new int, [](int* i) {
        fTestNet = false;
        delete i;
    });

    std::string txidIn0 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    std::string txidIn1 = "a9b000ba0eb6b88ba8daad4b89997b2b457164983a18bbf2bffa0515b0c84348";
    std::string txidIn2 = "4ffa03b170ea85c9138e4c8547b36ecfbd97105f57c206fce2703b372641f775";
    std::string txidIn3 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    CTxIn       in0(uint256(txidIn0), 2);
    CTxIn       in1(uint256(txidIn1), 0);
    CTxIn       in2(uint256(txidIn2), 0);
    CTxIn       in3(uint256(txidIn3), 3);

    CScript script1;
    CKeyID  keyId;
    EXPECT_TRUE(CBitcoinAddress("TXM3QJ8DR2tSikeAYLY9UF6t3Lkjvt2Dva").GetKeyID(keyId));
    script1.SetDestination(keyId);
    CTxOut out0(96900000, script1);
    CTxOut out1(1000000, script1);

    CTransaction tx;

    tx.vin.push_back(in0);
    tx.vin.push_back(in1);
    tx.vin.push_back(in2);
    tx.vin.push_back(in3);

    tx.vout.push_back(out0);
    tx.vout.push_back(out1);

    auto inputs = GetNTP1InputsOnline(tx, true);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    ASSERT_EQ(tx.vout.size(), static_cast<unsigned>(8));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue + tx.vout[4].nValue +
                  tx.vout[5].nValue + tx.vout[6].nValue + tx.vout[7].nValue,
              out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e5401150320510420410520e20638b10719");

    auto scriptPtr  = NTP1Script::ParseScript(opRetArg);
    auto scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
    EXPECT_NE(scriptPtrD, nullptr);

    EXPECT_EQ(scriptPtrD->getTransferInstructionsCount(), static_cast<unsigned>(5));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).amount, static_cast<unsigned>(50));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).outputIndex, static_cast<unsigned>(3));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).amount, static_cast<unsigned>(40));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).outputIndex, static_cast<unsigned>(4));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).amount, static_cast<unsigned>(1400));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).outputIndex, static_cast<unsigned>(5));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).amount, static_cast<unsigned>(3950));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).outputIndex, static_cast<unsigned>(6));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).amount, static_cast<unsigned>(25));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).outputIndex, static_cast<unsigned>(7));
}

TEST(ntp1_tests, amend_tx_6)
{
    fTestNet = true; // ensure testnet state will be reset on exit
    std::unique_ptr<int, void (*)(int*)> temp(new int, [](int* i) {
        fTestNet = false;
        delete i;
    });

    std::string txidIn0 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    std::string txidIn1 = "a9b000ba0eb6b88ba8daad4b89997b2b457164983a18bbf2bffa0515b0c84348";
    std::string txidIn2 = "4ffa03b170ea85c9138e4c8547b36ecfbd97105f57c206fce2703b372641f775";
    std::string txidIn3 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    // different order than the other test
    CTxIn in0(uint256(txidIn1), 0);
    CTxIn in1(uint256(txidIn0), 2);
    CTxIn in2(uint256(txidIn2), 0);
    CTxIn in3(uint256(txidIn3), 3);

    CScript script1;
    CKeyID  keyId;
    EXPECT_TRUE(CBitcoinAddress("TXM3QJ8DR2tSikeAYLY9UF6t3Lkjvt2Dva").GetKeyID(keyId));
    script1.SetDestination(keyId);
    CTxOut out0(96900000, script1);
    CTxOut out1(1000000, script1);

    CTransaction tx;

    tx.vin.push_back(in0);
    tx.vin.push_back(in1);
    tx.vin.push_back(in2);
    tx.vin.push_back(in3);

    tx.vout.push_back(out0);
    tx.vout.push_back(out1);

    auto inputs = GetNTP1InputsOnline(tx, true);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    ASSERT_EQ(tx.vout.size(), static_cast<unsigned>(8));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue + tx.vout[4].nValue +
                  tx.vout[5].nValue + tx.vout[6].nValue + tx.vout[7].nValue,
              out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e5401150320510420410520e20638b10719");

    auto scriptPtr  = NTP1Script::ParseScript(opRetArg);
    auto scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
    EXPECT_NE(scriptPtrD, nullptr);

    EXPECT_EQ(scriptPtrD->getTransferInstructionsCount(), static_cast<unsigned>(5));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).amount, static_cast<unsigned>(50));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).outputIndex, static_cast<unsigned>(3));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).amount, static_cast<unsigned>(40));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).outputIndex, static_cast<unsigned>(4));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).amount, static_cast<unsigned>(1400));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).outputIndex, static_cast<unsigned>(5));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).amount, static_cast<unsigned>(3950));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).outputIndex, static_cast<unsigned>(6));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).amount, static_cast<unsigned>(25));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).outputIndex, static_cast<unsigned>(7));
}

TEST(ntp1_tests, amend_tx_with_op_return)
{
    fTestNet = true; // ensure testnet state will be reset on exit
    std::unique_ptr<int, void (*)(int*)> temp(new int, [](int* i) {
        fTestNet = false;
        delete i;
    });

    std::string txidIn1 = "cee0c719a0375fde4137a415eb3f14fbf7afe6d83a9eca547a2134c4bc652c4f";
    std::string txidIn2 = "a9b000ba0eb6b88ba8daad4b89997b2b457164983a18bbf2bffa0515b0c84348";
    CTxIn       in0(uint256(txidIn1), 2);
    CTxIn       in1(uint256(txidIn2), 0);

    CScript script1;
    CKeyID  keyId;
    EXPECT_TRUE(CBitcoinAddress("TXM3QJ8DR2tSikeAYLY9UF6t3Lkjvt2Dva").GetKeyID(keyId));
    CScript script2;
    script2 = CScript() << OP_RETURN << ParseHex("ABC");
    script1.SetDestination(keyId);
    CTxOut out0(96900000, script1);
    CTxOut out1(1000000, script1);
    CTxOut out2(10000, script2);

    CTransaction tx;

    tx.vin.push_back(in0);
    tx.vin.push_back(in1);

    tx.vout.push_back(out0);
    tx.vout.push_back(out1);
    tx.vout.push_back(out2);

    auto inputs = GetNTP1InputsOnline(tx, true);
    EXPECT_ANY_THROW(NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1));
}

std::vector<std::pair<CTransaction, int>> GetInputsOnline(const CTransaction& tx, bool testnet)
{
    std::vector<std::pair<CTransaction, int>> inputs;
    for (const auto& in : tx.vin) {
        std::string rawInput = GetRawTxOnline(in.prevout.hash.ToString(), testnet);
        inputs.insert(inputs.begin(), std::make_pair(TxFromHex(rawInput), (int)in.prevout.n));
    }
    return inputs;
}

// TEST(ntp1_tests, total_fee_calculator)
//{
//    std::string txStr =
//    "01000000F18B745B01968C2B4EC329DD2D4EB5870344BDE419F96DCECEBA48836D837A2D9BCE66F"
//                        "36F040000006A473044022005F303750BD5C1022A693916B697853F6EAE96C4C39C1DDA7D309CAC"
//                        "B67903BA02200EC5AFE72A2A48EA6E3DF7D4321CD1B616155D02B2F2B9EA120877D978A657D3012"
//                        "10285985BE800D753339C665C2889E63CB24894B3C39DDC19BDE36F9F8CFA72613CFFFFFFFF0380"
//                        "841E00000000001976A914F164D4D21D041E328B6F446616D0B934D5B11A0D88AC1027000000000"
//                        "000126A104E54011202203202205202234202209250B52C00000000001976A9143BCD9645FE5D19"
//                        "B16CFB877C91A54DE07C70714588AC00000000";
//    CTransaction                              tx      = TxFromHex(txStr);
//    std::vector<std::pair<CTransaction, int>> inputs  = GetInputsOnline(tx, true);
//    uint64_t                                  totalIn = 0;
//    std::cout << inputs.size() << std::endl;
//    for (const auto& in : inputs) {
//        std::cout << in.first.GetHash().ToString() << "\t" << in.second << std::endl;
//        totalIn += in.first.vout[in.second].nValue;
//    }
//    std::cout << "Total in:  " << totalIn << std::endl;
//    std::cout << "Total out: " << tx.GetValueOut() << std::endl;
//    std::cout << "Total fee: " << totalIn - tx.GetValueOut() << std::endl;

//    std::cout << std::endl;

//    for (const auto& out : tx.vout) {
//        std::cout << out.nValue << "\t" << out.scriptPubKey.ToString() << std::endl;
//    }
//}

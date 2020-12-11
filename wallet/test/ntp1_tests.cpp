#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "chainparams.h"
#include "curltools.h"
#include "ntp1/ntp1apicalls.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1/ntp1sendtokensonerecipientdata.h"
#include "ntp1/ntp1tokenmetadata.h"
#include "ntp1/ntp1tokentxdata.h"
#include "ntp1/ntp1transaction.h"
#include "ntp1/ntp1txin.h"
#include "ntp1/ntp1txout.h"
#include "ntp1/ntp1v1_issuance_static_data.h"
#include "ntp1/ntp1wallet.h"
#include <boost/algorithm/string.hpp>
#include <boost/container/flat_map.hpp>
#include <fstream>
#include <random>
#include <unordered_map>
#include <unordered_set>

using boost::container::flat_map;

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
    std::cout << "The next parsing operation is expected to fail\n";
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
    EXPECT_EQ(tokenMetaData.getIconImageType(), "image/png");
    EXPECT_EQ(
        tokenMetaData.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/24d000d478d72d6a43b89bbabbe41a45acde9ba0.png");
    EXPECT_EQ(tokenMetaData.getIssuanceTxId().ToString(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssuanceTxIdHex(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
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
    EXPECT_EQ(tokenMetaData.getIconImageType(), "");
    EXPECT_EQ(tokenMetaData.getIconURL(), "");
    EXPECT_EQ(tokenMetaData.getIssuanceTxId().ToString(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssuanceTxIdHex(),
              "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
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
    auto TestWalletCachePath = Path(TEST_ROOT_PATH) / Path("/data") / NTP1WalletCacheFileName;
    ASSERT_TRUE(boost::filesystem::exists(TestWalletCachePath));
    EXPECT_NO_THROW(wallet1.importFromFile(TestWalletCachePath));
    Path tempWalletPath = Path(TEST_ROOT_PATH) / Path("/data/tmp.json");
    EXPECT_NO_THROW(wallet1.exportToFile(tempWalletPath));
    NTP1Wallet wallet2;
    EXPECT_NO_THROW(wallet2.importFromFile(tempWalletPath));
    EXPECT_NO_THROW(boost::filesystem::remove(Path(TEST_ROOT_PATH) / Path("/data/tmp.json")));
}

TEST(ntp1_tests, amount_to_int_random)
{
    auto seed = std::random_device{}();
    //    unsigned seed = 123456789;

    std::cout << "Using seed for random amount generator: " << seed << std::endl;
    std::mt19937 gen{seed};
    uint64_t     rangeMax = 1;
    rangeMax <<= 40;

    std::uniform_int_distribution<uint64_t> amount_dist{static_cast<uint64_t>(0), rangeMax};

    {
        // test the maximum
        std::string numHex          = NTP1Script::NumberToHexNTP1Amount(rangeMax);
        NTP1Int     amountToCompare = NTP1Script::NTP1AmountHexToNumber(numHex);
        ASSERT_EQ(amountToCompare, rangeMax);
    }

    const int tries_count = 100000;
    for (int i = 0; i < tries_count; i++) {
        uint64_t    amount64        = amount_dist(gen);
        NTP1Int     amount          = amount64;
        std::string numHex          = NTP1Script::NumberToHexNTP1Amount(amount);
        NTP1Int     amountToCompare = NTP1Script::NTP1AmountHexToNumber(numHex);
        EXPECT_EQ(amountToCompare, amount);
    }
}

TEST(ntp1_tests, amount_to_int)
{
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("69892a92"), 999901700);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("c007b60b6f687a"), 8478457292922);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("40ef54"), 38290000);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("201f"), 1000000000000000);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("60b0b460"), 723782);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("5545e1"), 871340);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("c007b60b6f687a"), 8478457292922);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("11"), 17);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("2011"), 10);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("2012"), 100);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("4bb3c1"), 479320);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("68c7e5b3"), 9207387000);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("8029990f1a"), 8723709100);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("a09c47f7b1a1"), 839027891720);
    EXPECT_EQ(NTP1Script::NTP1AmountHexToNumber("c0a60eea1aa8fd"), 182582987368701);

    EXPECT_ANY_THROW(NTP1Script::NTP1AmountHexToNumber("x"));
    EXPECT_ANY_THROW(NTP1Script::NTP1AmountHexToNumber(" "));
    EXPECT_ANY_THROW(NTP1Script::NTP1AmountHexToNumber("ssdsdmwdmo"));
    EXPECT_ANY_THROW(
        NTP1Script::NTP1AmountHexToNumber("999999999999999999999999999999999999999999999999"));

    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(999901700), "69892a92");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(8478457292922), "c007b60b6f687a");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(38290000), "40ef54");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(1000000000000000), "201f");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(723782), "60b0b460");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(871340), "5545e1");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(8478457292922), "c007b60b6f687a");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(17), "11");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(100), "2012");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(479320), "4bb3c1");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(9207387000), "68c7e5b3");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(8723709100), "8029990f1a");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(839027891720), "a09c47f7b1a1");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(182582987368701), "c0a60eea1aa8fd");

    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(999999999997990), "c38d7ea4c67826");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(276413656646664), "c0fb6591d0c408");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(9731165496688), "c008d9b6a9a570");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(943721684679640), "c35a4f53c83bd8");
    EXPECT_EQ(NTP1Script::NumberToHexNTP1Amount(1412849080), "80435eb161");
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
    EXPECT_EQ(script_transfer->getTransferInstruction(0).rawSize, 5);
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
        EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), std::runtime_error);
    }
    {
        // NI~BL name 4e497e424c
        toParse_issuance = "4e5401014e497e424cab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
                           "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), std::runtime_error);
    }
    {
        // NIBB. name 4e4942422e
        toParse_issuance = "4e5401014e4942422eab10c04e20e0aec73d58c8fbf2a9c26a6dc3ed666c7b80fef2"
                           "15620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0";
        EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), std::runtime_error);
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
            EXPECT_THROW(script = NTP1Script::ParseScript(toParse_issuance), std::runtime_error)
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
    EXPECT_EQ(script_burn->getTransferInstruction(0).rawSize, 3);
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

static CTransaction TxFromHex(const std::string& hex)
{
    CDataStream  stream(ParseHex(hex), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    return tx;
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_issuance)
{
    // issuance
    std::string transaction =
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
    EXPECT_TRUE(tx.CheckTransaction().isOk());

    std::string opReturnArg;
    EXPECT_TRUE(NTP1Transaction::IsTxNTP1(&tx, &opReturnArg));
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
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_transfer_1)
{
    // transfer
    std::string transaction =
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
    EXPECT_TRUE(tx.CheckTransaction().isOk());

    std::string opReturnArg;
    EXPECT_TRUE(NTP1Transaction::IsTxNTP1(&tx, &opReturnArg));
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
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_transfer_2_with_change)
{
    // transfer
    std::string transaction =
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
    EXPECT_TRUE(tx.CheckTransaction().isOk());

    std::string opReturnArg;
    EXPECT_TRUE(NTP1Transaction::IsTxNTP1(&tx, &opReturnArg));
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
}

TEST(ntp1_tests, parsig_ntp1_from_ctransaction_burn_with_transfer_1)
{
    // burn with transfer
    std::string transaction =
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
    EXPECT_TRUE(tx.CheckTransaction().isOk());

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
    EXPECT_TRUE(NTP1Transaction::IsTxNTP1(&tx, &opReturnArg));
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
}

std::string GetRawTxURL(const std::string& txid, NetworkType netType)
{
    if (netType == NetworkType::Mainnet) {
        return "https://ntp1node.nebl.io/ins/rawtx/" + txid;
    } else if (netType == NetworkType::Testnet) {
        return "https://ntp1node.nebl.io/testnet/ins/rawtx/" + txid;
    }
    assert(false);
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

    std::ifstream            ifs(testFile.string().c_str(), std::ifstream::in);
    std::vector<std::string> result;
    std::string              line;
    while (std::getline(ifs, line)) {
        if (line.empty())
            continue;
        result.push_back(line);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> read_map(const std::string& filename)
{
    std::vector<std::pair<std::string, std::string>> result;

    std::vector<std::string> dataLines = read_line_by_line(filename);
    result.reserve(dataLines.size());
    for (const std::string& dl : dataLines) {
        std::vector<std::string> line;
        boost::split(line, dl, boost::is_any_of("\t"));
        if (line.size() != 2) {
            throw std::runtime_error("While reading file " + filename +
                                     "; it was expected a table of two columns, but found " +
                                     std::to_string(dl.size()) + " in line " + dl);
        }
        result.push_back(std::make_pair(line[0], line[1]));
    }
    return result;
}

std::string NTP1Tests_GetTxidListFileName(NetworkType netType)
{
    if (netType == NetworkType::Mainnet) {
        return "ntp1txids_to_test.txt";
    } else if (netType == NetworkType::Testnet) {
        return "ntp1txids_to_test_testnet.txt";
    }
    assert(false);
}

std::string NTP1Tests_GetRawNeblioTxsFileName(NetworkType netType)
{
    if (netType == NetworkType::Mainnet) {
        return "txs_ntp1tests_raw_neblio_txs.txt";
    } else if (netType == NetworkType::Testnet) {
        return "txs_ntp1tests_raw_neblio_txs_testnet.txt";
    }
    assert(false);
}

std::string NTP1Tests_GetNTP1RawTxsFileName(NetworkType netType)
{
    if (netType == NetworkType::Mainnet) {
        return "txs_ntp1tests_ntp1_txs.txt";
    } else if (netType == NetworkType::Testnet) {
        return "txs_ntp1tests_ntp1_txs_testnet.txt";
    }
    assert(false);
}

std::string GetRawTxOnline(const std::string& txid, NetworkType netType)
{
    std::string rawTxJson =
        cURLTools::GetFileFromHTTPS_withRetries(64, 1000, GetRawTxURL(txid, netType), 0, 0);
    json_spirit::Value v;
    json_spirit::read_or_throw(rawTxJson, v);
    json_spirit::Object rawTxObj = v.get_obj();
    std::string         rawTx    = NTP1Tools::GetStrField(rawTxObj, "rawtx");
    return rawTx;
}

std::vector<std::pair<CTransaction, NTP1Transaction>> GetNTP1InputsOnline(const CTransaction& tx,
                                                                          NetworkType         netType)
{
    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs;

    for (int i = 0; i < (int)tx.vin.size(); i++) {
        std::string  inputTxid  = tx.vin[i].prevout.hash.ToString();
        std::string  inputRawTx = GetRawTxOnline(inputTxid, netType);
        CTransaction inputTx    = TxFromHex(inputRawTx);

        NTP1Transaction inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo(inputTxid, netType);

        inputs.push_back(std::make_pair(inputTx, inputNTP1Tx));
    }

    return inputs;
}

void TestNTP1TxParsing_onlyRead(const CTransaction& tx, NetworkType netType)
{
    //    const std::string& txid = tx.GetHash().ToString();

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs;

    for (int i = 0; i < (int)tx.vin.size(); i++) {
        std::string  inputTxid  = tx.vin[i].prevout.hash.ToString();
        std::string  inputRawTx = GetRawTxOnline(inputTxid, netType);
        CTransaction inputTx    = TxFromHex(inputRawTx);

        NTP1Transaction inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo(inputTxid, netType);

        inputs.push_back(std::make_pair(inputTx, inputNTP1Tx));
    }

    NTP1Transaction ntp1tx;
    ntp1tx.readNTP1DataFromTx(tx, inputs);
}

void TestScriptParsing(std::string OpReturnArg, const CTransaction& tx)
{
    // these transactions has an encoded NTP1 amount that is correct but unequivalent to our encoding
    // default
    std::string h = tx.GetHash().ToString();
    if (h == "087f6504c06dcdc04c6157d9cbcedb207bc2e179c7ab7655c9cb3ace9eccfec3" ||
        h == "0a71b6db7994cc91d7e24302428e44dc0871eff23caddfd75099e76666374175")
        return;

    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScript(OpReturnArg);
    scriptPtr->setEnableOpReturnSizeCheck(false);
    std::string calculatedScript = boost::algorithm::hex(scriptPtr->calculateScriptBin());
    std::transform(OpReturnArg.begin(), OpReturnArg.end(), OpReturnArg.begin(), ::tolower);
    std::transform(calculatedScript.begin(), calculatedScript.end(), calculatedScript.begin(),
                   ::tolower);
    EXPECT_EQ(calculatedScript, OpReturnArg)
        << "Calculated script doesn't match input script; at txid: " << tx.GetHash().ToString();
}

void TestNTP1TxParsing(const CTransaction& tx, NetworkType netType)
{
    const std::string&    txid       = tx.GetHash().ToString();
    const NTP1Transaction ntp1tx_ref = NTP1APICalls::RetrieveData_TransactionInfo(txid, netType);
    EXPECT_TRUE(tx.CheckTransaction().isOk()) << "Failed tx: " << txid;

    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs;

    std::string OpReturnArg;
    EXPECT_TRUE(NTP1Transaction::IsTxNTP1(&tx, &OpReturnArg));
    TestScriptParsing(OpReturnArg, tx);

    for (int i = 0; i < (int)tx.vin.size(); i++) {
        std::string  inputTxid  = tx.vin[i].prevout.hash.ToString();
        std::string  inputRawTx = GetRawTxOnline(inputTxid, netType);
        CTransaction inputTx    = TxFromHex(inputRawTx);

        NTP1Transaction inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo(inputTxid, netType);

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

void TestNTP1TxParsing(const std::string& txid, NetworkType netType)
{
    SwitchNetworkTypeTemporarily state_holder(netType);

    std::string  rawTx = GetRawTxOnline(txid, netType);
    CTransaction tx    = TxFromHex(rawTx);
    TestNTP1TxParsing(tx, netType);
}

void TestSingleNTP1TxParsingLocally(const CTransaction&                       tx,
                                    const flat_map<std::string, std::string>& nebltxs_map,
                                    const flat_map<std::string, std::string>& ntp1txs_map)
{
    const std::string& txid           = tx.GetHash().ToString();
    const std::string  ntp1tx_ref_str = ntp1txs_map.find(txid)->second;

    NTP1Transaction ntp1tx_ref;
    ntp1tx_ref.importJsonData(ntp1tx_ref_str);
    EXPECT_TRUE(tx.CheckTransaction().isOk()) << "Failed tx: " << txid;

    std::string OpReturnArg;
    EXPECT_TRUE(NTP1Transaction::IsTxNTP1(&tx, &OpReturnArg));

    TestScriptParsing(OpReturnArg, tx);

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

void TestSingleNTP1TxParsingLocally(const std::string&                        txid,
                                    const flat_map<std::string, std::string>& nebltxs_map,
                                    const flat_map<std::string, std::string>& ntp1txs_map)
{
    // This is temporary just to cut down the transactions required. Since all transactions come from a
    // list, if we try to reduce them for debugging, all of them will still be expected. This will ignore
    // those that don't exist since we truncated the full list of txids. if
    //    (nebltxs_map.find(txid) == nebltxs_map.cend()) {
    //        return;
    //    }

    ASSERT_NE(nebltxs_map.find(txid), nebltxs_map.cend());
    const std::string  rawTx = nebltxs_map.find(txid)->second;
    const CTransaction tx    = TxFromHex(rawTx);

    if (NTP1Transaction::IsTxNTP1(&tx)) {
        try {
            TestSingleNTP1TxParsingLocally(tx, nebltxs_map, ntp1txs_map);
        } catch (...) {
            std::cerr << "Error with transaction: " << tx.GetHash().ToString() << std::endl;
            throw;
        }
    }
}

void TestNTP1TxParsingLocally(NetworkType netType)
{
    SwitchNetworkTypeTemporarily state_holder(netType);

    flat_map<std::string, std::string> ntp1txs_map;
    flat_map<std::string, std::string> nebltxs_map;

    std::vector<std::string> txids = read_line_by_line(NTP1Tests_GetTxidListFileName(netType));

    // save memory by ensuring the destruction of json objects
    {
        using StrPairT = std::pair<std::string, std::string>;

        std::vector<StrPairT> nebltxs = read_map(NTP1Tests_GetRawNeblioTxsFileName(netType));
        std::vector<StrPairT> ntp1txs = read_map(NTP1Tests_GetNTP1RawTxsFileName(netType));

        auto pair_str_sort_functor = [](const std::pair<const std::string, const std::string>& p1,
                                        const std::pair<const std::string, const std::string>& p2) {
            return p1.first < p2.first;
        };

        std::sort(nebltxs.begin(), nebltxs.end(), pair_str_sort_functor);
        std::sort(ntp1txs.begin(), ntp1txs.end(), pair_str_sort_functor);

        nebltxs_map.reserve(nebltxs.size());
        ntp1txs_map.reserve(ntp1txs.size());

        for (const auto& el : nebltxs) {
            nebltxs_map[el.first] = el.second;
        }
        for (const auto& el : ntp1txs) {
            ntp1txs_map[el.first] = boost::algorithm::unhex(el.second);
        }
    }

    uint64_t count = 0;
    for (const auto& txid : txids) {
        if (count % 250 == 0)
            std::cout << "Finished testing " << count << " transactions" << std::endl;
        count++;
        if (Params().IsNTP1TxExcluded(uint256(txid))) {
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

    std::ofstream os(testFile.string().c_str());

    json_spirit::write_formatted(obj, os);
}

void write_map_to_table_file(const std::map<std::string, std::string>& data, const std::string& filename)
{
    namespace fs          = boost::filesystem;
    fs::path testRootPath = TEST_ROOT_PATH;
    fs::path testFile     = testRootPath / "data" / filename;

    std::ofstream os(testFile.string().c_str());

    static const std::string TAB     = std::string(1, '\t');
    static const std::string NEWLINE = std::string(1, '\n');

    for (const auto& p : data) {
        os.write(p.first.data(), p.first.size());
        os.write(TAB.data(), TAB.size());
        os.write(p.second.data(), p.second.size());
        os.write(NEWLINE.data(), NEWLINE.size());
    }
}

void DownloadAndCreateTxData(NetworkType netType)
{
    std::vector<std::string> txids = read_line_by_line(NTP1Tests_GetTxidListFileName(netType));

    std::map<std::string, std::string> rawNeblioTxsMap;
    std::map<std::string, std::string> ntp1TxsMap;

    std::vector<std::pair<std::string, std::string>> rawNeblioTxsVec(txids.size());
    std::vector<std::pair<std::string, std::string>> ntp1TxsVec(txids.size());

    std::atomic_int count{0};

    static const unsigned thread_count = std::thread::hardware_concurrency();

    // distribute the works over threads
    std::vector<unsigned> txs_per_thread(thread_count, 0);
    unsigned              total_txs_count = txids.size();
    for (unsigned i = 0; i < thread_count; i++) {
        if (i + 1 == thread_count) {
            txs_per_thread[i] = total_txs_count;
        } else {
            txs_per_thread[i] = txids.size() / thread_count;
        }
        total_txs_count -= txs_per_thread[i];
    }
    ASSERT_EQ(std::accumulate(txs_per_thread.cbegin(), txs_per_thread.cend(), 0), txids.size());

    std::vector<boost::promise<void>>       promises(thread_count);
    std::vector<boost::unique_future<void>> futures;
    for (auto&& p : promises) {
        futures.push_back(p.get_future());
    }

    std::vector<std::unique_ptr<std::thread>> threads;
    for (unsigned i = 0; i < thread_count; i++) {
        threads.push_back(MakeUnique<std::thread>(
            [&txids, netType, i, txs_per_thread, &rawNeblioTxsVec, &ntp1TxsVec, &promises, &count]() {
                // we get the start and end points by accumulating the txs_per_thread up to current
                // thread number
                unsigned start = 0;
                unsigned end   = 0;
                for (unsigned j = 0; j < i; j++) {
                    start += txs_per_thread[j];
                    end += txs_per_thread[j];
                }
                end += txs_per_thread[i];

                for (uint64_t j = start; j < (uint64_t)end; j++) {
                    const std::string rawTx = GetRawTxOnline(txids[j], netType);
                    const std::string ntp1tx_ref =
                        NTP1APICalls::RetrieveData_TransactionInfo_Str(txids[j], netType, 64);

                    rawNeblioTxsVec[j] = std::make_pair(txids[j], rawTx);
                    ntp1TxsVec[j]      = std::make_pair(txids[j], boost::algorithm::hex(ntp1tx_ref));

                    count.fetch_add(1, std::memory_order_relaxed);
                    if (count.load(std::memory_order_relaxed) % 100 == 0) {
                        std::cout << "Downloading tx: " << count.load(std::memory_order_relaxed)
                                  << std::endl;
                    }
                }
                promises[i].set_value();
            }));
        threads.back()->detach();
    }
    // wait for all threads to finish
    for (auto&& f : futures) {
        f.get();
    }

    ASSERT_EQ(rawNeblioTxsVec.size(), ntp1TxsVec.size());
    for (uint64_t i = 0; i < (uint64_t)rawNeblioTxsVec.size(); i++) {
        if (i % 100 == 0) {
            std::cout << "Parsing tx: " << i << std::endl;
        }

        const std::string&  rawTx      = rawNeblioTxsVec[i].second;
        const CTransaction& tx         = TxFromHex(rawTx);
        const std::string&  ntp1tx_ref = ntp1TxsVec[i].second;

        rawNeblioTxsMap[txids[i]] = rawTx;
        ntp1TxsMap[txids[i]]      = ntp1tx_ref;

        for (int i = 0; i < (int)tx.vin.size(); i++) {
            std::string inputTxid = tx.vin[i].prevout.hash.ToString();

            // if the tx is already in there, skip it
            if (rawNeblioTxsMap.find(inputTxid) != rawNeblioTxsMap.cend()) {
                continue;
            }

            std::string inputRawTxJson =
                cURLTools::GetFileFromHTTPS(GetRawTxURL(inputTxid, netType), 10000, 0);
            json_spirit::Value v;
            json_spirit::read_or_throw(inputRawTxJson, v);
            json_spirit::Object inputRawTxObj = v.get_obj();
            std::string         inputRawTx    = NTP1Tools::GetStrField(inputRawTxObj, "rawtx");

            std::string inputNTP1Tx = NTP1APICalls::RetrieveData_TransactionInfo_Str(inputTxid, netType);

            rawNeblioTxsMap[inputTxid] = inputRawTx;
            ntp1TxsMap[inputTxid]      = boost::algorithm::hex(inputNTP1Tx);
        }
    }

    const std::string f1 = NTP1Tests_GetRawNeblioTxsFileName(netType);
    const std::string f2 = NTP1Tests_GetNTP1RawTxsFileName(netType);

    std::remove(f1.c_str());
    std::remove(f2.c_str());

    write_map_to_table_file(rawNeblioTxsMap, f1);
    write_map_to_table_file(ntp1TxsMap, f2);
}

void DownloadTxidList(NetworkType netType)
{
    typedef boost::filesystem::path Path;

    namespace fs          = boost::filesystem;
    fs::path testRootPath = TEST_ROOT_PATH;

    std::vector<std::string> files;
    files.push_back(NTP1Tests_GetTxidListFileName(netType));

    for (uint64_t i = 0; i < (uint64_t)files.size(); i++) {
        std::cout << "Downloading test data file " << files[i] << std::endl;
        EXPECT_NO_THROW(boost::filesystem::remove(Path(TEST_ROOT_PATH) / Path("/data/" + files[i])));
        std::string content = cURLTools::GetFileFromHTTPS("https://assets.nebl.io/testdata/" + files[i], 10000, 0);
        fs::path    testFile = testRootPath / "data" / files[i];
        std::ofstream os(testFile.string().c_str());
        os << content;
        os.close();
    }
}

void DownloadPreMadeData(NetworkType netType)
{
    typedef boost::filesystem::path Path;

    namespace fs          = boost::filesystem;
    fs::path testRootPath = TEST_ROOT_PATH;

    std::vector<std::string> files;
    files.push_back(NTP1Tests_GetTxidListFileName(netType));
    files.push_back(NTP1Tests_GetRawNeblioTxsFileName(netType));
    files.push_back(NTP1Tests_GetNTP1RawTxsFileName(netType));

    for (uint64_t i = 0; i < (uint64_t)files.size(); i++) {
        std::cout << "Downloading test data file " << files[i] << std::endl;
        EXPECT_NO_THROW(boost::filesystem::remove(Path(TEST_ROOT_PATH) / Path("/data/" + files[i])));
        std::string content = cURLTools::GetFileFromHTTPS("https://assets.nebl.io/testdata/" + files[i], 10000, 0);
        fs::path    testFile = testRootPath / "data" / files[i];
        std::ofstream os(testFile.string().c_str());
        os << content;
        os.close();
    }
}

TEST(ntp1_tests, run_parse_test)
{
// we either create the data or download the premade one, unless downloading premade data is disabled
#ifndef UNITTEST_DOWNLOAD_AND_CREATE_TX_DATA
#if defined(UNITTEST_RUN_NTP1_PARSE_TESTS) && !defined(UNITTEST_FORCE_DISABLE_PREMADE_DATA_DOWNLOAD)
    DownloadPreMadeData(NetworkType::Testnet);
    DownloadPreMadeData(NetworkType::Mainnet);
#endif
    // if tests won't run, then no need to download the premade tx data
#else
    // we can optionally download the new list of TXids
#ifdef UNITTEST_REDOWNLOAD_TXID_LIST
    DownloadTxidList(NetworkType::Testnet);
    DownloadTxidList(NetworkType::Mainnet);
#endif
    DownloadAndCreateTxData(NetworkType::Testnet);
    DownloadAndCreateTxData(NetworkType::Mainnet);
#endif
#ifdef UNITTEST_RUN_NTP1_PARSE_TESTS
    EXPECT_NO_THROW(TestNTP1TxParsingLocally(NetworkType::Testnet));
    EXPECT_NO_THROW(TestNTP1TxParsingLocally(NetworkType::Mainnet));
#endif
}

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

CTransaction GetTxOnline(const std::string& txid, NetworkType netType)
{
    std::string rawTx = GetRawTxOnline(txid, netType);
    return TxFromHex(rawTx);
}

TEST(ntp1_tests, amend_tx_1)
{
    SwitchNetworkTypeTemporarily state_holder(NetworkType::Testnet);

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

    auto inputs = GetNTP1InputsOnline(tx, NetworkType::Testnet);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    EXPECT_EQ(tx.vout.size(), static_cast<unsigned>(4));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue, out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(NTP1Transaction::TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e54031001032051");

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
    SwitchNetworkTypeTemporarily state_holder(NetworkType::Testnet);

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

    auto inputs = GetNTP1InputsOnline(tx, NetworkType::Testnet);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    EXPECT_EQ(tx.vout.size(), static_cast<unsigned>(2));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1], out1);
    EXPECT_FALSE(NTP1Transaction::TxContainsOpReturn(&tx));
}

TEST(ntp1_tests, amend_tx_3)
{
    SwitchNetworkTypeTemporarily state_holder(NetworkType::Testnet);

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

    auto inputs = GetNTP1InputsOnline(tx, NetworkType::Testnet);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    EXPECT_EQ(tx.vout.size(), static_cast<unsigned>(4));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue, out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(NTP1Transaction::TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e54031001032051");

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
    SwitchNetworkTypeTemporarily state_holder(NetworkType::Testnet);

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

    auto inputs = GetNTP1InputsOnline(tx, NetworkType::Testnet);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    ASSERT_EQ(tx.vout.size(), static_cast<unsigned>(5));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue + tx.vout[4].nValue,
              out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(NTP1Transaction::TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e54031002032051042041");

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
    SwitchNetworkTypeTemporarily state_holder(NetworkType::Testnet);

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

    auto inputs = GetNTP1InputsOnline(tx, NetworkType::Testnet);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    ASSERT_EQ(tx.vout.size(), static_cast<unsigned>(8));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue + tx.vout[4].nValue +
                  tx.vout[5].nValue + tx.vout[6].nValue + tx.vout[7].nValue,
              out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(NTP1Transaction::TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e540310050320510420410520e20638b10719");

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
    SwitchNetworkTypeTemporarily state_holder(NetworkType::Testnet);

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

    auto inputs = GetNTP1InputsOnline(tx, NetworkType::Testnet);
    NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1);

    ASSERT_EQ(tx.vout.size(), static_cast<unsigned>(8));
    EXPECT_EQ(tx.vout[0], out0);
    EXPECT_EQ(tx.vout[1].nValue + tx.vout[2].nValue + tx.vout[3].nValue + tx.vout[4].nValue +
                  tx.vout[5].nValue + tx.vout[6].nValue + tx.vout[7].nValue,
              out1.nValue);
    std::string opRetArg;
    EXPECT_TRUE(NTP1Transaction::TxContainsOpReturn(&tx, &opRetArg));
    EXPECT_EQ(opRetArg, "4e540310050320510420410520e20638b10719");

    auto scriptPtr  = NTP1Script::ParseScript(opRetArg);
    auto scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
    EXPECT_NE(scriptPtrD, nullptr);

    EXPECT_EQ(scriptPtrD->getTransferInstructionsCount(), static_cast<unsigned>(5));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).amount, static_cast<unsigned>(50));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).outputIndex, static_cast<unsigned>(3));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).firstRawByte, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).rawAmount,
              boost::algorithm::unhex(std::string("2051")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).amount, static_cast<unsigned>(40));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).outputIndex, static_cast<unsigned>(4));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).firstRawByte, 4);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).rawAmount,
              boost::algorithm::unhex(std::string("2041")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).amount, static_cast<unsigned>(1400));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).outputIndex, static_cast<unsigned>(5));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).firstRawByte, 5);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).rawAmount,
              boost::algorithm::unhex(std::string("20e2")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).amount, static_cast<unsigned>(3950));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).outputIndex, static_cast<unsigned>(6));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).firstRawByte, 6);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).rawAmount,
              boost::algorithm::unhex(std::string("38b1")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).amount, static_cast<unsigned>(25));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).outputIndex, static_cast<unsigned>(7));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).firstRawByte, 7);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).rawSize, 2);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).rawAmount,
              boost::algorithm::unhex(std::string("19")));
}

TEST(ntp1_tests, some_transfer_instructions_test)
{
    // this test I added just so that I manipulate skipInput and test it

    const std::string script = "4e540310050320510420418520e20638b18719";

    auto scriptPtr  = NTP1Script::ParseScript(script);
    auto scriptPtrD = std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
    EXPECT_NE(scriptPtrD, nullptr);

    EXPECT_EQ(scriptPtrD->getTransferInstructionsCount(), static_cast<unsigned>(5));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).amount, static_cast<unsigned>(50));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).outputIndex, static_cast<unsigned>(3));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).firstRawByte, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(0).rawAmount,
              boost::algorithm::unhex(std::string("2051")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).amount, static_cast<unsigned>(40));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).outputIndex, static_cast<unsigned>(4));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).firstRawByte, 4);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(1).rawAmount,
              boost::algorithm::unhex(std::string("2041")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).amount, static_cast<unsigned>(1400));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).skipInput, true);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).outputIndex, static_cast<unsigned>(5));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).firstRawByte, 133);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(2).rawAmount,
              boost::algorithm::unhex(std::string("20e2")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).amount, static_cast<unsigned>(3950));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).skipInput, false);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).outputIndex, static_cast<unsigned>(6));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).firstRawByte, 6);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).rawSize, 3);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(3).rawAmount,
              boost::algorithm::unhex(std::string("38b1")));

    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).amount, static_cast<unsigned>(25));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).skipInput, true);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).outputIndex, static_cast<unsigned>(7));
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).firstRawByte, 135);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).rawSize, 2);
    EXPECT_EQ(scriptPtrD->getTransferInstruction(4).rawAmount,
              boost::algorithm::unhex(std::string("19")));
}

TEST(ntp1_tests, amend_tx_with_op_return)
{
    SwitchNetworkTypeTemporarily state_holder(NetworkType::Testnet);

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

    auto inputs = GetNTP1InputsOnline(tx, NetworkType::Testnet);
    EXPECT_ANY_THROW(NTP1Transaction::AmendStdTxWithNTP1(tx, inputs, 1));
}

std::vector<std::pair<CTransaction, int>> GetInputsOnline(const CTransaction& tx, NetworkType netType)
{
    std::vector<std::pair<CTransaction, int>> inputs;
    for (const auto& in : tx.vin) {
        std::string rawInput = GetRawTxOnline(in.prevout.hash.ToString(), netType);
        inputs.insert(inputs.begin(), std::make_pair(TxFromHex(rawInput), (int)in.prevout.n));
    }
    return inputs;
}

TEST(ntp1_tests, op_return_NTP1v3_test1)
{
    std::string                          opReturnArg = "4e540310020022a00160f42160";
    std::shared_ptr<NTP1Script>          script      = NTP1Script::ParseScript(opReturnArg);
    std::shared_ptr<NTP1Script_Transfer> script_transfer =
        std::dynamic_pointer_cast<NTP1Script_Transfer>(script);
    EXPECT_EQ(script_transfer->getHeader(), boost::algorithm::unhex(opReturnArg.substr(0, 6)));
    EXPECT_EQ(script_transfer->getHexMetadata().size(), static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getOpCodeBin()), "10");
    EXPECT_EQ(script_transfer->getTxType(), NTP1Script::TxType::TxType_Transfer);

    EXPECT_EQ(script_transfer->getTransferInstructionsCount(), static_cast<unsigned>(2));

    EXPECT_EQ(script_transfer->getTransferInstruction(0).amount, static_cast<uint64_t>(42));
    EXPECT_EQ(script_transfer->getTransferInstruction(0).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(0).outputIndex, static_cast<unsigned>(0));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(0).rawAmount), "22A0");
    EXPECT_EQ(script_transfer->getTransferInstruction(0).firstRawByte, 0);

    EXPECT_EQ(script_transfer->getTransferInstruction(1).amount, static_cast<uint64_t>(999958));
    EXPECT_EQ(script_transfer->getTransferInstruction(1).skipInput, false);
    EXPECT_EQ(script_transfer->getTransferInstruction(1).outputIndex, static_cast<unsigned>(1));
    EXPECT_EQ(boost::algorithm::hex(script_transfer->getTransferInstruction(1).rawAmount), "60F42160");
    EXPECT_EQ(script_transfer->getTransferInstruction(1).firstRawByte, 1);
}

TEST(ntp1_tests, metadata_decompression_issuance)
{
    // txid: f34666880ec73602e6ec72a8508cd4df5559190b2d2bbc6cf8decc57d13d352b
    std::string issuance_opRet =
        "4e540301524f4d415001010001f000000965789c8d58db72db3812fd15ac1fa6762ab2123b9924cecb96622b89666cc"
        "b9194496d6dcd03488212c624c000a0146e2a55f98daddafdb97cc99e6e9014e5785dfb648b22fa72fa74f781be1c65"
        "32c8a3575f8e82bd55e65a96eae8d5d1627e35b9391a1d65caa74e57415b83a7d72a29b4150b2bb35256f85a7b5f2bd"
        "77fb352b2c4d3da15fee8d53fbe1c99684ca738cd8ff1611342e55f3d7e6c4275724cdff8b12cfdd371a6d73ac8c2a6"
        "4a1a5fc954f9716acbc7cf5fbc3c4b7ef9e549fee2e4997afa4cbd4cce5ecae72a7bf1e4797a76969f9dbe789ee74a9"
        "e8d2bb3868f52976ad554ecb5946bf5981e7ffd03cebd72176da2a5a2bf88ef563578f18d4ac4e993931738be95454d"
        "67571b254ae96e55107e63775e486194ca446e9df0baac0a2512847a9b6ea436c2dba226843c7f9fd45e1be571c6642"
        "2c090ce941436172d7ada8bc43a3316aff716f0cc2b6584a4534297a5325e158da07864026f41a51b630bbb6e0e7c28"
        "9c2b6551b0af42bab51a8b7776a7b6cab1eb606d1103d9c728b752176c543a7e4300e7aad0a90c6d863b442c6466b9e"
        "ce32eee9d8c81e388489c36eb1f91384ea4878d8157e5b61aa5a433618f293e654e6ff74e081e8e779f2650a1273d9a"
        "3beb8a6c8c1a8558de65a0188ebe8eba329ef820dec922275bef9fdeade81efcaac67f29c095c6d8daa42a1b0f4ad33"
        "fdcbf47905028b9767011c0b0b178fb6106408a02c9c82a82d713e0a3361938331257329d2f470cc4a536f567067c0f"
        "3fd2ce64833fb047846b83f818cd7a5bbb54a136a805c555783b087d68e4ad0eefeae4c0c6805ad72a00ba5bb1b6284"
        "341a0db882cfb8eb077498376e121884f91c9ff8678927eaa35325cd94aac64015b54e8d7b52e3231af037b6ae33bb7"
        "78ef02342d6c454e19a379a55c0b230d123f161fd5f76fffc29b1bcd7c633e281fdf0605d79b409f4ae0cd0cfbd322d"
        "fc4868d0076400e27b33b3eecde47883e2678abb1353adc2ba351bef688a23e80a94649e72961f519cfb4024cc4cdf3"
        "478f46627c3d5d8dc44d1336d68c04ceca6c2b993c939b99e0ee20cfd1e1dfc41cc62d3bdbc9084edb0f1459a6a8386"
        "09aab07ac676c456cabba6a2b37183d8386e124900cf1447a0f97686821135b8791d86d34c84244202039310058149a"
        "0249ed317da31c629ce53030427ba28835cdbd02817080daa4459d31584e38e5eb52c12e5508f327615082add30dce5"
        "bf397ffb7519fdd65d1b450293028d132083f3603d1df7671db3cd7a99685981844a933f193d0f3a5985415aab9008f"
        "334ad11e12eec75e8d89d515e520aa42f25c2b24da6023ba18da79b66b834086ef9f8dfb6fc5c1b7a3d8a2b7c6ee788"
        "e7ffff6ef823268bffefeed3f235139bbe5d98aba824a811b305146e51ae66d37b57d00ded26587c10f1c7701018a02"
        "bb5894047bdbcbc63a6c047285b9eb63db4864de4150d6801f34ac2b4a980634ea1edf0110ae36dde44d0b5b672350f"
        "4b6edbd52e4d207e546f446294d2352491ba8a446f32aad9d8ac38ec329b83d1d4d9b26b66359d5f4005ee1859a1bf5"
        "a4d545b0813f117baeddfd153e18b46183a1f8a9960e26318ef5960e520b405104678b88267bee678ec6b9bf5e4f5f5"
        "ffe8c309bdd46b9c86c7c8853bd7d33e53739b8843a88f62f87e8bb31e254aad0ad1965b7d52ed4c0b611b4761be252"
        "c01a281f6a818341fa430b2c43c4bc233f2d9585f455a21cecdf680ee102338052ef6074aa5028079127f6f8e030cf4"
        "4aae90f564870b4a6040b26b00c4d4675633ac1606df4a75a0d0404bb5bd32669f1226de5e322493448df30b31087a2"
        "87dab580b6d3ab3d64dabd84a7183276775c41b7380e4a71fd09eb0e4b3288396c5aab4eedd01e3ece83a10b7a409d0"
        "f821966125a1c89100b829399a604c0abbe7fc8f7cdb9c8eba2880cb8642fcc689e9c3d9c141794e03eb396d4e49588"
        "f4d0da7c7f42157e39a8f062ba5cc129ad8958a199018f651afae93c40aadde05cea045b02348bfdba172568cb8abab"
        "236b41aba5d59d90010a99180c1d0e25079c6a228f25f39ed55145a2439783fc51a0ef70c58a6631b9378af4b562fce"
        "d6eb1834c840ee689d06fa07f4e10ab6dbd71cb7c2e600026d0e6722c6f1ba061bd1afdd767d8badf42bd4d4a24e9a6"
        "ef3fe0a1d3412d79048e33ffdcf4355cc5a384a50d6c7b14564a533f469a6aac23660e0e770bc26d53d1c2c872cbd47"
        "48151a981896d889cae3ec533d9083ddac52eb1b54a67c901ba777b971c59a806a782ecb4aeab511973c1ad19afb2a9"
        "5fd5b69f756ea54dc6cbdbce6db87d14c81a1d46e93bb5f47dc15deca60c259c3da1043148432b5ec2c61e83b6a548f"
        "f60a6aad55ac65165b8ddbd66c208928d0dca635b12af62e91a237d3ea20ccb058858dae625fec7979ccb792fb6e5d0"
        "fc2fbf42ebc030cdfcec56a2eae268bdfa62b31b9be10b41b6673a2e4efa7e2f574351197930fd7e7efc662965127e5"
        "4dbcd411b982303addf0254afa6ef976f71beace1492a2dded2277b68cab9d44a1ce3b0d42ebffbe22a0d49f25c947d"
        "fcbae774a16619392c2c3f6b1042176307a840a3312cb1a246ec4391be21d8869029aae2daacfbe287280a033fe1837"
        "b535ac82346787d6db5b3c680adc55f70a94a6fd718edb7022d3db38aff79032ef100044862d1589694fd4748aaf066"
        "4d893cce09b27a3b8d7f541f63bac1dd92d4d494b2f6bd252dbd3b1b87071d7e7b5e33da57177d9c60c49fdefd18402"
        "0a368512b883395f1efa3b7a77cb7d8843cfee726876757339bd9a5eaf26abd9fc9aa97331bd9cfd3e5dfc5dccdf0c6"
        "824961f66aba9009bdecc170207a68b9bc56c3915e71f96abf9d574b11c8b37dc169ddaa0344ad689c8f15ef4747bbf"
        "e973a820820c6f60ea360f1936ac485f8a7e00f08f0db204b97c41ed8ebe757cf767d35d470f4cb45a528107b3c0f7a"
        "7ad6aa5f1f07ec5d391f348e394a001c48b81162950dcc7d707e561cce7b4f8daca1c96730fc53e5dba7a14518d1371"
        "782c0c56476c41e0c4f28e81bafbfbc45eca74825c1b7a1b391c5ebc1ebc0b3f39397b34e444870c76799fc24ff8902"
        "157d71c7f243f93fe4715baac47cddcdefe52453d5be87f2240f519e19a35378236fda41cd188cfe24ac77da31b033c"
        "92e8b73b1e43f1c6c8bf3490b8ce713ae3df64188b28d59266b8e33a91d5aa1f6e2e9a57a44ad938b15ff83a8d24881"
        "25f7bd21c9e6f4f5c739fca226e9cf626db746d3c60d260cd24aab1301487685bb451cf460a02ff5bc88212d5d68146"
        "20ecb13889abc0569585f2a6f5a6da0b4cae0d0bf1f8f3d4fdebadff250aca2d6076dfb79dfff8faf5eb7f01d62e81a"
        "9";

    std::shared_ptr<NTP1Script>          p  = NTP1Script::ParseScript(issuance_opRet);
    std::shared_ptr<NTP1Script_Issuance> pd = std::dynamic_pointer_cast<NTP1Script_Issuance>(p);
    ASSERT_NE(pd, nullptr);

    ASSERT_EQ(pd->getTokenSymbol(), "ROMAP");
    ASSERT_EQ(pd->getAmount(), 1);
    ASSERT_EQ(pd->getDivisibility(), 7);
    ASSERT_EQ(pd->isLocked(), true);
    ASSERT_EQ(pd->getOpCodeBin(), boost::algorithm::unhex(std::string("01")));
    ASSERT_EQ(pd->getProtocolVersion(), 3);
    ASSERT_EQ(pd->getTransferInstructionsCount(), 1);
    ASSERT_EQ(pd->getTransferInstruction(0).amount, 1);
    ASSERT_EQ(pd->getTransferInstruction(0).firstRawByte, 0);
    ASSERT_EQ(pd->getTransferInstruction(0).rawAmount, boost::algorithm::unhex(std::string("01")));
    ASSERT_EQ(pd->getTransferInstruction(0).skipInput, false);
    ASSERT_EQ(pd->getTransferInstruction(0).outputIndex, 0);
    ASSERT_EQ(pd->getTransferInstruction(0).rawSize, 2);
    ASSERT_EQ(pd->getTxType(), NTP1Script::TxType::TxType_Issuance);
    ASSERT_EQ(pd->getHeader(), boost::algorithm::unhex(std::string("4e5403")));
    ASSERT_EQ(pd->getAggregationPolicyStr(),
              NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable_Str);
    ASSERT_EQ(pd->getAggregationPolicy(),
              NTP1Script::IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable);
    ASSERT_EQ(
        boost::algorithm::unhex(pd->getHexMetadata()),
        boost::algorithm::unhex(std::string(
            "789C8D58DB72DB3812FD15AC1FA6762AB2123B9924CECB96622B89666CCB9194496D6DCD03488212C624C000A01"
            "46E2A55F98DADDAFDB97CC99E6E9014E5785DFB648B22FA72FA74F781BE1C6532C8A3575F8E82BD55E65A96EAE8"
            "D5D1627E35B9391A1D65CAA74E57415B83A7D72A29B4150B2BB35256F85A7B5F2BD77FB352B2C4D3DA15FEE8D53"
            "FBE1C99684CA738CD8FF1611342E55F3D7E6C4275724CDFF8B12CFDD371A6D73AC8C2A64A1A5FC954F9716ACBC7"
            "CF5FBC3C4B7EF9E549FEE2E4997AFA4CBD4CCE5ECAE72A7BF1E4797A76969F9DBE789EE74A9E8D2BB3868F52976"
            "AD554ECB5946BF5981E7FFD03CEBD72176DA2A5A2BF88EF563578F18D4AC4E993931738BE95454D67571B254AE9"
            "6E55107E63775E486194CA446E9DF0BAAC0A2512847A9B6EA436C2DBA226843C7F9FD45E1BE571C66422C090CE9"
            "41436172D7ADA8BC43A3316AFF716F0CC2B6584A4534297A5325E158DA07864026F41A51B630BBB6E0E7C289C2B"
            "6551B0AF42BAB51A8B7776A7B6CAB1EB606D1103D9C728B752176C543A7E4300E7AAD0A90C6D863B442C6466B9E"
            "CE32EEE9D8C81E388489C36EB1F91384EA4878D8157E5B61AA5A433618F293E654E6FF74E081E8E779F2650A127"
            "3D9A3BEB8A6C8C1A8558DE65A0188EBE8EBA329EF820DEC922275BEF9FDEADE81EFCAAC67F29C095C6D8DAA42A1"
            "B0F4AD33FDCBF47905028B9767011C0B0B178FB6106408A02C9C82A82D713E0A3361938331257329D2F470CC4A5"
            "36F567067C0F3FD2CE64833FB047846B83F818CD7A5BBB54A136A805C555783B087D68E4AD0EEFEAE4C0C6805AD"
            "72A00BA5BB1B6284341A0DB882CFB8EB077498376E121884F91C9FF8678927EAA35325CD94AAC64015B54E8D7B5"
            "2E3231AF037B6AE33BB778EF02342D6C454E19A379A55C0B230D123F161FD5F76FFFC29B1BCD7C633E281FDF060"
            "5D79B409F4AE0CD0CFBD322DFC4868D0076400E27B33B3EECDE47883E2678ABB1353ADC2BA351BEF688A23E80A9"
            "4649E72961F519CFB4024CC4CDF3478F46627C3D5D8DC44D1336D68C04CECA6C2B993C939B99E0EE20CFD1E1DFC"
            "41CC62D3BDBC9084EDB0F1459A6A838609AAB07AC676C456CABBA6A2B37183D8386E124900CF1447A0F97686821"
            "135B8791D86D34C84244202039310058149A0249ED317DA31C629CE53030427BA28835CDBD02817080DAA4459D3"
            "1584E38E5EB52C12E5508F327615082ADD30DCE5BF397FFB7519FDD65D1B450293028D132083F3603D1DF7671DB"
            "3CD7A99685981844A933F193D0F3A5985415AAB9008F334AD11E12EEC75E8D89D515E520AA42F25C2B24DA6023B"
            "A18DA79B66B834086EF9F8DFB6FC5C1B7A3D8A2B7C6EE788E7FFFF6EF823268BFFEFEED3F235139BBE5D98ABA82"
            "4A811B305146E51AE66D37B57D00DED26587C10F1C7701018A02BB5894047BDBCBC63A6C047285B9EB63DB4864D"
            "E4150D6801F34AC2B4A980634EA1EDF0110AE36DDE44D0B5B672350F4B6EDBD52E4D207E546F446294D2352491B"
            "A8A446F32AAD9D8AC38EC329B83D1D4D9B26B66359D5F4005EE1859A1BF5A4D545B0813F117BAEDDFD153E18B46"
            "183A1F8A9960E26318EF5960E520B405104678B88267BEE678EC6B9BF5E4F5F5FFE8C309BDD46B9C86C7C8853BD"
            "7D33E53739B8843A88F62F87E8BB31E254AAD0AD1965B7D52ED4C0B611B4761BE252C01A281F6A818341FA430B2"
            "C43C4BC233F2D9585F455A21CECDF680EE102338052EF6074AA5028079127F6F8E030CF44AAE90F564870B4A604"
            "0B26B00C4D4675633AC1606DF4A75A0D0404BB5BD32669F1226DE5E322493448DF30B31087A287DAB580B6D3AB3"
            "D64DABD84A7183276775C41B7380E4A71FD09EB0E4B3288396C5AAB4EEDD01E3ECE83A10B7A409D0F821966125A"
            "1C89100B829399A604C0ABBE7FC8F7CDB9C8EBA2880CB8642FCC689E9C3D9C141794E03EB396D4E49588F4D0DA7"
            "C7F42157E39A8F062BA5CC129AD8958A199018F651AFAE93C40AADDE05CEA045B02348BFDBA172568CB8ABAB236"
            "B41ABA5D59D90010A99180C1D0E25079C6A228F25F39ED55145A2439783FC51A0EF70C58A6631B9378AF4B562FC"
            "ED6EB1834C840EE689D06FA07F4E10AB6DBD71CB7C2E600026D0E6722C6F1BA061BD1AFDD767D8BADF42BD4D4A2"
            "4E9A6EF3FE0A1D3412D79048E33FFDCF4355CC5A384A50D6C7B14564A533F469A6AAC23660E0E770BC26D53D1C2"
            "C872CBD4748151A981896D889CAE3EC533D9083DDAC52EB1B54A67C901BA777B971C59A806A782ECB4AEAB51197"
            "3C1AD19AFB2A95FD5B69F756EA54DC6CBDBCE6DB87D14C81A1D46E93BB5F47DC15DECA60C259C3DA1043148432B"
            "5EC2C61E83B6A548FF60A6AAD55AC65165B8DDBD66C208928D0DCA635B12AF62E91A237D3EA20CCB058858DAE62"
            "5FEC7979CCB792FB6E5D0FC2FBF42EBC030CDFCEC56A2EAE268BDFA62B31B9BE10B41B6673A2E4EFA7E2F574351"
            "197930FD7E7EFC662965127E54DBCD411B982303ADDF0254AFA6EF976F71BEACE1492A2DDED2277B68CAB9D44A1"
            "CE3B0D42EBFFBE22A0D49F25C947DFCBAE774A16619392C2C3F6B1042176307A840A3312CB1A246EC4391BE21D8"
            "869029AAE2DAACFBE287280A033FE1837B535AC82346787D6DB5B3C680ADC55F70A94A6FD718EDB7022D3DB38AF"
            "F79032EF100044862D1589694FD4748AAF0664D893CCE09B27A3B8D7F541F63BAC1DD92D4D494B2F6BD252DBD3B"
            "1B87071D7E7B5E33DA57177D9C60C49FDEFD184020A368512B883395F1EFA3B7A77CB7D8843CFEE726876757339"
            "BD9A5EAF26ABD9FC9AA97331BD9CFD3E5DFC5DCCDF0C6824961F66ABA9009BDECC170207A68B9BC56C3915E71F9"
            "6ABF9D574B11C8B37DC169DDAA0344AD689C8F15EF4747BBFE973A820820C6F60EA360F1936AC485F8A7E00F08F"
            "0DB204B97C41ED8EBE757CF767D35D470F4CB45A528107B3C0F7A7AD6AA5F1F07EC5D391F348E394A001C48B811"
            "62950DCC7D707E561CCE7B4F8DACA1C96730FC53E5DBA7A14518D1371782C0C56476C41E0C4F28E81BAFBFBC45E"
            "CA74825C1B7A1B391C5EBC1EBC0B3F39397B34E444870C76799FC24FF8902157D71C7F243F93FE4715BAAC47CDD"
            "CDEFE52453D5BE87F2240F519E19A35378236FDA41CD188CFE24AC77DA31B033C92E8B73B1E43F1C6C8BF3490B8"
            "CE713AE3DF64188B28D59266B8E33A91D5AA1F6E2E9A57A44AD938B15FF83A8D2488125F7BD21C9E6F4F5C739FC"
            "A226E9CF626DB746D3C60D260CD24AAB1301487685BB451CF460A02FF5BC88212D5D6814620ECB13889ABC05695"
            "85F2A6F5A6DA0B4CAE0D0BF1F8F3D4FDEBADFF250ACA2D6076DFB79DFFF8FAF5EB7F01D62E81A9")));

    std::string expected =
        R"({"data":{"tokenName":"ROMAP","description":"Neblio Roadmap","issuer":"NeblioTeam","urls":[{"name":"icon","url":"https://ntp1-icons.ams3.digitaloceanspaces.com/6789b550f714e34e8b98a6ed706c99f9276ffea9.png","mimeType":"image/png"}],"userData":{"meta":[{"key":"Feb 2017","value":"The market shows a need for simple blockchain solutions for business and the idea of Neblio is born. Blockchain is seen as an immensely valuable technology for businesses small and large. However the tools and solutions available are too complicated for wide adoption. Neblio was born to bring simple blockchain-based tools and services to the market to drive adoption of the technology in the business world.","type":"String"},{"key":"1st Half of Q3 2017","value":"Neblio is publicly announced. Neblio is announced publicly for the first time. GUI wallet applications for Windows, MacOS, and Linux are available on day one. The Neblio Wallet source code is also publicly available on GitHub. The Neblio Blockchain Network goes live on the day of the announcement.","type":"String"},{"key":"2nd Half of Q3 2017","value":"Acquire Top Talent to Build Out the Neblio Core Development and Operations Teams. We’re hiring the best and brightest minds to join both our core development and operations teams. Are you a senior developer with years of experience in C++, .NET, Python, or advanced API implementations? Or do you want to market and deliver true business value based upon the blockchain technology you are so passionate about, while working with brilliant co-workers? If so, drop us a line and include your resume, we’ll be in touch soon!","type":"String"},{"key":"1st Half of Q4 2017","value":"Electrum Lite Wallets along with official Android & iOS Apps. Rounding out the Neblio wallet application line up, we plan to launch Electrum-based wallets in Q4. Electrum based wallets, also known as “lite wallets”, provide a variety of benefits over the standard Neblio wallet. Electrum wallets offload much of the normal processing that a wallet must do up to servers that we run in the cloud, making them faster, in many cases more secure, and much lighter on your computer to run. We will also be launching official Android & iOS applications this quarter. Giving you control over your Neblio Coins (NEBL) anywhere and anytime. Neblio coins will be able to be sent and received on virtually every platform!","type":"String"},{"key":"2nd Half of Q4 2017","value":"Staking Wallets for Raspberry Pi and Docker. We will release staking wallets for both the Raspberry Pi and as a Docker image. Either of these unique solutions will give Neblio users the ability to stake their coins on the Neblio network on a low-power and efficient platform to earn stake rewards with their coins without running one of our traditional wallets on a PC fulltime. Learn more about staking and how to stake your coins here.","type":"String"},{"key":"Q1 2018","value":"RESTful APIs for Interacting with the Neblio Network. We believe that the first step in unlocking the potential of  the Neblio Blockchain in the enterprise world is to make the technology easier to consume. Through the use of a set of uniform and open-source RESTful APIs in a variety of languages (Python, Go, JS, Ruby, .NET, Java, Node.js) businesses large and small will rapidly deploy next-gen applications on the Neblio Blockchain Network like never before seen in the blockchain ecosystem.","type":"String"},{"key":"Q2 2018","value":"Marketing Campaign Launch. Enterprise marketing campaign creation to drive the initial adoption of Neblio blockchain technology in the business environment. Continuation of current strategies in addition to enhancing focus towards formation of market relationships for enterprise-wide blockchain solutions.","type":"String"},{"key":"Q3 2018","value":"Enterprise GO TO MARKET AND NEBLIO APIV2 BETA LAUNCH. Identify and target niche areas of the market that can benefit from the simplification of blockchain technology. Examples include Healthcare records management, Supply Chain contract negotiation and validation, and online identity management applications. Based upon user-feedback and enterprise driven customer design requirements we are targeting the beta release of our Neblio API Suite v2. Driving further innovation in blockchain protocol simplification and business adoption.","type":"String"},{"key":"Q4 2018","value":"IMPLEMENTATION AND DELIVERY OF NEBLIO API SUITE V2 FOR ENTERPRISE CUSTOMERS. Focus will be on improving design requirements with our business partners to ensure enterprise customer adoption is seamlessly integrated with current enterprise processes. Iterative-based development work will continue throughout Q4 to ensure customer satisfaction and innovation in improving  business results in the wide variety of markets where our blockchain-based solutions provide inherent business value.","type":"String"},{"key":"2019+","value":"Iterative Innovation & Industry-Wide Adoption. The secure and decentralized exchange of information, credentials, records and tokens of value are all afforded to our users by the Neblio platform. Learning from 2017 and 2018 successes and missteps, we will scale the delivery of our enterprise technology beyond niche markets, integrating into a multitude of world-wide opportunities that finally bring blockchain technology to the mainstream.","type":"String"}]}}})";

    EXPECT_EQ(ZlibDecompress(pd->getRawMetadata()), expected);
    EXPECT_EQ(pd->getInflatedMetadata(), expected);
}

TEST(ntp1_tests, metadata_decompression_transfer)
{
    // txid: ff7510b3d8deb69d15683c86eb7b1adecfd94e69b21d697c40e8bcb171583064
    std::string transfer_opRet =
        "4e54031001000100000e61789c7559db8ee5b611fc15625e36018e06481e9d27df7700db09b0468c201b04944449dca"
        "14899a4e6f8d8f05fe50ff263a9eaa674661708b097195dc8be54575753bf3decc5e5af6cb50f9ffdf6b03afeffcfdf"
        "1ebe5cec565dfed3bfff6673fdf3c3670f4fd55c6d31d6acb6e286499319161b0767ea62ab793265497b18cd625f9cc"
        "92e5637e2615c2bcef86852747ca52ece949a6d9c5da96648ebba475fbd2b7ce68794eb623e5f5df6837d346dc71475"
        "87125c1cb171f6a96255e34bb07134d7c50f8b71bf54dcc5c55a5c98ccb83be32c76c0963fb8abf947cacf469f76991"
        "6f35f9bddc5d835c5d9245e31d1d63ddb60861d7b1431eb62ea35993dee65c78d296578ef532c5c98db3f9a1faff0f5"
        "66561fe0c494d32a3e0e1ed7acd9ac9748b9883769b49b67ace947bc031f03bd1e52843f59cc2b6eb3d9327429869be"
        "9b9c6809bd515fc666f17f3618753f8ebf192ecb426b839a615f1c48a7cb54fe38d9b161b184126cb4779f6273ce472"
        "346fddeacbc6205ce4fa9c1d027c75157be478b379e4fbdf31324f1ae577691767177763dc4c4cd56c2e4f6e80392f3"
        "694ae0bfe59224b1f8f0dbf4c615f7bf85d6aca375ed2d77b04dc0c792f0bec9d0236c71f891b8261b12692d975fd2e"
        "1711c26db915895776c5ad7d10dcad3b3cef91475310a241f0059b3657992bac34edc5133d1aa8790fa1289426c436b"
        "db8bc384ba7f4fed5c719391488278147664c1103442a22c0116bf9d22c1a7d291e49b7999986bb0eebdd90f08c34ec"
        "b80c480e6e036c51484e93eb7f758f069512fc0b8ba34a3eccd7f32c59e8baab0b411322764cb62cb0dff6e12c1d809"
        "10fa47d5ef09f671188b958a6eccc871f3c7caf76a6d7ee972d7321bed9fb5fe99018c2e45958512b562e3e7aa24242"
        "9fad84b45e9d9304ae8fe6fb5b2b62a97ecd92b85afd7698e5e8822076f213c24104bd2a0601cf4563f0f3eedcaf04e"
        "9b109ca6bd96767b66007d7f2d3e803e586fb2e8050f0139756b3600ed742de51e3290a2c9560a2596f20887939c86a"
        "4821151482b1d3c45a6445c51be088056064d7f9e3c909b0236e90d42a352e2584b49bb7fffd4f75c18ccefcdd87e01"
        "a53ad5801f578f540b245b4afe444e5b982fabe98b2d9f84cf84410d02ed46599b408df5b85657b35fee5a6a121b366"
        "e6ba5cfdba0aee520a724bf088c8448684c43220af4a41d7284fcc58d1c59333bfb5b5f4b737054b46d6c0a3f92b88d"
        "392e5b0590106473fc637d53cc77435dfe7c7f686f177969717e1ec627b2fb442329a9199e07053538f5c45bb3a4149"
        "8229af90828ab8b9ae24720c0bf95cb9ac36845737693e6e2e96a0407e599821a56707c894045379c79a178f3036bc0"
        "9a95d845f516e48da7113d98f0ee9ef5386f38c8eaedfa8a5a42071cde917a6595812351c58651e21ed3a9a46d87111"
        "dc1f5340296b8dc5baa07a3f1f726a25f59a98d5ac0581824d8a64acfdba82bfb65aeb6666e1817ae15420c3be76a89"
        "98aca16be3c1ae6beae6c7c0ed6613737fbd8daa223ed44fa848ce6c44241f45a7f030348331e7d8cb82848e52b3fa2"
        "2cbfd8d9bc91e347f395f5f081b941f48a439846ba56a4690fece56b12bec2f5a737a34026ca22d2bc50113300f039e"
        "e7e2021dba9aa05583253166c808c94f9686fe5b442defe12bddbce09617d8b7796bdf45688427af20ba8952df3e47e"
        "3b402f6cc197055c5cd146ef90b98b0be1c28dd538edc188261098929ec93137e195547b661ad7a90ede224a11b9a70"
        "060ba00013fef592addc28ddbe5be4302beb13d729d9810488f825012ee76d891fd4060610fb23f42c3009234452474"
        "5ce5b4e6462e98355e571242b12f290b6e2cd441376029fbcba379ebd90a00532489693db484fc8e16bfdcba8e28789"
        "50e0208f4eb1c7481461c1175b78f051c619e1d6a014e68af25172c0e290e6eaa477224ef037950049584a4e1ba91b5"
        "d00ad0969ecd8d42a6a79a404810b8cf6417c0b54ac796c5fbccf645cd42eea21a6418d8bbc1d5f827fa43487d67a12"
        "8be496cc327b32d8c5495d807f4533541e88aa6ad4a4360299795c4f94e0b1323278d13504cf22280f7d3d264c9600f"
        "17514ae9e0c62679883405b23537b037f7fa26d3a78b7818d3ebce9fa52d9de51c519aec5dd0c6ae75602dd03dd235e"
        "094a9e415c1a84b5b7007542526225cf942a6d4ad69767c9fa621cd9a53f47e848006b258419cd68fadb20f46306862"
        "6e5b000425e4b307f42e786c0d12ee3a655bb853a4898aca946540a7504bb9b6572717aac69e447015e52f5e9297101"
        "171a538270df0d41a577fb82bad1d368dd952510f06cabbdfb55e8ebeeba157065a2ffc7916ed2c0d470807ed413ac7"
        "86d4b851ed495ab399c04787fe3f0499ee7c8c9f61a83054821753f6c21a28f095f43558484b58fccc364eac8520a08"
        "098b8773ba93fe9d22e58341fd0b9f6eb278a3088643633a891c5292fe5c6a7da31be7529cf1e4f432e03fee09fa3fb"
        "1ebdd0b73e814ea36a471a3faa2a5755927253e888cb02995424522ab881628aacb7a4456fcdcf3b5f94c9c4cae4c2b"
        "962dda40e25717bec46d85164b51eb07b869be1b9fdbe6789a86a0e34cdc947db301c9910e145d75a99c448a0c177f6"
        "4d3b1a14126ba857adf6e2236ade9643da9eca71a5ecaefb4a4b31da99bc37b5a72e31f4a093676c2bea04dd5de0f30"
        "d0424424100a42b1ac51cd2559418084ee830bb294856cc8c9c6ba55e6953dadc3930bc029190744c4751bfeea07423"
        "fb91cb0f8173a496a4284c5e94fd58a82066f8489a387ab73ab1a53c2cd202a5f6b8eaec382d0863e2c5b34509f5a06"
        "3ff203c7dc829540e8528a76a5ca68e19cd29d03ce562d3a88da88541d140ead264a74c0e830fd26df166146e413980"
        "e499e80b47a134a32951b515b112bc85aaf432bfa1c9000052b9048ff41e3bb8a64b5fe8846693756ab36d356ec3958"
        "220382b68b2f3cc6105a391d203d14b57ab1697689d6972dcaa5223435c7315eef649f8073d87603ae5ad63f6de54a3"
        "37ddca29b9eb8845e756273d6582146c2d1e220d439118d683780ab8bafa60dae9858f0ddd356d28c541ec87b7687f6"
        "d77328a6d73f566073115f3ea4079bfb45a9072a1fd7a7202db5468e9acb0c8a8293ddddedb9fd5e97e008eda5878ba"
        "17c8b97676143218ad31b1f051028b5b20f2c2222fc9b369d1b67d9aa8b99ed100b9082e8ea30682aef95586c736084"
        "d19e31101123950aa9a7e011af44c201f60ac896288bd9c8236ea185eddba8970f0f5d292a9dcd45a1de9cc43eec3f0"
        "b3354abba3ce7a2dd14474117d8b9c733042f35e5110e6fd036ae2d2fa36a3f62c52008c26f60b7c28dc44f9c8d42c9"
        "c7579ff603e4240a1d67bfff0414f16064bee7a7ab332e990a42edfa731a54e1b95e699789bdd23d6fba919df4e400a"
        "65c53b2009c4fb2e0dded536ee81fad911dbd3511a26da2d341fe0fd745407bdfd241b825eada62c7821b4b4da50140"
        "ccc2ada5fce63587a52fed25151fa654198dce459cdad1d4b4a5b29413d51146101d07d1bc1a1f79001141ef4ed3977"
        "a045608e3eb8ebfdc3d31b14fa2c070b1108d3715e04cf116428928bec212432056a48d64dcfe32c0a211e7a0402f6c"
        "7d660e855e6d901d99dda99842c5d41e61152bf6dc7478b50d9394f29b3bec03f7b415487b00b430806510c57e73621"
        "bec8d6f1045fd97db59fb136161b2619b21996910f6f7be4e86b30fcb972f066dcfb2ee2022181790df5669b28ead14"
        "d0f8a90b68252439567a7b112cd9584e6a9af304ffba86495bc347f0209fd00d41e359f2d0e76c69317aa42e8867053"
        "43fabc6f95817bfff0937b43859428b9b12f9711406a32eb22d29b0ea2edd24b824c459e3d7a313cbc753c36611f29e"
        "02e4a5cf00ad0cae153cc380f1fb5bb233b1f775c56adf3d26d5c3b363c6f09dced07abb53483fe5725432a21f1afd4"
        "96435716d094253a51e6cae73c083ecb75cee884777579f8725a86b194d388c39c00b6bc9e01e00339a5f5a29707ccf"
        "1dc57869b2a487351055ad223c876402c2585f9de1174b31c0e7b3d22948996e2e623d555588d57376a7be234d3a6d4"
        "c1799e449c8c4709b1810bc7bb5a47bcdc9987bca30bafb6a9e9025680e349c416e71b8416dcf7aa389d6519c9a9189"
        "22663b313c0143ddbb23c5ce83ff0f4b6f114c3711edc1c2d65503a8fed909d12194cc76c25998199ca7e4f1c54e0fb"
        "5dbfed1b5538ff0c8bd84fe19eda495d1b63e508d8c723f11a071eac92e75c39061e44e5e8ae53d849dff22b7235e9d"
        "c46866c7c4d191838fff5ecb84ca69c4558293d9efa0ac05b29bd02c9931e25cb6149c11cfb11ed890245773df5c0a1"
        "0e17bfe99166b4e741e889a4e324070855298611cd0f95870a2d47d763908847cb6bed1d79c04f1f49ccb2341866ce9"
        "e67911d075b56e6694581770a5aa5d533b3e7b39f1a0864dd7ff934c12d6d2402c415f069be4fb89a1bc2e4f94357e9"
        "12873095cf3c326c71627bd724ab7eaa616fc12408d512672a51b634618b63a1d1bf5850dd2bec8264015d3da217bd4"
        "e61cd836baa2488b2224da1918122a42cadbfc2138091e96323d49317ad1f85360dfb79f755a5887a3a49869e749502"
        "710d1e66508f5ca71cd56fb19c4d8d0f8ae2c5008d2daa8e2d5d87cc384e804f1a9ad00eecf396c56821ad75cff8a35"
        "8476f47e1cf3a260396bc3a7a2afd5e4f1148b9435ab5a71da52e1435fb1c2e3ab293314627cb55d5603c37c18e5d57"
        "e41b83d5ccb22aee9afb9816785c0ab91245f51d9f1210ffae13be9215083c996cb1450f7ff023109bc5ae960779461"
        "5cfd3f102cf8af45889e2e8d8f7a46e02b6698ad56c1dbf88851bbf1968c6a17b382e16696eefee661ccdf19e793904"
        "b9e75bbe5a608d43812d0e427dbda976d08ad76abf9863e606ab69726e32d25c7874bcfaa28caf6377ab5c649c7da3b"
        "1e935650e01b4819915808934d3c306d6bd3d043ddb169f5cf82dea9d9cfecaa372d2a6a83837236878e07e60f00e6b"
        "e65d16fc026d06b5f90711648b7c6900ca2514b2ca719ad356d62fac0af8951daa8972a2e3e86670f22fcc310f64306"
        "5503a0ed9573ff8b2eaa20234cf1344fd7475a0e0f18f0fbfffebf7dfff07606de34b";

    std::shared_ptr<NTP1Script>          p  = NTP1Script::ParseScript(transfer_opRet);
    std::shared_ptr<NTP1Script_Transfer> pd = std::dynamic_pointer_cast<NTP1Script_Transfer>(p);
    ASSERT_NE(pd, nullptr);

    std::string expected =
        R"({"userData":{"meta":[{"Chapter1_Part2":"It was a matter of chance that I should have rented a house in one of the strangest communities in North America. It was on that slender riotous island which extends itself due east of New York and where there are, among other natural curiosities, two unusual formations of land. Twenty miles from the city a pair of enormous eggs, identical in contour and separated only by a courtesy bay, jut out into the most domesticated body of salt water in the Western Hemisphere, the great wet barnyard of Long Island Sound. They are not perfect ovals--like the egg in the Columbus story they are both crushed flat at the contact end--but their physical resemblance must be a source of perpetual confusion to the gulls that fly overhead. To the wingless a more arresting phenomenon is their dissimilarity in every particular except shape and size.  I lived at West Egg, the--well, the less fashionable of the two, though this is a most superficial tag to express the bizarre and not a little sinister contrast between them. My house was at the very tip of the egg, only fifty yards from the Sound, and squeezed between two huge places that rented for twelve or fifteen thousand a season. The one on my right was a colossal affair by any standard--it was a factual imitation of some Hôtel de Ville in Normandy, with a tower on one side, spanking new under a thin beard of raw ivy, and a marble swimming pool and more than forty acres of lawn and garden. It was Gatsby's mansion. Or rather, as I didn't know Mr. Gatsby it was a mansion inhabited by a gentleman of that name. My own house was an eye-sore, but it was a small eye-sore, and it had been overlooked, so I had a view of the water, a partial view of my neighbor's lawn, and the consoling proximity of millionaires--all for eighty dollars a month.  Across the courtesy bay the white palaces of fashionable East Egg glittered along the water, and the history of the summer really begins on the evening I drove over there to have dinner with the Tom Buchanans. Daisy was my second cousin once removed and I'd known Tom in college. And just after the war I spent two days with them in Chicago.  Her husband, among various physical accomplishments, had been one of the most powerful ends that ever played football at New Haven--a national figure in a way, one of those men who reach such an acute limited excellence at twenty-one that everything afterward savors of anti-climax. His family were enormously wealthy--even in college his freedom with money was a matter for reproach--but now he'd left Chicago and come east in a fashion that rather took your breath away: for instance he'd brought down a string of polo ponies from Lake Forest. It was hard to realize that a man in my own generation was wealthy enough to do that.  Why they came east I don't know. They had spent a year in France, for no particular reason, and then drifted here and there unrestfully wherever people played polo and were rich together. This was a permanent move, said Daisy over the telephone, but I didn't believe it--I had no sight into Daisy's heart but I felt that Tom would drift on forever seeking a little wistfully for the dramatic turbulence of some irrecoverable football game.  And so it happened that on a warm windy evening I drove over to East Egg to see two old friends whom I scarcely knew at all. Their house was even more elaborate than I expected, a cheerful red and white Georgian Colonial mansion overlooking the bay. The lawn started at the beach and ran toward the front door for a quarter of a mile, jumping over sun-dials and brick walks and burning gardens--finally when it reached the house drifting up the side in bright vines as though from the momentum of its run. The front was broken by a line of French windows, glowing now with reflected gold, and wide open to the warm windy afternoon, and Tom Buchanan in riding clothes was standing with his legs apart on the front porch.  He had changed since his New Haven years. Now he was a sturdy, straw haired man of thirty with a rather hard mouth and a supercilious manner. Two shining, arrogant eyes had established dominance over his face and gave him the appearance of always leaning aggressively forward. Not even the effeminate swank of his riding clothes could hide the enormous power of that body--he seemed to fill those glistening boots until he strained the top lacing and you could see a great pack of muscle shifting when his shoulder moved under his thin coat. It was a body capable of enormous leverage--a cruel body.  His speaking voice, a gruff husky tenor, added to the impression of fractiousness he conveyed. There was a touch of paternal contempt in it, even toward people he liked--and there were men at New Haven who had hated his guts.  \"Now, don't think my opinion on these matters is final,\" he seemed to say, \"just because I'm stronger and more of a man than you are.\" We were in the same Senior Society, and while we were never intimate I always had the impression that he approved of me and wanted me to like him with some harsh, defiant wistfulness of his own.  We talked for a few minutes on the sunny porch.  \"I've got a nice place here,\" he said, his eyes flashing about restlessly.  Turning me around by one arm he moved a broad flat hand along the front vista, including in its sweep a sunken Italian garden, a half acre of deep pungent roses and a snub-nosed motor boat that bumped the tide off shore.  \"It belonged to Demaine the oil man.\" He turned me around again, politely and abruptly. \"We'll go inside.\"  We walked through a high hallway into a bright rosy-colored space, fragilely bound into the house by French windows at either end. The windows were ajar and gleaming white against the fresh grass outside that seemed to grow a little way into the house. A breeze blew through the room, blew curtains in at one end and out the other like pale flags, twisting them up toward the frosted wedding cake of the ceiling--and then rippled over the wine-colored rug, making a shadow on it as wind does on the sea.  The only completely stationary object in the room was an enormous couch on which two young women were buoyed up as though upon an anchored balloon. They were both in white and their dresses were rippling and fluttering as if they had just been blown back in after a short flight around the house. I must have stood for a few moments listening to the whip and snap of the curtains and the groan of a picture on the wall. Then there was a boom as Tom Buchanan shut the rear windows and the caught wind died out about the room and the curtains and the rugs and the two young women ballooned slowly to the floor.  The younger of the two was a stranger to me. She was extended full length at her end of the divan, completely motionless and with her chin raised a little as if she were balancing something on it which was quite likely to fall. If she saw me out of the corner of her eyes she gave no hint of it--indeed, I was almost surprised into murmuring an apology for having disturbed her by coming in.  The other girl, Daisy, made an attempt to rise--she leaned slightly forward with a conscientious expression--then she laughed, an absurd, charming little laugh, and I laughed too and came forward into the room.  \"I'm p-paralyzed with happiness.\"  She laughed again, as if she said something very witty, and held my hand for a moment, looking up into my face, promising that there was no one in the world she so much wanted to see. That was a way she had. She hinted in a murmur that the surname of the balancing girl was Baker. (I've heard it said that Daisy's murmur was only to make people lean toward her; an irrelevant criticism that made it no less charming.)" +
        std::string(")\"}]}}");

    EXPECT_EQ(ZlibDecompress(pd->getRawMetadata()), expected);
    EXPECT_EQ(pd->getInflatedMetadata(), expected);
}

TEST(ntp1_tests, random_compressions)
{
    for (int i = 0; i < 10; i++) {
        // generate random data, compress, decompress and verify it's the same
        std::string strHex        = GeneratePseudoRandomHex(2000000);
        std::string str           = boost::algorithm::unhex(strHex);
        std::string compressedStr = ZlibCompress(str);
        std::string restoredStr   = ZlibDecompress(compressedStr);
        EXPECT_EQ(restoredStr, str);
    }
}

TEST(ntp1_tests, ntp1v1_metadata_map)
{
    json_spirit::Value r;
    EXPECT_NO_THROW(r = GetNTP1v1IssuanceMetadataNode("La6h77fYdhWAAEgRqM1BJBwwnjc1XTSaPdhfxo"));
    EXPECT_ANY_THROW(GetNTP1v1IssuanceMetadataNode("La6h77fYdhWAAEgRqM1BJBwwnjc1XTSaPdhfxoxxxx"));
}

TEST(ntp1_tests, ntp1_metadata_parsing)
{
    NTP1TokenMetaData d1;
    d1.readSomeDataFromStandardJsonFormat(
        GetNTP1v1IssuanceMetadataNode("La4oBu5JpYfJhMYDgDV56NeHA9nGgt6jx3xFjH"));
    d1.setTokenId("La4oBu5JpYfJhMYDgDV56NeHA9nGgt6jx3xFjH");
    d1.setIssuanceTxId(156);

    EXPECT_EQ(d1.getTokenName(), "NUB");
    EXPECT_EQ(d1.getTokenDescription(), "Nubbles");
    EXPECT_EQ(d1.getTokenIssuer(), "Nubbles");
    EXPECT_EQ(
        d1.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/be7e0f36a8ee11beecfcbccec7c784411910d3cb.png");
    EXPECT_EQ(d1.getIssuanceTxId(), 156);
    EXPECT_EQ(d1.getIconImageType(), "image/png");
}

TEST(ntp1_tests, ntp1_metadata_parsing_1)
{
    const std::string tx_hex =
        "01000000AE37105C02C0ED46778E0CFD1BEEF47AE7B14CC6F34FAA73BC48B4516F7CD744FB0C272D8E000000006A473"
        "044022059ECCAFA41B1EEE11F6B6DFF61AF0673156C08533F51AA2FB5DA5B7E77D3C15B02201A0A8168B1EFF400A7D5"
        "69F6E49AD040FA39A68126050B6D688A07539BAD790C01210263B966632FB59B27023A1D08042E4109C6A467BDEA212"
        "151CB0A7C8BB5553747FFFFFFFFC0ED46778E0CFD1BEEF47AE7B14CC6F34FAA73BC48B4516F7CD744FB0C272D8E0200"
        "00006B483045022100D65293B354F32010421D47BA766E0962584F16823F0CD2FB652FBED6EBDCA01C02202747B696A"
        "EBC7880B16D142A0251494C05D55EC5AB27BB725B5666ECC89D9FBC01210263B966632FB59B27023A1D08042E4109C6"
        "A467BDEA212151CB0A7C8BB5553747FFFFFFFF0310270000000000001976A9143A8B4853887B846BE7355F3D8D75957"
        "3AEFF306288AC10270000000000006B6A4C684E5403015433362020201601002016F000000054789CAB564A492C4954"
        "B2AA562AC9CF4ECDF34BCC4D55B2520A313653D2514A492D4E2ECA2C28C9CCCF838B65161797A61601B97EA9493999F"
        "921A989B940D1D2E2D42217A839B9A9203A3AB6B6B6160084931E9A50E63377000000001976A9143A8B4853887B846B"
        "E7355F3D8D759573AEFF306288AC00000000";

    const std::string ntp1tx_hex =
        "01000000C8149A326E0100004CF4BB07079B6587FA886FB90C2EFCDF5EAAECE9086FD46416058E388B89221102C0ED4"
        "6778E0CFD1BEEF47AE7B14CC6F34FAA73BC48B4516F7CD744FB0C272D8E00000000D434373330343430323230353945"
        "43434146413431423145454531314636423644464636314146303637333135364330383533334635314141324642354"
        "44135423745373744334331354230323230314130413831363842314546463430304137443536394636453439414430"
        "34304641333941363831323630353042364436383841303735333942414437393043303132313032363342393636363"
        "33246423539423237303233413144303830343245343130394336413436374244454132313231353143423041374338"
        "424235353533373437000000000000000000C0ED46778E0CFD1BEEF47AE7B14CC6F34FAA73BC48B4516F7CD744FB0C2"
        "72D8E02000000D634383330343530323231303044363532393342333534463332303130343231443437424137363645"
        "30393632353834463136383233463043443246423635324642454436454244434130314330323230323734374236393"
        "64145424337383830423136443134324130323531343934433035443535454335414232374242373235423536363645"
        "43433839443946424330313231303236334239363636333246423539423237303233413144303830343245343130394"
        "33641343637424445413231323135314342304137433842423535353337343700000000000000000003102700000000"
        "00003237364139313433413842343835333838374238343642453733353546334438443735393537334145464633303"
        "63238384143554F505F445550204F505F48415348313630203361386234383533383837623834366265373335356633"
        "6438643735393537336165666633303632204F505F455155414C564552494659204F505F434845434B5349470022544"
        "64A6D3368706473554D796E3666656F44586F704E6573373151394E5A715942471027000000000000D6364134433638"
        "34453534303330313534333333363230323032303136303130303230313646303030303030303534373839434142353"
        "63441343932433439353442324141353632414339434634454344463334424343344435354232353230413331333635"
        "33443235313441343932443445324543413243323843394343434638333842363531363137393741363136303142393"
        "74541393439333939394639323141393839423934304431443245324434323231374138333942394139323033413341"
        "423642364236313630303834393331453941DA4F505F52455455524E203465353430333031353433333336323032303"
        "23031363031303032303136663030303030303035343738396361623536346134393263343935346232616135363261"
        "63396366346563646633346263633464353562323532306133313336353364323531346134393264346532656361326"
        "33238633963636366383338623635313631373937613631363031623937656139343933393939663932316139383962"
        "39343064316432653264343232313761383339623961393230336133616236623662363136303038343933316539610"
        "00050E63377000000003237364139313433413842343835333838374238343642453733353546334438443735393537"
        "33414546463330363238384143554F505F445550204F505F48415348313630203361386234383533383837623834366"
        "2653733353566336438643735393537336165666633303632204F505F455155414C564552494659204F505F43484543"
        "4B534947002254464A6D3368706473554D796E3666656F44586F704E6573373151394E5A71594247000000000000000"
        "001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "T36");
    EXPECT_EQ(metadataObj.getTokenDescription(), "T36");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "NeblioTeam");
    EXPECT_EQ(metadataObj.getIconURL(), "");
    EXPECT_EQ(metadataObj.getIconImageType(), "");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("1000000"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La5hsvMU77nh1M7y7uH9rrrfqd85sTNXLohjMd");
}

TEST(ntp1_tests, ntp1_metadata_parsing_2)
{
    const std::string tx_hex =
        "010000008A7BF75A01C369A3C208A2F8CE953C75588F6AB52FD4A6E27B0E9AE0A161273FB1E6155504010000006A473"
        "04402202C1D6A2E05A4615F0C50F3CE1E68EA7BB1060657BD633CA1EA357514B8661D3A022032368CADAB3848F12928"
        "BCE31BAAA75A048E8E12459EFBC5020541937D0B47CF01210269AB6AECB0C341C84B69A51B6FD91E1C41E641221D5A7"
        "24125BC22FE9B4CADBCFFFFFFFF0310270000000000001976A914C3B3289221D299C5665CE9ED79995B3937025CA688"
        "AC1027000000000000456A434E5401014D5432202007A07EA4A45205C1E4B5194E12E6A01C7B3D7ABC9F814F2DBF418"
        "1D7EC0BDBE0B0927AF21DC07711A5ABAC72951ABDD016315C602018002018F0D0A47018020000001976A914C3B32892"
        "21D299C5665CE9ED79995B3937025CA688AC00000000";

    const std::string ntp1tx_hex =
        "01000000C0B09C326E010000543D77C30360B4961971F76B3BA0D5243B06EA78565BBC34DF2F41953891DA1101C369A"
        "3C208A2F8CE953C75588F6AB52FD4A6E27B0E9AE0A161273FB1E615550401000000D434373330343430323230324331"
        "44364132453035413436313546304335304633434531453638454137424231303630363537424436333343413145413"
        "33537353134423836363144334130323230333233363843414441423338343846313239323842434533314241414137"
        "35413034384538453132343539454642433530323035343139333744304234374346303132313032363941423641454"
        "34230433334314338344236394135314236464439314531433431453634313232314435413732343132354243323246"
        "45394234434144424300000000000000000003102700000000000032373641393134433342333238393232314432393"
        "9433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F484153483136"
        "302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505F455"
        "155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D62596850"
        "7735794D69397248413310270000000000008A364134333445353430313031344435343332323032303037413037454"
        "13441343532303543314534423531393445313245364130314337423344374142433946383134463244424634313831"
        "44374543304244424530423039323741463231444330373731314135414241433732393531414244443031363331354"
        "33630323031383030323031384630904F505F52455455524E2034653534303130313464353433323230323030376130"
        "37656134613435323035633165346235313934653132653661303163376233643761626339663831346632646266343"
        "13831643765633062646265306230393237616632316463303737313161356162616337323935316162646430313633"
        "31356336303230313830303230313866300000D0A470180200000032373641393134433342333238393232314432393"
        "9433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F484153483136"
        "302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505F455"
        "155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D62596850"
        "7735794D693972484133000000000000000001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "MT2");
    EXPECT_EQ(metadataObj.getTokenDescription(), "MathToken2");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "Sam");
    EXPECT_EQ(
        metadataObj.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/50aa59a0cecaed91b9e1fce937f14f363b2d222b.png");
    EXPECT_EQ(metadataObj.getIconImageType(), "image/png");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("100000000"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La3NzpGLMZz7QQXx3ExHrrnGgPaXEAQc2wBEjz");
}

TEST(ntp1_tests, ntp1_metadata_parsing_3)
{
    const std::string tx_hex =
        "01000000175B105C01F146A39A4301C6A7F76AC3A16AEC76747C48922F4EC79C9E234A97401CBDC63B030000006B483"
        "045022100AA0A5EAD2F96809EA1479B6C4D8E70A72CACE3A93B599DC47A31828831F243A502206CDA8E0B3F840BA0A7"
        "B24C3B136E382C3190F1F5C1FB51220E67ADA798D5366001210263B966632FB59B27023A1D08042E4109C6A467BDEA2"
        "12151CB0A7C8BB5553747FFFFFFFF0310270000000000001976A9143A8B4853887B846BE7355F3D8D759573AEFF3062"
        "88AC1027000000000000FD8E046A4D8A044E540301564552332060F423E0010060F423E0F000000472789CED59CB8ED"
        "B4610FC9531CF5AED7BBD904F061C047BB18364611F0C1F8664531C6B1ECCF4500CBDD87F4FF50CB502F20539F044CD"
        "ABBBBABABAFBA097AAD54957BB972A8503F9CFDA51B5ABBEFEF6E76DB5A95AE2269A2199E0B1F9F9F98F6BF59522632"
        "9A78679A42807545B139E493BEC8ED172B5FBFE52F962CA34789CB7B1E8531A787779E9D3707D2127BCD58E6FB7ADD9"
        "9BA46D68487B1E7443BC6D82BBBCD2573777F7DDCD63FD48FAF1FEFDB57EBC7EB8A7DBF76D73FD4057570F77370FCD1"
        "DD5DBC1EFE1C31947CFF390BD3ABDA74BD97EFD01E74CF1D312A623F902DF81665CFC4631F58862531DB51DE5E94735"
        "05DF52EC46ABF08EBC49B3EA35ABA441901A0233B170A042A7DCACC82713497118ED4659732005838C8D89282917A23"
        "77ECF729987889F6AEA4DD3AB273CFC19663599D48B99A90F96544F3AA62D0EB553E0C3D34669DFAA8EC88A59D5F43A"
        "3AB145FF184EE41B52C6E3C430AC87B4598C4F80DB44D289F036C4FC1439E2020348B92075C6D3E28D03821C8679236"
        "05AC0505D34E4DB8D9CE89A43AC612C3B2378FF7B346C12A2248F58850950A552D41E27F60C6F83FB3AC185A7BDA526"
        "8975641AA4B138E61E605A554B188D1E746DB3B136EA49A8D28AF1C11EA708792A5812F74304C15EC875F87CC814CD2"
        "45E16A2168F478A990AADF6998CA8C02E80C90DAF7C98B6EA5B4F3EB36673DE940D47B2B382182C012991E392A2A31E"
        "C22806C2086F6E498C3C41E4A63530C8A317A02096F3C1380C70C963ECA068094B368D1BC813889250BB600D84BA282"
        "945222E76EB310175479318D4B3020728142C485B509542B1E5BD38D0BE49A38E48DD13B66398608CC982C63079E436"
        "8049B90EDE2D98D090413D979D689A8315A6E1061E329540C0B0640DE46603B2BB781369F638C4228C2C28477FF0E26"
        "2B01AF904370452936920143870B403B12899A72CEB22C2F1D7AF1313D6A4040EA610A180334A06CCC342C35E82E9B4"
        "33A029963CE42A400EF01471185F7A54BDD0191DBF110D6136824AEA07C1B04831A379ABA6A2A3736E3E5A67F67D9A4"
        "510215B4324A378515261126A6E2CE7DCD7A2ABBEBC87E8466FA039468E44474B31D608BEC0E011C101176C66924D02"
        "B2A033755956C517A444106B6E3CB09C0BF7C3C9A5CF057AAACDCC6FABE3C10B1B419C2328DD72EE4CF3494EE0FF483"
        "EFFCC5944D99293BCB6135911945C97BE90AF946A073A46B79B289EDB5AE6443069442521A2128C4887CFEC860E852F"
        "2DC91F4AC6D0C6F690D8467D817AA65CEF4F92427C97E49D5A2606424379DCC05CB9817211EBA82534DC414B41898A3"
        "3DB46BAD831AB17D91A6D413F498714C712D5A9FF80E99C5A557C611123FA62A9BBD2BA35BF9160F83F978A9E3AE3A5"
        "E5FD1EDA77EACB390BEAE222D7ABC9885208CA8D48BBF45D310876FC1E34E0D693B4B3035482F1926D4E94412D1E784"
        "05F6CA5CB940D3072343269F89D5AA7D23A95D6A9B44EA5752AAD53699D4AEB545AA7D23A95D6A9B44EA5752AAD5369"
        "9D4AEB545AA7D23A95D6A9B44EA5FFED54DA569B2A953FE7FE4A82B77AFDF1FAFAFA2F55F12BCE70FECEB2000000001"
        "976A9143A8B4853887B846BE7355F3D8D759573AEFF306288AC00000000";

    const std::string ntp1tx_hex =
        "0100000050169D326E0100006CB71B65682DABAF1602ECC6D6C271C2D8F0A2706B2AB1323E08958CE65A0E9E01F146A"
        "39A4301C6A7F76AC3A16AEC76747C48922F4EC79C9E234A97401CBDC63B03000000D634383330343530323231303041"
        "41304135454144324639363830394541313437394236433444384537304137324341434533413933423539394443343"
        "74133313832383833314632343341353032323036434441384530423346383430424130413742323443334231333645"
        "33383243333139304631463543314642353132323045363741444137393844353336363030313231303236334239363"
        "63633324642353942323730323341314430383034324534313039433641343637424445413231323135314342304137"
        "43384242353535333734370000000000000000000310270000000000003237364139313433413842343835333838374"
        "23834364245373335354633443844373539353733414546463330363238384143554F505F445550204F505F48415348"
        "3136302033613862343835333838376238343662653733353566336438643735393537336165666633303632204F505"
        "F455155414C564552494659204F505F434845434B534947002254464A6D3368706473554D796E3666656F44586F704E"
        "6573373151394E5A715942471027000000000000FD1C093641344438413034344535343033303135363435353233333"
        "23036304634323345303031303036304634323345304630303030303034373237383943454435394342384544423436"
        "31304643393533314346354145443742424439303446303631433034374242313833363436313146304331463836363"
        "43533314336423145434346343530304342444438374634464635304342353032463230353339463034344344414242"
        "42424142414241464241303937414144353439353742423937324138353033463943464441353142354142424546454"
        "63645373644423541393541453232363941323139394530423146394639463938463642463539353232363332394137"
        "38363739413432383037353435423133394534393342454338454431373242354642464535324639363243413334373"
        "83943423742314538353331413738373737394539443337303744323132374243443538453646423741444439394241"
        "34364436383438374231453734343342433644383242424243443235373337373746374444434436334644343846414"
        "63146454644423537454243374542384137444246373644373346443430353735373046373733373046434431444435"
        "44424331454645314333313934374346463339304244334142444137344244393745464430314537344346314433313"
        "24136323346393032444638313636354346433436333146353838363235333144423531444535453934373335303544"
        "46353245433436414246303845424334394233454133354142413434313930314130323333423137304130343241374"
        "44341434338323731333439373131384544343635393733323030353833384338443839323832393137413233373745"
        "43463732393938373838394636414541344444334142323733434643313936363335393944343842393941393046393"
        "63534344633414136324430454235353345304333443334363639444641413845433838413539443546343341334142"
        "31343546463138344545343142353243364533433433304143383742343539384334463830444234344432383946303"
        "33643344643313433394532303230333438423932303735433644334532384430333832314338363739323336303541"
        "43303530354433344534444238443943453839413433414336313243334232333738464637423334364331324132323"
        "43846353838353039353041353532443431453237463630433646383346423341433138354137424441353236383937"
        "35363431414134423133384536314536303541353534423138384431453734364442334231333645413439413844323"
        "84146314331314541373038373932413538313246373433303443313545433837354638374343383134434432343545"
        "31364132313638463437384139393041414446363939384341384330324538304339304441463743393842364541354"
        "23446334542333636373344453934304434374232423338323138324330313239393145333932413241333145433232"
        "38303643323038364636453439384333433431453441363335333043384133313741303230393646334331333830433"
        "73043393633454341303638303934423336384431424338313338383932353042423630304438344241323832393435"
        "32323245373645423331303137353437393331384434423330323037323831343243343835423530393534324231453"
        "54244333844304245343941333845343844443133423636333938363038434339383243363330373945343336383034"
        "39423930454445324439384430393034313344393739443638394138333135413645313036314533323935343043304"
        "23036343044453436363033423242423738313336394636333843343232384332433238343737464630453236324230"
        "31414639303433373034353239333639323031343338373042343033423132383939413732434542323243324631443"
        "74146313331334436413430343045413631304131383033333441303643434333343243333545383245394234333341"
        "30323939363343453432413430304546303134373131383546374135344244443031393144424631313044363133363"
        "83234414541303743314230343833314133373941424136413241333733364533453541363746363744394134353130"
        "32313542343332344133373835313532363131323641364532434537444344374132414242454243383745383436364"
        "64130333934363845343434373442333144363038424543304530313143313031313736433636393234443032423241"
        "30333337353539353643353137413434343130364236453343423039433042463743334339413543463035374141414"
        "34443433646414245334331304231423431394332333238444437324545344346333439344545304646343833454646"
        "43433539343444393932393342434236313335393131393435433937424539304146393436413037334134364237394"
        "23238394544423541453634343330363934343235323141323132384334383837434645433836304538353246324443"
        "39314634414336443043364636393044383436374438313741413635434546344639323432374339374534394435413"
        "23630363432343337394443433035434239383137323131454241383235333444433431344234313839384133334442"
        "34364241443833314142313744393141364434313346343938373134433731324435413946463830453939433541353"
        "53743363131313233464136324139424244324241333542463931363046383346393738413945334145334135453546"
        "44314544413737454143423339304245414532323244374142433938383532303843413844343842424634354433313"
        "03837364643314533344530443639334234423330333534383246313932364434453934343132443145373834303546"
        "36434135434239343044333037323334333236394638394435414137443233413935443641394234344541353735324"
        "14144353336393944344145423534354141374432334139354436413942343445413537353241414435333639394434"
        "41454235343541413744323341393544364139423434454135464645443534444135363942324139353346453746453"
        "4413832423737414644463146414641464132463535463132424345FD1E094F505F52455455524E2034653534303330"
        "31353634353532333332303630663432336530303130303630663432336530663030303030303437323738396365643"
        "53963623865646234363130666339353331636635616564376262643930346630363163303437626231383336343631"
        "31663063316638363634353331633662316563636634353030636264643837663466663530636235303266323035333"
        "96630343463646162626262616261626166626130393761616435343935376262393732613835303366396366646135"
        "31623561626265666566366537366462356139356165323236396132313939653062316639663966393866366266353"
        "93532323633323961373836373961343238303735343562313339653439336265633865643137326235666266653532"
        "66393632636133343738396362376231653835333161373837373739653964333730376432313237626364353865366"
        "66237616464393962613436643638343837623165373434336263366438326262626364323537333737376637646463"
        "64363366643438666166316665666462353765626337656238613764626637366437336664343035373537306637373"
        "33730666364316464356462633165666531633331393437636666333930626433616264613734626439376566643031"
        "65373463663164333132613632336639303264663831363635636663343633316635383836323533316462353164653"
        "56539343733353035646635326563343661626630386562633439623365613335616261343431393031613032333362"
        "31373061303432613764636163633832373133343937313138656434363539373332303035383338633864383932383"
        "23931376132333737656366373239393837383839663661656134646433616232373363666331393636333539396434"
        "38623939613930663936353434663361613632643065623535336530633364333436363964666161386563383861353"
        "96435663433613361623134356666313834656534316235326336653363343330616338376234353938633466383064"
        "62343464323839663033366334666331343339653230323033343862393230373563366433653238643033383231633"
        "83637393233363035616330353035643334653464623864396365383961343361633631326333623233373866663762"
        "33343663313261323234386635383835303935306135353264343165323766363063366638336662336163313835613"
        "76264613532363839373536343161613462313338653631653630356135353462313838643165373436646233623133"
        "36656134396138643238616631633131656137303837393261353831326637343330346331356563383735663837636"
        "33831346364323435653136613231363866343738613939306161646636393938636138633032653830633930646166"
        "37633938623665613562346633656233363637336465393430643437623262333832313832633031323939316533393"
        "26132613331656332323830366332303836663665343938633363343165346136333533306338613331376130323039"
        "36663363313338306337306339363365636130363830393462333638643162633831333838393235306262363030643"
        "83462613238323934353232326537366562333130313735343739333138643462333032303732383134326334383562"
        "35303935343262316535626433386430626534396133386534386464313362363633393836303863633938326336333"
        "03739653433363830343962393065646532643938643039303431336439373964363839613833313561366531303631"
        "65333239353430633062303634306465343636303362326262373831333639663633386334323238633263323834373"
        "76666306532363262303161663930343337303435323933363932303134333837306234303362313238393961373263"
        "65623232633266316437616631333133643661343034306561363130613138303333346130366363633334326333356"
        "53832653962343333613032393936336365343261343030656630313437313138356637613534626464303139316462"
        "66313130643631333638323461656130376331623034383331613337396162613661326133373336653365356136376"
        "63637643961343531303231356234333234613337383531353236313132366136653263653764636437613261626265"
        "62633837653834363666613033393436386534343437346233316436303862656330653031316331303131373663363"
        "63932346430326232613033333735353935366335313761343434313036623665336362303963306266376333633961"
        "35636630353761616163646363366661626533633130623162343139633233323864643732656534636633343934656"
        "53066663438336566666363353934346439393239336263623631333539313139343563393762653930616639343661"
        "30373361343662373962323839656462356165363434333036393434323532316132313238633438383763666563383"
        "63065383532663264633931663461633664306336663639306438343637643831376161363563656634663932343237"
        "63393765343964356132363036343234333739646363303563623938313732313165626138323533346463343134623"
        "43138393861333364623436626164383331616231376439316136643431336634393837313463373132643561396666"
        "38306539396335613535376336313131323366613632613962626432626133356266393136306638336639373861396"
        "53361653361356535666431656461373765616362333930626561653232326437616263393838353230386361386434"
        "38626266343564333130383736666331653334653064363933623462333033353438326631393236643465393434313"
        "26431653738343035663663613563623934306433303732333433323639663839643561613764323361393564366139"
        "62343465613537353261616435333639396434616562353435616137643233613935643661396234346561353735326"
        "16164353336393964346165623534356161376432336139356436613962343465613566666564353464613536396232"
        "61393533666537666534613832623737616664663166616661666132663535663132626365000070FECEB2000000003"
        "23736413931343341384234383533383837423834364245373335354633443844373539353733414546463330363238"
        "384143554F505F445550204F505F4841534831363020336138623438353338383762383436626537333535663364386"
        "43735393537336165666633303632204F505F455155414C564552494659204F505F434845434B534947002254464A6D"
        "3368706473554D796E3666656F44586F704E6573373151394E5A71594247000000000000000001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "VER3");
    EXPECT_EQ(metadataObj.getTokenDescription(), "NTP1 Version3");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "NeblioTeam");
    EXPECT_EQ(
        metadataObj.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/0a0245f28b8ea8571a8165e37dc16e006426c4eb.png");
    EXPECT_EQ(metadataObj.getIconImageType(), "image/png");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("999998"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La4YygP6o8Uszwj2Vmc4WGKn8NGCiswZfpsGdJ");
}

TEST(ntp1_tests, ntp1_metadata_parsing_4)
{
    const std::string tx_hex =
        "010000002E7DF75A01543D77C30360B4961971F76B3BA0D5243B06EA78565BBC34DF2F41953891DA11020000006B483"
        "045022100DD9E342B920614DF4C78B084601E0BCFBAF621687292AFA162372E6CC2A60CAF02200E4F42C4E86D68C3BF"
        "5FEFA4F95AF2C4B918700D1B1877AAACB001FE83FFD6A501210269AB6AECB0C341C84B69A51B6FD91E1C41E641221D5"
        "A724125BC22FE9B4CADBCFFFFFFFF0310270000000000001976A914C3B3289221D299C5665CE9ED79995B3937025CA6"
        "88AC1027000000000000456A434E5401014D54332020D1F4EA66558411F49AA025626196071F061E6538DA897098F13"
        "4B061D3E385ACABAC3616162B995A36BE58B858C3C42F0610D88C2018002018F0A065D5DC010000001976A914C3B328"
        "9221D299C5665CE9ED79995B3937025CA688AC00000000";

    const std::string ntp1tx_hex =
        "0100000050169D326E010000E45814712400B95C445A40601BDD30D966015A930C0C724E8C2F6E83B4D21B7001543D7"
        "7C30360B4961971F76B3BA0D5243B06EA78565BBC34DF2F41953891DA1102000000D634383330343530323231303044"
        "44394533343242393230363134444634433738423038343630314530424346424146363231363837323932414641313"
        "63233373245364343324136304341463032323030453446343243344538364436384333424635464546413446393541"
        "46324334423931383730304431423138373741414143423030314645383346464436413530313231303236394142364"
        "14543423043333431433834423639413531423646443931453143343145363431323231443541373234313235424332"
        "32464539423443414442430000000000000000000310270000000000003237364139313443334233323839323231443"
        "23939433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F48415348"
        "3136302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505"
        "F455155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D6259"
        "68507735794D69397248413310270000000000008A36413433344535343031303134443534333332303230443146344"
        "54136363535383431314634394141303235363236313936303731463036314536353338444138393730393846313334"
        "42303631443345333835414341424143333631363136324239393541333642453538423835384333433432463036313"
        "044383843323031383030323031384630904F505F52455455524E203465353430313031346435343333323032306431"
        "66346561363635353834313166343961613032353632363139363037316630363165363533386461383937303938663"
        "13334623036316433653338356163616261633336313631363262393935613336626535386238353863336334326630"
        "363130643838633230313830303230313866300000A065D5DC010000003237364139313443334233323839323231443"
        "23939433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F48415348"
        "3136302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505"
        "F455155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D6259"
        "68507735794D693972484133000000000000000001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "MT3");
    EXPECT_EQ(metadataObj.getTokenDescription(), "MathToken3");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "Sam");
    EXPECT_EQ(
        metadataObj.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/a5709018231ada78143bd609f30633db7f81d0a4.png");
    EXPECT_EQ(metadataObj.getIconImageType(), "image/png");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("100000000"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La3BKCuBoeqG3FWegVMnwSvivh7xhuxu4XZbSq");
}

TEST(ntp1_tests, ntp1_metadata_parsing_5)
{
    const std::string tx_hex =
        "010000008579F75A014F46A7377AC2307EC22017BCE489CCEB0BF1E295E0804B8ECC7AE5E9D8E69502030000006B483"
        "045022100DE16FBFA765CF0C0AFCB5508BBD272021575BAD46AA64F8AD0A08DB24E6463830220609A465BCB45AAC035"
        "E30025885B8B8087A1E30D0E0E2457AD8C03A823373EF601210269AB6AECB0C341C84B69A51B6FD91E1C41E641221D5"
        "A724125BC22FE9B4CADBCFFFFFFFF0310270000000000001976A914C3B3289221D299C5665CE9ED79995B3937025CA6"
        "88AC1027000000000000456A434E5401014D542020200DF2CD9F93D912C9D1FC1B56A7C9582AB407D4880ACB2EE5983"
        "B7A327AFA13425E709A9195B446022DAEE2260403440402BCED022019002019F0C426F66A130000001976A914C3B328"
        "9221D299C5665CE9ED79995B3937025CA688AC00000000";

    const std::string ntp1tx_hex =
        "0100000050169D326E0100003537464AC8C866D5E544694283DC031CA6C3ACAD8A6FE034017B6D13601C9F9B014F46A"
        "7377AC2307EC22017BCE489CCEB0BF1E295E0804B8ECC7AE5E9D8E6950203000000D634383330343530323231303044"
        "45313646424641373635434630433041464342353530384242443237323032313537354241443436414136344638414"
        "43041303844423234453634363338333032323036303941343635424342343541414330333545333030323538383542"
        "38423830383741314533304430453045323435374144384330334138323333373345463630313231303236394142364"
        "14543423043333431433834423639413531423646443931453143343145363431323231443541373234313235424332"
        "32464539423443414442430000000000000000000310270000000000003237364139313443334233323839323231443"
        "23939433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F48415348"
        "3136302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505"
        "F455155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D6259"
        "68507735794D69397248413310270000000000008A36413433344535343031303134443534323032303230304446324"
        "34439463933443931324339443146433142353641374339353832414234303744343838304143423245453539383342"
        "37413332374146413133343235453730394139313935423434363032324441454532323630343033343430343032424"
        "345443032323031393030323031394630904F505F52455455524E203465353430313031346435343230323032303064"
        "66326364396639336439313263396431666331623536613763393538326162343037643438383061636232656535393"
        "83362376133323761666131333432356537303961393139356234343630323264616565323236303430333434303430"
        "326263656430323230313930303230313966300000C426F66A130000003237364139313443334233323839323231443"
        "23939433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F48415348"
        "3136302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505"
        "F455155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D6259"
        "68507735794D693972484133000000000000000001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "MT");
    EXPECT_EQ(metadataObj.getTokenDescription(), "MyToken");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "Sam");
    EXPECT_EQ(
        metadataObj.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/176c4dba9a03a235d0eb54e5b66e79af97592bb6.png");
    EXPECT_EQ(metadataObj.getIconImageType(), "image/png");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("1000000000"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La3t57xN4xA2jtgnzLW6HbviPZDgdTU1mvP6Yk");
}

TEST(ntp1_tests, ntp1_metadata_parsing_6)
{
    const std::string tx_hex =
        "01000000003A105C024CF4BB07079B6587FA886FB90C2EFCDF5EAAECE9086FD46416058E388B892211000000006A473"
        "04402206B6689BB103E2A19C801DCAEE4BE32CE44EBF6290DAA28C09F86FBF6E425F57902200B20C7998482100454D0"
        "0C42916FF39586C1480221A9AF7A2A0367FA2496474501210263B966632FB59B27023A1D08042E4109C6A467BDEA212"
        "151CB0A7C8BB5553747FFFFFFFF4CF4BB07079B6587FA886FB90C2EFCDF5EAAECE9086FD46416058E388B8922110200"
        "00006B483045022100BADC1C2A923DE65F5BA4CE925220D757FD0BF8834522661B3748D5DD9F4E357D022021413F0F5"
        "FCE5BE6668300FEC6D41A4CBEB67F56787B6A51D3F09880E626FC3701210263B966632FB59B27023A1D08042E4109C6"
        "A467BDEA212151CB0A7C8BB5553747FFFFFFFF0310270000000000001976A9143A8B4853887B846BE7355F3D8D75957"
        "3AEFF306288AC10270000000000006B6A4C684E5403015433372020201601002016F000000054789CAB564A492C4954"
        "B2AA562AC9CF4ECDF34BCC4D55B2520A313657D2514A492D4E2ECA2C28C9CCCF838B65161797A61601B97EA9493999F"
        "921A989B940D1D2E2D42217A839B9A9203A3AB6B6B6160085091E9C30CE983B000000001976A9143A8B4853887B846B"
        "E7355F3D8D759573AEFF306288AC00000000";

    const std::string ntp1tx_hex =
        "0100000050169D326E0100002D9200FDC8E5B5735BB8F2F086EFC0EA0CD802ACF7A10239B387340F10D3E73C024CF4B"
        "B07079B6587FA886FB90C2EFCDF5EAAECE9086FD46416058E388B89221100000000D434373330343430323230364236"
        "36383942423130334532413139433830314443414545344245333243453434454246363239304441413238433039463"
        "83646424636453432354635373930323230304232304337393938343832313030343534443030433432393136464633"
        "39353836433134383032323141394146374132413033363746413234393634373435303132313032363342393636363"
        "33246423539423237303233413144303830343245343130394336413436374244454132313231353143423041374338"
        "4242353535333734370000000000000000004CF4BB07079B6587FA886FB90C2EFCDF5EAAECE9086FD46416058E388B8"
        "9221102000000D634383330343530323231303042414443314332413932334445363546354241344345393235323230"
        "44373537464430424638383334353232363631423337343844354444394634453335374430323230323134313346304"
        "63546434535424536363638333030464543364434314134434245423637463536373837423641353144334630393838"
        "30453632364643333730313231303236334239363636333246423539423237303233413144303830343245343130394"
        "33641343637424445413231323135314342304137433842423535353337343700000000000000000003102700000000"
        "00003237364139313433413842343835333838374238343642453733353546334438443735393537334145464633303"
        "63238384143554F505F445550204F505F48415348313630203361386234383533383837623834366265373335356633"
        "6438643735393537336165666633303632204F505F455155414C564552494659204F505F434845434B5349470022544"
        "64A6D3368706473554D796E3666656F44586F704E6573373151394E5A715942471027000000000000D6364134433638"
        "34453534303330313534333333373230323032303136303130303230313646303030303030303534373839434142353"
        "63441343932433439353442324141353632414339434634454344463334424343344435354232353230413331333635"
        "37443235313441343932443445324543413243323843394343434638333842363531363137393741363136303142393"
        "74541393439333939394639323141393839423934304431443245324434323231374138333942394139323033413341"
        "423642364236313630303835303931453943DA4F505F52455455524E203465353430333031353433333337323032303"
        "23031363031303032303136663030303030303035343738396361623536346134393263343935346232616135363261"
        "63396366346563646633346263633464353562323532306133313336353764323531346134393264346532656361326"
        "33238633963636366383338623635313631373937613631363031623937656139343933393939663932316139383962"
        "39343064316432653264343232313761383339623961393230336133616236623662363136303038353039316539630"
        "00030CE983B000000003237364139313433413842343835333838374238343642453733353546334438443735393537"
        "33414546463330363238384143554F505F445550204F505F48415348313630203361386234383533383837623834366"
        "2653733353566336438643735393537336165666633303632204F505F455155414C564552494659204F505F43484543"
        "4B534947002254464A6D3368706473554D796E3666656F44586F704E6573373151394E5A71594247000000000000000"
        "001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "T37");
    EXPECT_EQ(metadataObj.getTokenDescription(), "T37");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "NeblioTeam");
    EXPECT_EQ(metadataObj.getIconURL(), "");
    EXPECT_EQ(metadataObj.getIconImageType(), "");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("1000000"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La3rtBsLSMCLw4vtoE8YXyPCEmnNkmLe2Z2EaK");
}

TEST(ntp1_tests, ntp1_metadata_parsing_7)
{
    const std::string tx_hex =
        "010000006143105C012EFA20BA0084EB1C5E9E440B5147BB89080A046C268423C896C60A6E14F68FA5020000006B483"
        "045022100BE36DA906A6BFAF4E8032CA7AC5935E671224023D27CA442E4464A5100E7BB76022000F272FF3ACD88A566"
        "7304BA3A0F11C7544960808AC33B814357A749F3A4A64201210263B966632FB59B27023A1D08042E4109C6A467BDEA2"
        "12151CB0A7C8BB5553747FFFFFFFF0310270000000000001976A9143A8B4853887B846BE7355F3D8D759573AEFF3062"
        "88AC1027000000000000FDAD096A4DA9094E5403015434322020201601002016F000000995789CB559CBAE1CB711FD1"
        "562D69D5114CB57B1B232AC043090080E2C7B1378C1E9E6CC65DC2FB1C9010441FF9E532F926305C8260624DD3B3D4D"
        "B21EA74E9DA23E9D269FFDE9CDA753DE7E0DEB3BBF84D39BD3FB577F3A0DA7291C638A7B8EDB5A9FC5E32821E1E3BB7"
        "099E3F63EF8054F4B9A8FD39B7F7D3AADB23E8E58C28FF1E139E7FD78F3E2C59AF7977FA06F8EB35F8EAFCE53BCC5EC"
        "E76D0C7E3D763F86E33C6ECB8BD74F97EB18A66FC2CB5797700D9797AFFF78B95EBFFEF3EBA7E9EBA7F0E4FDEBA7974"
        "F5FBDFAE67C8B579CB1C425BCFFB8F3A98BBF8517F4F8F32F38FC08E9ADFAB604FA09FB7E0D1FAB2B773F175AF6F72D"
        "85C5C5FD288B9BB6794BEE88D9C1913C38B2368C396438EDFC14F7788C71BDB930C77C76DF8615B6E3A565D9A6CDCDF"
        "15666EFC22D64D9A8BEB1F8E3F067F71D4E38B631C6C3AD3E6F1F4A707BC06FF1520E8745B07FC57713FEEE3EE59262"
        "58F1745B7338062C394618925C8A531CCB8C354B39CEEEEDB686D17D287E7157D88537CB9C531C63C031611C70C63C0"
        "7DA830E0C050F52C811B67C28F4F61196B37B576698CEEE62A72C16F30B2EAC71B153F63005F7EF72E46D70D7844844"
        "5A760FF3E0FC1CB17F9623EF65DE4BF6397034F0651ACBD97DBFF266B6417ADED6115E14BC10973DA42962BDC76A448"
        "DC272B83B10120659A046224971CC459D853F62D4B2CDF4515DA3B3328E4E2EC7758C535991AEEF924770FD4EE13EBB"
        "9FE3DD2F14F7392C880EA52690110E298835739D237388D7B04E2E239CB481BE3107CB3C02BBA59C63068428CC2D9AE"
        "A46DDC2234412D66F296AF06966107A183D98892EAE0845BC8794BCA6EA1ACA2D7A8E9159F1C3B33FF8D7FAEACAA92C"
        "C821D082C73E45FC983D4E0888C23FB1130121959C2850EA464DC0D9FD354758D4810801A11C3B5F6E25108C93BF444"
        "222E763DC1285EDF17D240B7B482994687B6AC6CFEE1F3E8C48F1013796BD1CE68E2C40DCA6A839D1158C52C1B86669"
        "8E97900022ABD5BE3AE9DD3590934798A4B2C51EDE602D2BC27F993D4EC982DDB98C14AABDCCF7B8FA34B8676429858"
        "4EFE33448723AA3A912B6296E54B394B20632F5C7CA85F3EE0EE433AC941131BAC3F7D59751E0A8D8E68AA33D2D64D5"
        "BF2D8D51E2832DDACA1EE06F6931F078763FC2EF569F8B07951C6DAB355E9ECDC4639BFC8CA41D1E6C4C26110979D9A"
        "102786094F3E117B8B04E4C64BC701054501CB1FD58D2019B24C4ECCB2DF97B9CBC6CEB96E8BC46E1ECFE564068FFAD"
        "C2246028D1701037CE386D2F09FB52FE90F2116F2641317187963E720D92DCD6B22C1F396D8CFF1A593F8E6539A8608"
        "5EF5701269342C32E0C04A300DBC05287821ACB07A338F7D238F614974825DB12C3095364217072068ED8376AA101B1"
        "BAC4397A2EA7F01722473F924D123718F2D07C660F68472F383602573B09864CB00C00FAD58183F007B1428C5117597"
        "9B8D12CD374FB183D3A0A518C560BA5DF2B3755CB05478D5BD97F3A6E68D1F5BF699A72AEF9CCE9D25D7E80DB0735B8"
        "56B93D9DF5DC223116C662705156D7B1665B33DDA5E6BECD25EFA04A0B95818EF31D2749BE725FD19AE2D238E6560BF"
        "7BA618591B02BBDDEF7D9A6187A00B6F02A710FDCAB24BA731873630AC570431C42F448EF5CAC025C6E878F6DFD0A78"
        "60038AABF04697DAFA1B21D2221746A1C8FE082A72162E83F2187943053E724BCB3D2F4A4312AA6F0D41259014F0D0F"
        "1226F14B0064AC9DAA181A21A380879559E5D88608A9F7C0716808BE826246A83D99415B395708FD68450AF9441552F"
        "7A60A31C299B16810A63DDEC2E42EFF7AAF9AECA249652FB4D4688B703EB4DF1D962482751D9B91DCCC0DF88761558C"
        "93E92EFE6DD4361366C3FAA59EDC45C524347F7C4AEE606E997C573897062A524BB28B5DEC3E2AC35695110E03A4913"
        "ACEB51ACFB77E9B6D7D4387D61D0C4727FD754D5122363B4295993977E04623FA8CC46407542D64A424752C56F158F6"
        "0B6A2E4A857A1A18D52F4ADD18BEF28964361062B5DD8C680683920F6A1ED8FD0090DB113696C9AA0933F227B4C35D8"
        "99D4BE072772BC0B9E74608E3767FF8BA4FC5FA12BCD1B44CA9DF9EC7ECAA6345BD11BE73604EA58542BBC03A132E00"
        "4F2FA0D2A4AAE54D53F7E26CEF13C20A54B7430177B89E85B83B9B1866CB39572093C5E02557AED9E3718E1B9AD71FB"
        "65A5D2B02E0DACAF962CF408D80833D6F9445AB8CC259C0245693BBB8E270F9E28C18A64A5BC1232A908362448D523E"
        "74D260C4FA8D33542C3460ADC5EBBE14527021BB4A4F9FCBE41FC89EB40E6BC2AE82424D68AB552DB08A682E781881D"
        "F6EB1FA82C45889F71AA98CDA53483C7BDCE84B4A43523940A97B3941715C786B822A5F3764118A928BB97D548449C0"
        "9A431588D0970CB4152E6EB058AF68966AEC9DE879952398146291EBA1F32A1556EC2AD5511E3AD7D6238DAB68165B6"
        "57C156BBB3596A68D4A6AF49AAAC931F094447C4C1266B9244F7F04C770D9E3AF3200DD720202125CD204D8BC125C41"
        "9B1659A08AF8B41A72907BD2818947AA7D8D1649DA465D0117B6B68B4B3237C4BD5318545B9B0116B2696706D70EEAA"
        "9585A86290666B1B3374566B970E551ED8E0C7C164992642B3E5D93A0C85013C455361476F1D4406E6163855FB756D5"
        "81C68E66D655042CBF030069B3792190628B652F328F5325E311753FE92AEA36B880E3BF8A447144BB3B0969089D585"
        "7541BB2AD0F8AB24E8ED62001A190EF5264CB87CB0A2ED7A505F343A96529972D62D8DD5FC5A31DAE275066221D4698"
        "49A03C1AF8EB7B2178CBD7B6E2C82C46864A49DEE61F678B827B35AD18B0453306DC3D6C4458FEB6450EF780A2B373F"
        "5A74A5C10227CC8F47771503A308D0A0CEEE22A1658DBD15556F174032C3D2465F8E3E767D243A68FCF2D6A6EB6536E"
        "2B1EEAE01B5BA13D7219BE4AEEA7F73AFC4DC2B25E93D52D6E15D69D32E23544EA905BD7D4296169CAE795AE55085CB"
        "20AC8321D5FD60F3A14E177DC72E7C49400F825EF808C0597F33176A7B6D9E8F5FF4FC664803CD207364AF7C6D101BD"
        "46DD6690A953666EBA024533E19201255A6166BE7FA12EB54BAAF30F1A401ECA46CABF8AAB92C9975468EB487101453"
        "A791B436C9878BA6079724AEA42C9CA862BB30B5E6A014C648B2C6272C1CFB6B288113AB50E3706DA822FDB2CD60959"
        "9BBCB4D15BFB5A3F4FDC1E8461C917F8D14901F36C3DE65231583251B10F42A0A56713E09170896F25E114A69F8B329"
        "D1245AD5D07CF5D25DE17DBF3ECE5CDAC14A36C120D7EE35F4A2792C73E2A94465E8C24E7056014042B0DD7BCA6DBFE"
        "871BEE79086A8D8AE5612F73FDCAE4CB52DD060271788DDB452F55AFF90059EF833D88D3CCF880A71BD08D24E711A4E"
        "59FE93E6C74C78387DFEE5F3E7CFFF01241BA2FC80EA3177000000001976A9143A8B4853887B846BE7355F3D8D75957"
        "3AEFF306288AC00000000";

    const std::string ntp1tx_hex =
        "0100000050169D326E0100000C79381C4B802530E2D3AC9D8EC7638CC7EEB00B4F73CDE2A752796869802C8D012EFA2"
        "0BA0084EB1C5E9E440B5147BB89080A046C268423C896C60A6E14F68FA502000000D634383330343530323231303042"
        "45333644413930364136424641463445383033324341374143353933354536373132323430323344323743413434324"
        "53434363441353130304537424237363032323030304632373246463341434438384135363637333034424133413046"
        "31314337353434393630383038414333334238313433353741373439463341344136343230313231303236334239363"
        "63633324642353942323730323341314430383034324534313039433641343637424445413231323135314342304137"
        "43384242353535333734370000000000000000000310270000000000003237364139313433413842343835333838374"
        "23834364245373335354633443844373539353733414546463330363238384143554F505F445550204F505F48415348"
        "3136302033613862343835333838376238343662653733353566336438643735393537336165666633303632204F505"
        "F455155414C564552494659204F505F434845434B534947002254464A6D3368706473554D796E3666656F44586F704E"
        "6573373151394E5A715942471027000000000000FD5A133641344441393039344535343033303135343334333232303"
        "23032303136303130303230313646303030303030393935373839434235353943424145314342373131464431353632"
        "44363944353131344342353742314232333241433034333039303038304532433742313337384331453945364343363"
        "54443324642314339303130343431464639453533324639323633303543383236303632344444334233443444423231"
        "45413734453944413233453944323639464644453943444137353344453745304445423342424638344433394244334"
        "64235373746334130444137323931433633384137423845444235413946433545333238323145314533424237303939"
        "45334636334546383035344634423941384644333942374637443341414442323345384535384332384646314531333"
        "94537464437384633453243353941463739373746413036463845423335463845414643453533424343354543453736"
        "44304337453344373633463836453333433645434238424437344639374542313841363646433243423537393737303"
        "04439373937414646463738423935454246464546334542413745394542413746304534464445424137393734463546"
        "42444641453637433842353739434231433432354243464642384633413938424246383531374634463846333246333"
        "84643303845394144464142363034464130394642374530443146414232423737334631373541463646373244383543"
        "35433546443238384239424236373934424545383844394331393133433338423233363843333936343338454446433"
        "13446373738384337314244423933304337374337364446383631354236453341353635443941364344434446313536"
        "36364546433232443634443941384245423146384533463036374637314434453338423633314336433341443345364"
        "63146344137303742433036464631353230453837343542303746433537373133464545453345453539323632353846"
        "31373435423733333830363243333934363138393235433841353331434342384333353442333943454545454442363"
        "83644313744323837453731353744383835333743423943353331433633433033313631314337304336334330374441"
        "38333045304330353046353243383131423637433238463446363131393642333742353736363938434545453632413"
        "73243313646333042324541433731423135334636333030354637454637324534364437304437383434383434354137"
        "36304646334530464331434231374639363233454636354445344246363339373033344630363531414342443937444"
        "24646323636423634313741444544363131354531344243313039373344413432393632424443373641343438444332"
        "37324238334231303132303635394130343632323439373143433435394438353346363244344232434446343531354"
        "44133423333323845344532454337373538433533353939314145454639323437373046443445453133454242394645"
        "33444432463134463733393243383830454135323639303131304532393838333537333944323337333838443742303"
        "44532453233394342343831424533313037434233433032424241353943363330363834323843433244394145413436"
        "44444332323334343132443636463239364146303639363631303741313833443938383932454145303834354243383"
        "73934424341364541314143413244374138453931353946314333423333464638443746414541434141393243433832"
        "31443038324337334534354643393833443445303838384332334642313133303132313935394332383530454134363"
        "44443304439464433353437353844343831303830314131314333423546364532353130384339334246343434323232"
        "45373633444331323835454446313744323430423742343832393934363837423641433643464545314633453843343"
        "84631303133373936424431434536384532433430444341364138333944313135384335324331423836363639384539"
        "37393030303232414244354245334145394444333539303933343739384134423243353145444536303244324243323"
        "74639393344344543393832444442393843313441414244434346374238464133344238363736343239383538344546"
        "45333334343837323341413341393132423632393645353442333934423230363332463543374341383546334545304"
        "54534333341433934313133314241433346374435393735314530413844384536384141333344324436344435424632"
        "44384435314532383332444441434131454530364636393331463037383736334643324546353639463842303739353"
        "14336444142333535453945434443343633394246433843413431443145364334433236313130393739443941313032"
        "37383630393446334531313742384230344534433634424337303130353435303143423146443538443230313942323"
        "44334454343423244463937423943424336434542393645384243343645314543464535363430363846464144433232"
        "34363032384431373031303337434533383644324630394642353246453930463231313646323634313331373138373"
        "93633453732304439324443443642323243314633393644384346463141353933463845363533394138363038354546"
        "35373031323639333432433332453043303441333030444243303532383738323141434230374133333846374432333"
        "84636313439373438323544423132433330393533363432313730373230363845443833373641413130314231424143"
        "34333937413245413746303137323234373346393234443132333731384632443037433636304636383437324633383"
        "33630323537334230393836344342303043303046414435383138334630303742313432384335313137353937394238"
        "44313243443337344642313833443341304135313843353630424135444632423337353543423035343738443542443"
        "93746334136453638443146354246363939413732414546394343453944323544374538304442303733354238353642"
        "39334439444635444332323331313643363632373035313536443742313636354233334444413545364245434432354"
        "54641303441304239353831384546333144323734394245373235464431394145324432333845363536304246374241"
        "36313835393142303242424444454637443941363138374130304236463032413731304644434142323442413733313"
        "83733363330414335373034333143343246343438454635434143303235433645383738463644464430413738363030"
        "33384141424630343639374441464131423231443232323137343641314338464530383241373231363245383346323"
        "13837393433303533453732344243423344324634413433313241413646304434313235393031344630443046313232"
        "36463134423030363441433944414131383141323141333830383739353539453544383836303841394637433037313"
        "63830384245383236323436413833443939343135423339353730384644363834353041463934343135353246374136"
        "30413331433239394231363831304136334444454332453432454646374141463941454341323439363532464234443"
        "43638384237303345423444463144393632343832373531443942393144434343304446383837363135353843393345"
        "39324546453644443433363133363643334641413539454443343543353234333437463743344145453630364539393"
        "74335373338393730363241353234424232384235444543334532414333353639353131304530334134393133414345"
        "42353141434642373745394236443744343338374436314430433437323746443735344435313232333633423432393"
        "53939333937374530343632334641384343343634303735343244363441343234373532433536463135384636304236"
        "41324534413835374131413138443532463441444431384245463238393634333631303632423544443843363830363"
        "83339323046364131454438464430303930444231313336393643394141303933334632323742344333354438393944"
        "34424530373237373242433042394537343630384533373637464638424134464335464131324243443142343443413"
        "94446394543374543414136333435424431314245373336303445413538353432424243303341313332453030344632"
        "46413044324134414145353444353346374532364345463133433230413534423734333031373742383945383542383"
        "34239423138363643423339353732303933433545303235353741454439453337313845314239414437314642363541"
        "35443242303245304441434146393632434634303844383038333344364639343435414238434332353943303234353"
        "63933424242384532373046394532384331384136344135424331323332413930383336323434384435323345373444"
        "32363043344641384433333534324333343630414443354542424531343532373032314242344134463946434245343"
        "14643383945423430453642433241453832343234443638414235353244423038413638324537383138383144463645"
        "42314641383243343538383946373141413938434441353334383343374244434538344234413433353233393430413"
        "93742333934313731354337383642383232413546333736343131384139323842423937443534383434394330394134"
        "33313538384430393730434234313532453645423035384146363839363641454339444538373939353233393831343"
        "63239314542413146333241313535364543324144353531314533414437443632333844414236383136354236353743"
        "31353642424233353936413638443441364146343941414143393331463039343434374334433132363642393234344"
        "63746303443373730443945334146333230304444373230323032313235434432303444384243313235433431394231"
        "36353941303841463842343141373239303742443238313839343741413744384431363439444134363544303131374"
        "23642363842344233323337433442443533313835343542394230313136423236393637303644373045454141393538"
        "35413836323930363636423142333337343536364239373045353531454438453043374331363439393236343242334"
        "53544393341304338353031334334353533363134373646314434343036453631363338353546423735364435383143"
        "36384536364436353530343243424630333030363942333739323139303632384236353246333238463533323545333"
        "13137353346453932414541333642383830453342463841343437313434424233423039363930383944353835373534"
        "31424232414430463841423234453845443632303031413139304546353236344342383743423041324544374135303"
        "54633343341393635323939373244363244384444354643354133314441453237353036363232314434363938343941"
        "30334331414638454237423231373843424437423645324338324334363836344134394445453631463637384238323"
        "74233354144313842303435333330364443334436433434353846454236343530454637383041324233373346354137"
        "34413543313032323743433846343737373135303341333038443041304345454532324131363538444244313535353"
        "64631373430333243334432343635463845334537363744323433413638464346324436413645423635333645324231"
        "45454145303142354241313344373231394245344145454137463733414643344443324232354539334435324436453"
        "13544363944333245323335343445413930354244374434323936313639434145373935414535353038354342323041"
        "43383332314435464436304633413134453137374443373245374334393430304638323545463830384330353937463"
        "33331373641374236443945384635464634464336363438303343443230373336344146374336443130314244343644"
        "44363639304139353336363645424130323435333345313932303132353541363136364245374641313245423534424"
        "14146333046314134303145434134364341424638414142393243393937353436384542343837313031343533413739"
        "31423433364339383738424136303739373234414541343243394341383632424233304235453641303134433634384"
        "23243363237324331434642364232383831313341423530453337303644413832324644423243443630393539394242"
        "43423444313542464235413346344644433145383436314339313746384431343930314633364333444536353233313"
        "53833323531423130463432413041353637313345303931373038393646323545313134413639463842333239443132"
        "34354144354430374346354432354445313744424633454345354344414331344133364331323044374545333546344"
        "13237393243373345324139343436354538433234453730353630313430343242304444374243413644424645383731"
        "42454537393038364138443841453536313246373346444341453443423532444430363032373137383844444234353"
        "24635354146463930303539454638333344383844334343463838304137314244303844323445373131413445353946"
        "45393345364337344337383338374446454535463345374346464630313234314241324643FD5C134F505F524554555"
        "24E20346535343033303135343334333232303230323031363031303032303136663030303030303939353738396362"
        "35353963626165316362373131666431353632643639643531313463623537623162323332616330343330393030383"
        "06532633762313337386331653965366363363564633266623163393031303434316666396535333266393236333035"
        "63383236303632346464336233643464623231656137346539646132336539643236396666646539636461373533646"
        "53765306465623362626638346433396264336662353737663361306461373239316336333861376238656462356139"
        "66633565333238323165316533626237303939653366363365663830353466346239613866643339623766376433616"
        "16462323365386535386332386666316531333965376664373866336532633539616637393737666130366638656233"
        "35663865616663653533626363356563653736643063376533643736336638366533336336656362386264373466393"
        "76562313861363666633263623537393737303064393739376166666637386239356562666665663365626137653965"
        "62613766306534666465626137393734663566626466616536376338623537396362316334323562636666623866336"
        "13938626266383531376634663866333266333866633038653961646661623630346661303966623765306431666162"
        "32623737336631373561663666373264383563356335666432383862396262363739346265653838643963313931336"
        "33338623233363863333936343338656466633134663737383863373162646239333063373763373664663836313562"
        "36653361353635643961366364636466313536363665666332326436346439613862656231663865336630363766373"
        "16434653338623633316336633361643365366631663461373037626330366666313532306538373435623037666335"
        "37373133666565653365653539323632353866313734356237333338303632633339343631383932356338613533316"
        "36362386333353462333963656565656462363836643137643238376537313537643838353337636239633533316336"
        "33633033313631316337306336336330376461383330653063303530663532633831316236376332386634663631313"
        "93662333762353736363938636565653632613732633136663330623265616337316231353366363330303566376566"
        "37326534366437306437383434383434356137363066663365306663316362313766393632336566363564653462663"
        "63339373033346630363531616362643937646266663236366236343137616465643631313565313462633130393733"
        "64613432393632626463373661343438646332373262383362313031323036353961303436323234393731636334353"
        "96438353366363264346232636466343531356461336233333238653465326563373735386335333539393161656566"
        "39323437373066643465653133656262396665336464326631346637333932633838306561353236393031313065323"
        "93838333537333964323337333838643762303465326532333963623438316265333130376362336330326262613539"
        "63363330363834323863633264396165613436646463323233343431326436366632393661663036393636313037613"
        "13833643938383932656165303834356263383739346263613665613161636132643761386539313539663163336233"
        "33666638643766616561636161393263633832316430383263373365343566633938336434653038383863323366623"
        "13133303132313935396332383530656134363464633064396664333534373538643438313038303161313163336235"
        "66366532353130386339336266343434323232653736336463313238356564663137643234306237623438323939343"
        "63837623661633663666565316633653863343866313031333739366264316365363865326334306463613661383339"
        "64313135386335326331623836363639386539373930303032326162643562653361653964643335393039333437393"
        "86134623263353165646536303264326263323766393933643465633938326464623938633134616162646363663762"
        "38666133346238363736343239383538346566653333343438373233616133613931326236323936653534623339346"
        "23230363332663563376361383566336565306565343333616339343131333162616333663764353937353165306138"
        "64386536386161333364326436346435626632643864353165323833326464616361316565303666363933316630373"
        "83736336663326566353639663862303739353163366461623335356539656364633436333962666338636134316431"
        "65366334633236313130393739643961313032373836303934663365313137623862303465346336346263373031303"
        "53435303163623166643538643230313962323463346563636232646639376239636263366365623936653862633436"
        "65316563666535363430363866666164633232343630323864313730313033376365333836643266303966623532666"
        "53930663231313666323634313331373138373936336537323064393264636436623232633166333936643863666631"
        "61353933663865363533396138363038356566353730313236393334326333326530633034613330306462633035323"
        "83738323161636230376133333866376432333866363134393734383235646231326333303935333634323137303732"
        "30363865643833373661613130316231626163343339376132656137663031373232343733663932346431323337313"
        "86632643037633636306636383437326633383336303235373362303938363463623030633030666164353831383366"
        "30303762313432386335313137353937396238643132636433373466623138336433613061353138633536306261356"
        "46632623337353563623035343738643562643937663361366536386431663562663639396137326165663963636539"
        "64323564376538306462303733356238353662393364396466356463323233313136633636323730353135366437623"
        "13636356233336464613565366265636432356566613034613062393538313865663331643237343962653732356664"
        "31396165326432333865363536306266376261363138353931623032626264646566376439613631383761303062366"
        "63032613731306664636162323462613733313837333633306163353730343331633432663434386566356361633032"
        "35633665383738663664666430613738363030333861616266303436393764616661316232316432323231373436613"
        "16338666530383261373231363265383366323138373934333035336537323462636233643266346134333132616136"
        "66306434313235393031346630643066313232366631346230303634616339646161313831613231613338303837393"
        "53539653564383836303861396637633037313638303862653832363234366138336439393431356233393537303866"
        "64363834353061663934343135353266376136306133316332393962313638313061363364646563326534326566663"
        "76161663961656361323439363532666234643436383862373033656234646631643936323438323735316439623931"
        "64636363306466383837363135353863393365393265666536646434333631333636633366616135396564633435633"
        "53234333437663763346165653630366539393763353733383937303632613532346262323862356465633365326163"
        "33353639353131306530336134393133616365623531616366623737653962366437643433383764363164306334373"
        "23766643735346435313232333633623432393539393339373765303436323366613863633436343037353432643634"
        "61343234373532633536663135386636306236613265346138353761316131386435326634616464313862656632383"
        "93634333631303632623564643863363830363833393230663661316564386664303039306462313133363936633961"
        "61303933336632323762346333356438393964346265303732373732626330623965373436303865333736376666386"
        "26134666335666131326263643162343463613964663965633765636161363334356264313162653733363034656135"
        "38353432626263303361313332653030346632666130643261346161653534643533663765323663656631336332306"
        "13534623734333031373762383965383562383362396231383636636233393537323039336335653032353537616564"
        "39653337313865316239616437316662363561356432623032653064616361663936326366343038643830383333643"
        "66639343435616238636332353963303234353639336262623865323730663965323863313861363461356263313233"
        "32613930383336323434386435323365373464323630633466613864333335343263333436306164633565626265313"
        "43532373032316262346134663966636265343166633839656234306536626332616538323432346436386162353532"
        "64623038613638326537383138383164663665623166613832633435383839663731616139386364613533343833633"
        "76264636538346234613433353233393430613937623339343137313563373836623832326135663337363431313861"
        "39323862623937643534383434396330396134333135383864303937306362343135326536656230353861663638393"
        "63661656339646538373939353233393831343632393165626131663332613135353665633261643535313165336164"
        "37643632333864616236383136356236353763313536626262333539366136386434613661663439616161633933316"
        "63039343434376334633132363662393234346637663034633737306439653361663332303064643732303230323132"
        "35636432303464386263313235633431396231363539613038616638623431613732393037626432383138393437616"
        "13764386431363439646134363564303131376236623638623462333233376334626435333138353435623962303131"
        "36623236393637303664373065656161393538356138363239303636366231623333373435363662393730653535316"
        "56438653063376331363439393236343262336535643933613063383530313363343535333631343736663164343430"
        "36653631363338353566623735366435383163363865363664363535303432636266303330303639623337393231393"
        "03632386236353266333238663533323565333131373533666539326165613336623838306533626638613434373134"
        "34626233623039363930383964353835373534316262326164306638616232346538656436323030316131393065663"
        "53236346362383763623061326564376135303566333433613936353239393732643632643864643566633561333164"
        "61653237353036363232316434363938343961303363316166386562376232313738636264376236653263383263343"
        "63836346134396465653631663637386238323762333561643138623034353333303664633364366334343538666562"
        "36343530656637383061326233373366356137346135633130323237636338663437373731353033613330386430613"
        "06365656532326131363538646264313535353666313734303332633364323436356638653365373637643234336136"
        "38666366326436613665623635333665326231656561653031623562613133643732313962653461656561376637336"
        "16663346463326232356539336435326436653135643639643332653233353434656139303562643764343239363136"
        "39636165373935616535353038356362323061633833323164356664363066336131346531373764633732653763343"
        "93430306638323565663830386330353937663333313736613762366439653866356666346663363634383033636432"
        "30373336346166376336643130316264343664643636393061393533363636656261303234353333653139323031323"
        "53561363136366265376661313265623534626161663330663161343031656361343663616266386161623932633939"
        "37353436386562343837313031343533613739316234333663393837386261363037393732346165613432633963613"
        "83632626233306235653661303134633634386232633632373263316366623662323838313133616235306533373036"
        "64613832326664623263643630393539396262636234643135626662356133663466646331653834363163393137663"
        "86431343930316633366333646536353233313538333235316231306634326130613536373133653039313730383936"
        "66323565313134613639663862333239643132343561643564303763663564323564653137646266336563653563646"
        "16331346133366331323064376565333566346132373932633733653261393434363565386332346537303536303134"
        "30343262306464376263613664626665383731626565373930383661386438616535363132663733666463616534636"
        "23532646430363032373137383864646234353266353561666639303035396566383333643838643363636638383061"
        "37316264303864323465373131613465353966653933653663373463373833383764666565356633653763666666303"
        "13234316261326663000080EA3177000000003237364139313433413842343835333838374238343642453733353546"
        "33443844373539353733414546463330363238384143554F505F445550204F505F48415348313630203361386234383"
        "5333838376238343662653733353566336438643735393537336165666633303632204F505F455155414C5645524946"
        "59204F505F434845434B534947002254464A6D3368706473554D796E3666656F44586F704E6573373151394E5A71594"
        "247000000000000000001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "T42");
    EXPECT_EQ(metadataObj.getTokenDescription(), "T42");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "NeblioTeam");
    EXPECT_EQ(
        metadataObj.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/76bfced9e14befeb170bff5876d56e6aa7616349.gif");
    EXPECT_EQ(metadataObj.getIconImageType(), "image/gif");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("1000000"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La8hreGMQjDmbhbC7assHJBvSBFS9dh7BRQMy3");
}

TEST(ntp1_tests, ntp1_metadata_parsing_8)
{
    const std::string tx_hex =
        "01000000F27AF75A01F6484080F4601C7862F62C53A08F94045E46F316606E91F657E3459B2E97FF46020000006B483"
        "045022100D0FEC5B458E0770DE1BAB716E66114D991A10EAB1B8D38C097601620F0E9F82A0220065209E298B340786E"
        "E9BFEF9D443DF705B9F57F31D9B0A2E733901AD45CC38A01210269AB6AECB0C341C84B69A51B6FD91E1C41E641221D5"
        "A724125BC22FE9B4CADBCFFFFFFFF0310270000000000001976A914C3B3289221D299C5665CE9ED79995B3937025CA6"
        "88AC1027000000000000456A434E5401014D54312020941900979453B87DDBE7B504DFD1C5846740934013D3C29BAAC"
        "88E16966D28A15497FA1D8EB1A3878E379DABAC23F6B81C7C3E732019002019F064A8BFF3120000001976A914C3B328"
        "9221D299C5665CE9ED79995B3937025CA688AC00000000";

    const std::string ntp1tx_hex =
        "0100000050169D326E0100001062118FE1699FB1ECB896DE15E5673C9AF368A3394728229FB8332F4E39FE3201F6484"
        "080F4601C7862F62C53A08F94045E46F316606E91F657E3459B2E97FF4602000000D634383330343530323231303044"
        "30464543354234353845303737304445314241423731364536363131344439393141313045414231423844333843303"
        "93736303136323046304539463832413032323030363532303945323938423334303738364545394246454639443434"
        "33444637303542394635374633314439423041324537333339303141443435434333384130313231303236394142364"
        "14543423043333431433834423639413531423646443931453143343145363431323231443541373234313235424332"
        "32464539423443414442430000000000000000000310270000000000003237364139313443334233323839323231443"
        "23939433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F48415348"
        "3136302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505"
        "F455155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D6259"
        "68507735794D69397248413310270000000000008A36413433344535343031303134443534333132303230393431393"
        "03039373934353342383744444245374235303444464431433538343637343039333430313344334332394241414338"
        "38453136393636443238413135343937464131443845423141333837384533373944414241433233463642383143374"
        "333453733323031393030323031394630904F505F52455455524E203465353430313031346435343331323032303934"
        "31393030393739343533623837646462653762353034646664316335383436373430393334303133643363323962616"
        "16338386531363936366432386131353439376661316438656231613338373865333739646162616332336636623831"
        "63376333653733323031393030323031396630000064A8BFF3120000003237364139313443334233323839323231443"
        "23939433536363543453945443739393935423339333730323543413638384143554F505F445550204F505F48415348"
        "3136302063336233323839323231643239396335363635636539656437393939356233393337303235636136204F505"
        "F455155414C564552494659204F505F434845434B534947002254546F79506365465A6A77775738384A3174556D6259"
        "68507735794D693972484133000000000000000001000000";

    CDataStream tx_ds(ParseHex(tx_hex), SER_NETWORK, PROTOCOL_VERSION);
    CDataStream ntp1tx_ds(ParseHex(ntp1tx_hex), SER_NETWORK, PROTOCOL_VERSION);

    CTransaction    tx;
    NTP1Transaction ntp1tx;
    tx_ds >> tx;
    ntp1tx_ds >> ntp1tx;

    NTP1TokenMetaData metadataObj = NTP1Transaction::GetFullNTP1IssuanceMetadata(tx, ntp1tx);

    EXPECT_EQ(metadataObj.getTokenName(), "MT1");
    EXPECT_EQ(metadataObj.getTokenDescription(), "MathToken1");
    EXPECT_EQ(metadataObj.getTokenIssuer(), "Sam");
    EXPECT_EQ(
        metadataObj.getIconURL(),
        "https://ntp1-icons.ams3.digitaloceanspaces.com/8dde92ea5fab1b98f793b67b7b0b2345230719a0.png");
    EXPECT_EQ(metadataObj.getIconImageType(), "image/png");
    EXPECT_EQ(metadataObj.getTotalSupply(), NTP1Int("1000000000"));
    EXPECT_EQ(metadataObj.getDivisibility(), 7);
    EXPECT_EQ(metadataObj.getLockStatus(), true);
    EXPECT_EQ(metadataObj.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(metadataObj.getIssuanceTxId(), tx.GetHash());
    EXPECT_EQ(metadataObj.getTokenId(), "La37utZFe8P1fPc789szDp2QL7TMC7hyWERxr5");
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

// TEST(ntp1_tests, tmp)
//{
//    TestNTP1TxParsing("0a71b6db7994cc91d7e24302428e44dc0871eff23caddfd75099e76666374175", false);
//}

// TEST(ntp1_tests, tmp2)
//{
//    std::shared_ptr<NTP1Script> script =
//        NTP1Script::ParseScript("4e54040164697634302018010120189000000053789cab564a492c4954b2aa562ac9cf4"
//                                "ecdf34bcc4d55b2524ac92c333150d2514a492d4e2eca2c28c9cccf038aba6496651667"
//                                "2665e66496542a98288035001565161797a61601e5831373956a6b017fb01bda");
//    std::shared_ptr<NTP1Script_Issuance> script_issuance =
//        std::dynamic_pointer_cast<NTP1Script_Issuance>(script);
//    ASSERT_NE(script_issuance, nullptr);
//    std::cout << script_issuance->getAmount() << std::endl;
//    for (unsigned i = 0; i < script_issuance->getTransferInstructionsCount(); i++) {
//        std::cout << "Transfer instruction: " << i << std::endl;
//        std::cout << "\t Output index: " << script_issuance->getTransferInstruction(i).outputIndex
//                  << std::endl;
//        std::cout << "\t Amount: " << script_issuance->getTransferInstruction(i).amount << std::endl;
//        std::cout << "\t Skip: " << script_issuance->getTransferInstruction(i).skipInput << std::endl;
//    }
//}

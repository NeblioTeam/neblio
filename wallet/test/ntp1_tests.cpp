#include "googletest/googletest/include/gtest/gtest.h"

#include "ntp1/ntp1txin.h"
#include "ntp1/ntp1txout.h"
#include "ntp1/ntp1tokentxdata.h"
#include "ntp1/ntp1transaction.h"
#include "ntp1/ntp1tokenmetadata.h"

TEST(ntp1_tests, parse_NTP1TxIn_from_json)
{
    std::string tx_str = "{\"txid\":\"4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90\",\"vout\":1,\"scriptSig\":{\"asm\":\"3045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f01 03f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\",\"hex\":\"483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\"},\"sequence\":4294967295,\"previousOutput\":{\"asm\":\"OP_DUP OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[{\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":997000,\"issueTxid\":\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"value\":10000,\"fixed\":true}";
    NTP1TxIn tx_good, tx_nogood;
    EXPECT_NO_THROW(tx_good.importJsonData(tx_str));
    EXPECT_ANY_THROW(tx_nogood.importJsonData(tx_str.substr(0, tx_str.size() - 1)));
    EXPECT_EQ(tx_good.getOutPoint().getHash().ToString(), "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getOutPoint().getIndex(), (unsigned int)1);
    EXPECT_EQ(tx_good.getScriptSigHex(), "483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getSequence(), (uint64_t)4294967295);
    EXPECT_EQ(tx_good.getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getToken(0).getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getToken(0).getDivisibility(), (unsigned)7);
    EXPECT_EQ(tx_good.getToken(0).getAmount(), (unsigned)997000);
    EXPECT_EQ(tx_good.getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getToken(0).getIssueTxId().ToString(), "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
}

TEST(ntp1_tests, parse_NTP1TxOut_from_json)
{
    std::string tx_str = "{\"value\":10000,\"n\":0,\"scriptPubKey\":{\"asm\":\"OP_DUP OP_HASH160 930b31797c0e6f0d4239909b044aaadfde371995 OP_EQUALVERIFY OP_CHECKSIG\",\"hex\":\"76a914930b31797c0e6f0d4239909b044aaadfde37199588ac\",\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":[\"NZKTvgBXGBFDde73TizBmxVPzNauT1ivxV\"]},\"tokens\":[{\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":1000,\"issueTxid\":\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"used\":false,\"blockheight\":159366}";
    NTP1TxOut tx_good, tx_nogood;
    EXPECT_NO_THROW(tx_good.importJsonData(tx_str));
    EXPECT_ANY_THROW(tx_nogood.importJsonData(tx_str.substr(0, tx_str.size() - 1)));
    EXPECT_EQ(tx_good.getValue(), 10000);
    EXPECT_EQ(tx_good.getScriptPubKeyHex(), "76a914930b31797c0e6f0d4239909b044aaadfde37199588ac");
    EXPECT_EQ(tx_good.getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getToken(0).getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getToken(0).getDivisibility(), (unsigned)7);
    EXPECT_EQ(tx_good.getToken(0).getAmount(), (unsigned)1000);
    EXPECT_EQ(tx_good.getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getToken(0).getIssueTxId().ToString(), "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
}

TEST(ntp1_tests, parse_NTP1TokenData)
{
    std::string token_str = "{\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":512345,\"issueTxid\":\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}";
    NTP1TokenTxData token_good, token_nogood;
    EXPECT_NO_THROW(token_good.importJsonData(token_str));
    EXPECT_ANY_THROW(token_nogood.importJsonData(token_str.substr(0, token_str.size() - 1)));
    EXPECT_EQ(token_good.getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(token_good.getDivisibility(), (unsigned)7);
    EXPECT_EQ(token_good.getAmount(), (unsigned)512345);
    EXPECT_EQ(token_good.getLockStatus(), true);
    EXPECT_EQ(token_good.getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(token_good.getIssueTxId().ToString(), "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");

    // test database saving
    json_spirit::Value v = token_good.exportDatabaseJsonData();
    NTP1TokenTxData m;
    m.importDatabaseJsonData(v);
    EXPECT_EQ(m, token_good);
}

TEST(ntp1_tests, parse_NTP1Transaction)
{
    std::string tx_str = "{\"hex\":\"010000001c4d9e5a0290bed89598211fb5feac9c80f475dc16486ca352d6a40c867a9f6106a3e3334f010000006b483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8ffffffff90bed89598211fb5feac9c80f475dc16486ca352d6a40c867a9f6106a3e3334f030000006a47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8ffffffff0410270000000000001976a914930b31797c0e6f0d4239909b044aaadfde37199588ac10270000000000001976a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac10270000000000000d6a0b4e54011500201301403e4390a1883b000000001976a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac00000000\",\"txid\":\"1ec95ea69b16385da91a87af7c60bc625d4b96416a45f5f95f47e4d07c19aebc\",\"version\":1,\"locktime\":0,\"vin\":[{\"txid\":\"4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90\",\"vout\":1,\"scriptSig\":{\"asm\":\"3045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f01 03f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\",\"hex\":\"483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\"},\"sequence\":4294967295,\"previousOutput\":{\"asm\":\"OP_DUP OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[{\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":997000,\"issueTxid\":\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"value\":10000,\"fixed\":true},{\"txid\":\"4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90\",\"vout\":3,\"scriptSig\":{\"asm\":\"304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a5501 03f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\",\"hex\":\"47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8\"},\"sequence\":4294967295,\"previousOutput\":{\"asm\":\"OP_DUP OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[],\"value\":998840000,\"fixed\":true}],\"vout\":[{\"value\":10000,\"n\":0,\"scriptPubKey\":{\"asm\":\"OP_DUP OP_HASH160 930b31797c0e6f0d4239909b044aaadfde371995 OP_EQUALVERIFY OP_CHECKSIG\",\"hex\":\"76a914930b31797c0e6f0d4239909b044aaadfde37199588ac\",\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":[\"NZKTvgBXGBFDde73TizBmxVPzNauT1ivxV\"]},\"tokens\":[{\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":1000,\"issueTxid\":\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"used\":false,\"blockheight\":159366},{\"value\":10000,\"n\":1,\"scriptPubKey\":{\"asm\":\"OP_DUP OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[{\"tokenId\":\"LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z\",\"amount\":996000,\"issueTxid\":\"8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\"}],\"used\":true,\"blockheight\":159366,\"usedBlockheight\":-1,\"usedTxid\":\"800ab6605c9d55730615617d9147aacde311433f497e9acf9d3dab143058045a\"},{\"value\":10000,\"n\":2,\"scriptPubKey\":{\"asm\":\"OP_RETURN 4e54011500201301403e43\",\"hex\":\"6a0b4e54011500201301403e43\",\"type\":\"nulldata\"},\"tokens\":[],\"used\":false,\"blockheight\":159366},{\"value\":998810000,\"n\":3,\"scriptPubKey\":{\"asm\":\"OP_DUP OP_HASH160 9e719d5db5e01bb357188f7ab25e336a9c2de112 OP_EQUALVERIFY OP_CHECKSIG\",\"hex\":\"76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac\",\"reqSigs\":1,\"type\":\"pubkeyhash\",\"addresses\":[\"NaMk49sEa6z5jUQVVtkjsraSV7HtsRvR1D\"]},\"tokens\":[],\"used\":true,\"blockheight\":159366,\"usedBlockheight\":-1,\"usedTxid\":\"9343e14519eaef287519d02d3baecf3126de462d74dea7ddd479ff2abe365b02\"}],\"blocktime\":1520323876000,\"blockheight\":159366,\"totalsent\":998840000,\"fee\":10000,\"blockhash\":\"c1cba88c61842c1c760bbfeb0fb977e9d0b266208084a09af0bf2e27cfad29fd\",\"time\":1520323876000,\"confirmations\":26948}";
    NTP1Transaction tx_good, tx_nogood;
    EXPECT_NO_THROW(tx_good.importJsonData(tx_str));
    EXPECT_ANY_THROW(tx_nogood.importJsonData(tx_str.substr(0, tx_str.size() - 1)));
    std::string hex = tx_good.getHex();
    std::transform(hex.begin(), hex.end(), hex.begin(), ::tolower);
    EXPECT_EQ(hex, "010000001c4d9e5a0290bed89598211fb5feac9c80f475dc16486ca352d6a40c867a9f6106a3e3334f010000006b483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8ffffffff90bed89598211fb5feac9c80f475dc16486ca352d6a40c867a9f6106a3e3334f030000006a47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8ffffffff0410270000000000001976a914930b31797c0e6f0d4239909b044aaadfde37199588ac10270000000000001976a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac10270000000000000d6a0b4e54011500201301403e4390a1883b000000001976a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac00000000");
    EXPECT_EQ(tx_good.getLockTime(), (uint64_t)0);
    EXPECT_EQ(tx_good.getTime(), (uint64_t)1520323876000);
    EXPECT_EQ(tx_good.getTxHash().ToString(), "1ec95ea69b16385da91a87af7c60bc625d4b96416a45f5f95f47e4d07c19aebc");


    EXPECT_EQ(tx_good.getTxInCount(), (unsigned long)2);
    EXPECT_EQ(tx_good.getTxIn(0).getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxIn(1).getNumOfTokens(), (unsigned long)0);

    EXPECT_EQ(tx_good.getTxIn(0).getOutPoint().getHash().ToString(), "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getTxIn(0).getOutPoint().getIndex(), (unsigned)1);
    EXPECT_EQ(tx_good.getTxIn(0).getScriptSigHex(), "483045022100975208417dc562459a5d65d8996edf7be7e8ba4f6c30a04a345c63a7cefc413b02202068896fca8e916a85c10202e7bad783d756db9d33571cde646113f68aa99a7f012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getTxIn(0).getSequence(), (uint64_t)4294967295);

    EXPECT_EQ(tx_good.getTxIn(1).getOutPoint().getHash().ToString(), "4f33e3a306619f7a860ca4d652a36c4816dc75f4809cacfeb51f219895d8be90");
    EXPECT_EQ(tx_good.getTxIn(1).getOutPoint().getIndex(), (unsigned)3);
    EXPECT_EQ(tx_good.getTxIn(1).getScriptSigHex(), "47304402200f4123e57d950f434605a845efce86c06af1d0017691c98f13c3dfe51881ac0802205768dedbe3baaefafd65fd9d67e583cad6c96fdeb542d50f40022fef07d87a55012103f4db6a95b42b695ed59f3584c162e6fdd4e8e5223c2da74938a725ed7b0244a8");
    EXPECT_EQ(tx_good.getTxIn(1).getSequence(), (uint64_t)4294967295);

    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getAmount(), (uint64_t)997000);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getDivisibility(), (uint64_t)7);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getIssueTxId().ToString(), "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getLockStatus(), true);
    EXPECT_EQ(tx_good.getTxIn(0).getToken(0).getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");

    EXPECT_EQ(tx_good.getTxOutCount(), (unsigned long)4);
    EXPECT_EQ(tx_good.getTxOut(0).getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxOut(1).getNumOfTokens(), (unsigned long)1);
    EXPECT_EQ(tx_good.getTxOut(2).getNumOfTokens(), (unsigned long)0);
    EXPECT_EQ(tx_good.getTxOut(3).getNumOfTokens(), (unsigned long)0);

    EXPECT_EQ(tx_good.getTxOut(0).getScriptPubKeyHex(), "76a914930b31797c0e6f0d4239909b044aaadfde37199588ac");
    EXPECT_EQ(tx_good.getTxOut(0).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getAmount(), (uint64_t)1000);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getDivisibility(), (uint64_t)7);
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getIssueTxId().ToString(), "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getTxOut(0).getToken(0).getLockStatus(), true);

    EXPECT_EQ(tx_good.getTxOut(1).getScriptPubKeyHex(), "76a9149e719d5db5e01bb357188f7ab25e336a9c2de11288ac");
    EXPECT_EQ(tx_good.getTxOut(1).getValue(), 10000);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getAggregationPolicy(), "aggregatable");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getAmount(), (uint64_t)996000);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getDivisibility(), (uint64_t)7);
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getIssueTxId().ToString(), "8e5b8361d16f166afd4f091d50554b93395dc44bd18a0904ac1e4f5532925d6b");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getTokenIdBase58(), "LaAGekc9oBLcxMUXWzzbMoHRsZXYwhw1Ciox3z");
    EXPECT_EQ(tx_good.getTxOut(1).getToken(0).getLockStatus(), true);

    json_spirit::Value v = tx_good.exportDatabaseJsonData();
    NTP1Transaction t;
    t.importDatabaseJsonData(v);
    EXPECT_EQ(t, tx_good);
}

TEST(ntp1_tests, token_meta_data) {
    std::string metadata = "{\"tokenId\":\"La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\",\"someUtxo\":\"0001e26e6b5e98d1b4e96987f1e010f351f4fa2d6d0bacfafc7f501787bbf4f6:8\",\"numOfHolders\":8281,\"totalSupply\":1607509562,\"numOfTransfers\":1679,\"numOfIssuance\":1,\"numOfBurns\":1,\"firstBlock\":159746,\"issuanceTxid\":\"578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56\",\"issueAddress\":\"NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW\",\"metadataOfIssuence\":{\"data\":{\"tokenName\":\"TRIF\",\"description\":\"Trifid\",\"issuer\":\"TrifidTeam\",\"urls\":[{\"name\":\"icon\",\"url\":\"https://ntp1-icons.ams3.digitaloceanspaces.com/24d000d478d72d6a43b89bbabbe41a45acde9ba0.png\",\"mimeType\":\"image/png\"}],\"userData\":{\"meta\":[]}}},\"sha2Issue\":\"c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113\"}";
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
    EXPECT_EQ(tokenMetaData.getIconURL(), "https://ntp1-icons.ams3.digitaloceanspaces.com/24d000d478d72d6a43b89bbabbe41a45acde9ba0.png");
    EXPECT_EQ(tokenMetaData.getIssuanceTxId().ToString(), "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssuanceTxIdHex(), "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssueAddress().ToString(), "NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW");
    EXPECT_EQ(tokenMetaData.getSha2Issue(), "c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113");
    EXPECT_EQ(tokenMetaData.getTokenDescription(), "Trifid");
    EXPECT_EQ(tokenMetaData.getTokenName(), "TRIF");
    EXPECT_EQ(tokenMetaData.getTokenIssuer(), "TrifidTeam");
    EXPECT_EQ(tokenMetaData.getTotalSupply(), (unsigned)1607509562);
}

TEST(ntp1_tests, token_meta_data_without_url) {
    std::string metadata = "{\"tokenId\":\"La3QxvUgFwKz2jjQR2HSrwaKcRgotf4tGVkMJx\",\"divisibility\":7,\"lockStatus\":true,\"aggregationPolicy\":\"aggregatable\",\"someUtxo\":\"0001e26e6b5e98d1b4e96987f1e010f351f4fa2d6d0bacfafc7f501787bbf4f6:8\",\"numOfHolders\":8281,\"totalSupply\":1607509562,\"numOfTransfers\":1679,\"numOfIssuance\":1,\"numOfBurns\":1,\"firstBlock\":159746,\"issuanceTxid\":\"578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56\",\"issueAddress\":\"NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW\",\"metadataOfIssuence\":{\"data\":{\"tokenName\":\"TRIF\",\"description\":\"Trifid\",\"issuer\":\"TrifidTeam\",\"userData\":{\"meta\":[]}}},\"sha2Issue\":\"c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113\"}";
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
    EXPECT_EQ(tokenMetaData.getIssuanceTxId().ToString(), "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssuanceTxIdHex(), "578a788a8a86ccc7fa0c045ee63ff1dd9c05ae38b08ef10ac62d18ff9783ee56");
    EXPECT_EQ(tokenMetaData.getIssueAddress().ToString(), "NMi41ze2XnxJrMNGtSGheGqLR3h4dabREW");
    EXPECT_EQ(tokenMetaData.getSha2Issue(), "c5f374b0f087cd3ebba03c0046123459e5e8adb2071af6cbae840e1f778cc113");
    EXPECT_EQ(tokenMetaData.getTokenDescription(), "Trifid");
    EXPECT_EQ(tokenMetaData.getTokenName(), "TRIF");
    EXPECT_EQ(tokenMetaData.getTokenIssuer(), "TrifidTeam");
    EXPECT_EQ(tokenMetaData.getTotalSupply(), (unsigned)1607509562);

    // test database saving
    json_spirit::Value v = tokenMetaData.exportDatabaseJsonData();
    NTP1TokenMetaData m;
    m.importDatabaseJsonData(v);
    EXPECT_EQ(m, tokenMetaData);
}

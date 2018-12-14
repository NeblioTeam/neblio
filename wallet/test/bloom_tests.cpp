#include "googletest/googletest/include/gtest/gtest.h"
#include <vector>

#include "bloom.h"
#include "util.h"
#include "key.h"
#include "base58.h"
#include "main.h"

using namespace std;
using namespace boost::tuples;

/*
 Relevant transactions:

Spending transaction:
txid: ee1e4abd0c0240aebf450eed3e575b26440ab4202cf54f7ba0b12d514ebf646b
raw:
```
010000004d73435a01d3db1c519251eeecc76b3c68e550988290aa1d7c63d6b396b0ad069ebe987335000000006b483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff02b592849f300000001976a91401854abf39d84762ebeeb332e3116be49f1c4dad88ac00e87648170000001976a91438f54f511df07214284de94d7cffda10b38004d988ac00000000284de94d7cffda10b38004d988ac00000000284de94d7cffda10b38004d988ac00000000
```
json:
```
{
   "txid":"ee1e4abd0c0240aebf450eed3e575b26440ab4202cf54f7ba0b12d514ebf646b",
   "version":1,
   "time":1514369869,
   "locktime":0,
   "vin":[
      {
         "txid":"357398be9e06adb096b3d6637c1daa90829850e5683c6bc7ecee5192511cdbd3",
         "vout":0,
         "scriptSig":{
            "asm":"3045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d9801 027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbf",
            "hex":"483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbf"
         },
         "sequence":4294967295
      }
   ],
   "vout":[
      {
         "value":2088.34695861,
         "n":0,
         "scriptPubKey":{
            "asm":"OP_DUP OP_HASH160 01854abf39d84762ebeeb332e3116be49f1c4dad OP_EQUALVERIFY OP_CHECKSIG",
            "reqSigs":1,
            "type":"pubkeyhash",
            "addresses":[
               "NL41YEhKDahecGNzrhn5DMffhPR2Z86Suw"
            ]
         }
      },
      {
         "value":1000.00000000,
         "n":1,
         "scriptPubKey":{
            "asm":"OP_DUP OP_HASH160 38f54f511df07214284de94d7cffda10b38004d9 OP_EQUALVERIFY OP_CHECKSIG",
            "reqSigs":1,
            "type":"pubkeyhash",
            "addresses":[
               "NR78tvZnR1n1sgPVgD2LFyr5dXzb6KCyre"
            ]
         }
      }
   ]
}
```
=========================================
Main transaction:

txid: 357398be9e06adb096b3d6637c1daa90829850e5683c6bc7ecee5192511cdbd3

raw:
```
0100000082ca3c5a0376142d8fd77a2061cd7941afe7657cdd29debe2625d3cf6d2f17f83902cf6505010000006a4730440220183e52d87de1bcb2a445d385bd4301e10c12aecae37624be8d63c4159dd08eca02206ead801d61dd08c57618e64a619f5aa756a91671efd10aadcf5fb6cb7995f29d0121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff0f1ff19094598cb6d7059937877b777e670cc4a1932d5427ae802bfbcf53a0bd01000000484730440220527ffbff0a466b8e0e2939908f2d969f3d0ce67a358f9c579e217ffb8ecef42c02206eefaf800037348b9ddac5860db09a356b096e7c7752de1cd359c9252287615701ffffffff42b86ba78de2711c9f84930b96de857a8310347a3fe06c31d2049ee60d49f247010000006a4730440220232b396a3ecd9a2f70a54d16a5bde69cb46505ae00cd425e741228ce453fcf1b022018d2b42a00c2b43c9ec35d8eece4838941ed87523c4a8b33f7463ca2ae794a190121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff01c5a1fbe7470000001976a914990176ee35e3f2d5dcf87f849d7c2722a52fd66088ac00000000
```
json:
```
{
   "txid":"ee1e4abd0c0240aebf450eed3e575b26440ab4202cf54f7ba0b12d514ebf646b",
   "version":1,
   "time":1514369869,
   "locktime":0,
   "vin":[
      {
         "txid":"357398be9e06adb096b3d6637c1daa90829850e5683c6bc7ecee5192511cdbd3",
         "vout":0,
         "scriptSig":{
            "asm":"3045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d9801 027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbf",
            "hex":"483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbf"
         },
         "sequence":4294967295
      }
   ],
   "vout":[
      {
         "value":2088.34695861,
         "n":0,
         "scriptPubKey":{
            "asm":"OP_DUP OP_HASH160 01854abf39d84762ebeeb332e3116be49f1c4dad OP_EQUALVERIFY OP_CHECKSIG",
            "reqSigs":1,
            "type":"pubkeyhash",
            "addresses":[
               "NL41YEhKDahecGNzrhn5DMffhPR2Z86Suw"
            ]
         }
      },
      {
         "value":1000.00000000,
         "n":1,
         "scriptPubKey":{
            "asm":"OP_DUP OP_HASH160 38f54f511df07214284de94d7cffda10b38004d9 OP_EQUALVERIFY OP_CHECKSIG",
            "reqSigs":1,
            "type":"pubkeyhash",
            "addresses":[
               "NR78tvZnR1n1sgPVgD2LFyr5dXzb6KCyre"
            ]
         }
      }
   ]
}
```
 */

TEST(bloom_tests, bloom_create_insert_serialize)
{
    CBloomFilter filter(3, 0.01, 0, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    EXPECT_TRUE( filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"))) << "BloomFilter doesn't contain just-inserted object!";
    // One bit different in first byte
    EXPECT_TRUE(!filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8"))) << "BloomFilter contains something it shouldn't!";

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    EXPECT_TRUE(filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"))) << "BloomFilter doesn't contain just-inserted object (2)!";

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    EXPECT_TRUE(filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"))) << "BloomFilter doesn't contain just-inserted object (3)!";

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    filter.Serialize(stream, SER_NETWORK, PROTOCOL_VERSION);

    vector<unsigned char> vch = ParseHex("03614e9b050000000000000001");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    EXPECT_TRUE(std::equal(stream.begin(), stream.end(), expected.begin()));
}

TEST(bloom_tests, bloom_create_insert_serialize_with_tweak)
{
    // Same test as bloom_create_insert_serialize, but we add a nTweak of 100
    CBloomFilter filter(3, 0.01, 2147483649, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    EXPECT_TRUE( filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"))) << "BloomFilter doesn't contain just-inserted object!";
    // One bit different in first byte
    EXPECT_TRUE(!filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8"))) << "BloomFilter contains something it shouldn't!";

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    EXPECT_TRUE(filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"))) << "BloomFilter doesn't contain just-inserted object (2)!";

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    EXPECT_TRUE(filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"))) << "BloomFilter doesn't contain just-inserted object (3)!";

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    filter.Serialize(stream, SER_NETWORK, PROTOCOL_VERSION);

    vector<unsigned char> vch = ParseHex("03ce4299050000000100008001");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    EXPECT_TRUE(std::equal(stream.begin(), stream.end(), expected.begin()));
}

TEST(bloom_tests, bloom_create_insert_key)
{
    string strSecret = string("TtnutkcnaPcu3zmjWcrJazf42fp1YAKRpm8grKRRuYjtiykmGuM7");
    CBitcoinSecret vchSecret;
    EXPECT_TRUE(vchSecret.SetString(strSecret));

    CKey key;
    bool fCompressed;
    CSecret secret = vchSecret.GetSecret(fCompressed);
    key.SetSecret(secret, fCompressed);

    CBloomFilter filter(2, 0.001, 0, BLOOM_UPDATE_ALL);
    filter.insert(key.GetPubKey().Raw());
    uint160 hash = key.GetPubKey().GetID();
    filter.insert(vector<unsigned char>(hash.begin(), hash.end()));

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    filter.Serialize(stream, SER_NETWORK, PROTOCOL_VERSION);

    vector<unsigned char> vch = ParseHex("032294de080000000000000001");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    EXPECT_TRUE(std::equal(stream.begin(), stream.end(), expected.begin()));
}

TEST(bloom_tests, bloom_match)
{
    string transactionStr = "0100000082ca3c5a0376142d8fd77a2061cd7941afe7657cdd29debe2625d3cf6d2f17f83902cf6505010000006a4730440220183e52d87de1bcb2a445d385bd4301e10c12aecae37624be8d63c4159dd08eca02206ead801d61dd08c57618e64a619f5aa756a91671efd10aadcf5fb6cb7995f29d0121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff0f1ff19094598cb6d7059937877b777e670cc4a1932d5427ae802bfbcf53a0bd01000000484730440220527ffbff0a466b8e0e2939908f2d969f3d0ce67a358f9c579e217ffb8ecef42c02206eefaf800037348b9ddac5860db09a356b096e7c7752de1cd359c9252287615701ffffffff42b86ba78de2711c9f84930b96de857a8310347a3fe06c31d2049ee60d49f247010000006a4730440220232b396a3ecd9a2f70a54d16a5bde69cb46505ae00cd425e741228ce453fcf1b022018d2b42a00c2b43c9ec35d8eece4838941ed87523c4a8b33f7463ca2ae794a190121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff01c5a1fbe7470000001976a914990176ee35e3f2d5dcf87f849d7c2722a52fd66088ac00000000";
    CDataStream stream(ParseHex(transactionStr), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    EXPECT_TRUE(tx.CheckTransaction()) << "Simple deserialized transaction should be valid.";

    string spendTransaction = "010000004d73435a01d3db1c519251eeecc76b3c68e550988290aa1d7c63d6b396b0ad069ebe987335000000006b483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff02b592849f300000001976a91401854abf39d84762ebeeb332e3116be49f1c4dad88ac00e87648170000001976a91438f54f511df07214284de94d7cffda10b38004d988ac00000000";
    CDataStream spendStream(ParseHex(spendTransaction), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction spendingTx;
    spendStream >> spendingTx;
    EXPECT_TRUE(spendingTx.CheckTransaction()) << "Simple deserialized of spending transaction should be valid.";

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256("0x357398be9e06adb096b3d6637c1daa90829850e5683c6bc7ecee5192511cdbd3"));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match tx hash";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // byte-reversed tx hash
    filter.insert(ParseHex("d3db1c519251eeecc76b3c68e550988290aa1d7c63d6b396b0ad069ebe987335"));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match manually serialized tx hash";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("30440220183e52d87de1bcb2a445d385bd4301e10c12aecae37624be8d63c4159dd08eca02206ead801d61dd08c57618e64a619f5aa756a91671efd10aadcf5fb6cb7995f29d01"));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match input signature";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbf"));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match input pub key";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("990176ee35e3f2d5dcf87f849d7c2722a52fd660"));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match output address";
    EXPECT_TRUE(filter.IsRelevantAndUpdate(spendingTx, spendingTx.GetHash())) << "Simple Bloom filter didn't add output";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256("0x0565cf0239f8172f6dcfd32526bede29dd7c65e7af4179cd61207ad78f2d1476"), 1));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match COutPoint";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256("0xbda053cffb2b80ae27542d93a1c40c677e777b87379905d7b68c599490f11f0f"), 1));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match COutPoint";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256("0x47f2490de69e04d2316ce03f7a3410837a85de960b93849f1c71e28da76bb842"), 1));
    EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match COutPoint";

    {
        filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
        COutPoint prevOutPoint(uint256("0x0565cf0239f8172f6dcfd32526bede29dd7c65e7af4179cd61207ad78f2d1476"), 1);
        {
            vector<unsigned char> data(32 + sizeof(unsigned int));
            memcpy(&data[0], prevOutPoint.hash.begin(), 32);
            memcpy(&data[32], &prevOutPoint.n, sizeof(unsigned int));
            filter.insert(data);
        }
        EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match manually serialized COutPoint";
    }


    {
        filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
        COutPoint prevOutPoint(uint256("0xbda053cffb2b80ae27542d93a1c40c677e777b87379905d7b68c599490f11f0f"), 1);
        {
            vector<unsigned char> data(32 + sizeof(unsigned int));
            memcpy(&data[0], prevOutPoint.hash.begin(), 32);
            memcpy(&data[32], &prevOutPoint.n, sizeof(unsigned int));
            filter.insert(data);
        }
        EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match manually serialized COutPoint";
    }


    {
        filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
        COutPoint prevOutPoint(uint256("0x47f2490de69e04d2316ce03f7a3410837a85de960b93849f1c71e28da76bb842"), 1);
        {
            vector<unsigned char> data(32 + sizeof(unsigned int));
            memcpy(&data[0], prevOutPoint.hash.begin(), 32);
            memcpy(&data[32], &prevOutPoint.n, sizeof(unsigned int));
            filter.insert(data);
        }
        EXPECT_TRUE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter didn't match manually serialized COutPoint";
    }

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256("00000009e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436"));
    EXPECT_FALSE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter matched random tx hash";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("0000006d2965547608b9e15d9032a7b9d64fa431"));
    EXPECT_FALSE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter matched random address";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256("0x90c122d70786e899529d71dbeba91ba216982fb6ba58f3bdaab65e73b7e9260b"), 1));
    EXPECT_FALSE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter matched COutPoint for an output we didn't care about";

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256("0x000000d70786e899529d71dbeba91ba216982fb6ba58f3bdaab65e73b7e9260b"), 0));
    EXPECT_FALSE(filter.IsRelevantAndUpdate(tx, tx.GetHash())) << "Simple Bloom filter matched COutPoint for an output we didn't care about";
}

TEST(bloom_tests, merkle_block_1)
{
    // block 94ba77c0d8a82eff26d12e4d551bd95fcd1212bf552c6e113aaf6b7d076ded2f
    std::string raw_block = "06000000025ed8783f8819e4a79c13fdbd19a0c2e8e11a6494877228c7f295a130799e77f3bb35c553013926977636d1df049cf1ef0795babef70a0e5d35c90622e0fa506f73435a5c35011d0000000007010000006f73435a010000000000000000000000000000000000000000000000000000000000000000ffffffff04039cad01ffffffff0100000000000000000000000000010000006f73435a01e6b6217bf9b3eb6295eb4c30599475a2e8b1400b6a1799845b809a314e211c190100000049483045022100b7ded9009cdab501b4ef9285872fab6a22196de75d0e9e6315ba0a9c7a1d2183022074e8d0fe2f981b4381b627d3590ecd9504ec7cdea0df190ebbda5fde1cef87fb01ffffffff02000000000000000000e5365aabb200000023210316f0ea8d277b6b771cc9bf5bb7e57b4c7ede9fde1175adadf1b4226b3aa400b3ac00000000010000004d73435a01d3db1c519251eeecc76b3c68e550988290aa1d7c63d6b396b0ad069ebe987335000000006b483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff02b592849f300000001976a91401854abf39d84762ebeeb332e3116be49f1c4dad88ac00e87648170000001976a91438f54f511df07214284de94d7cffda10b38004d988ac00000000010000002f73435a014fbb28867c487da9f0a9d3bbca92f78633f13147d66cd0ffe70c0b42ec92d5b1000000006b483045022100c8408d5b486e52fc195167abc1d9104521c27fde34809537cdb6642ce5d552c302206fe5db971a9b5523bb2c077b8d5544ad6fd60d1e998432198029be0b7088aa49012102d2277729673603bbe7a4bcbb25dca44fa33df181ac91bf8d72949e6970a701b5ffffffff0208028999000000001976a91481a30ef48add462cf3680d42db1ec55545ec265388ac0885e636000000001976a914d0fa16d2e97cd855f40e60eb92372d6f12af7a9c88ac0000000001000000cf72435a01315f26519ae722bf95a609ba2ef452874f0b197efb18fd1ec391c6e72d4a35a0000000006a47304402206c7147aed33ebd13339556c9a7200443d9f276bcfb618a5837d0fade730672af02207d936d3ae5dec97777f3caed97514f644f7cd50705a2409ffbdf1948c85f3baa012103c312dc2ada3230e975cc189ed52ed5291543e7ef1acd7e92cd687ae00bee7468ffffffff0270181922000000001976a91430fe08bbdad7b3f515a24326893577807663349d88acc0b6e804000000001976a914265f3c333eef8e8a3e352670e4260ff22de5c22088ac0000000001000000de72435a01004ffaed5d35b031db24f963fb31c8672010643a79891bb69825b82fdee419b1000000006a4730440220119a504a5f1f8bbac685004a60bdb17bedb659dc08f459c4aea09c49c4b0faf3022020abd94c40aa156c2e5d754ee11c86de0dfcb046693a79aad60654bd33cd34b20121030c753c1d9e7b95e9839421e09b31a702650a4590e860bce626cff60264f5aafdffffffff02d0cc3a1e000000001976a9149e2f86a3950b4e673d42d3bb0bd4131380e9f42188ac9024de03000000001976a9146cbb3d369c0b1a92613651af4935f12b61756b0888ac0000000001000000ee72435a01775b8a5162ce832946205dab9e9126177dd9bbba2052f688ed9b6e90ba1fda11000000006b4830450221008b3ca4a98d95db321568ca9c526353475925f44f8133cae912755fc21836f8430220164966c5ce1b32c0de3212a040a2c80408ebce5e67eded0d95ad6f19b93b36cd012103aa0a7ad5f30bd2f7bd872a32080ec3e57eb90d45e07c52c21f13fd64d8372cdaffffffff0210d2801a000000001976a9146b12765aa0037e96dd70f479a632dafb57e3155188acb0d3b903000000001976a91484c775d030a8a5b5b6ac89a9cf8999b449d6624f88ac00000000463044022027ca81a9753434f972ed663fb9c3932db6ded5795de711534d0be5590193b14f02205b7a87f6a167b267e5a86c385423b6705a05d4e4b49e3beb71a6480dcc9f126a";
    CDataStream stream(ParseHex(raw_block), SER_NETWORK, PROTOCOL_VERSION);
    CBlock block;
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256("0xee1e4abd0c0240aebf450eed3e575b26440ab4202cf54f7ba0b12d514ebf646b"));

    CMerkleBlock merkleBlock(block, filter);
    EXPECT_EQ(merkleBlock.header.GetHash(), block.GetHash());

    EXPECT_EQ(merkleBlock.vMatchedTxn.size(), (unsigned)1);

    EXPECT_EQ(merkleBlock.vMatchedTxn[0].second, uint256("0xee1e4abd0c0240aebf450eed3e575b26440ab4202cf54f7ba0b12d514ebf646b"));
    EXPECT_EQ(merkleBlock.vMatchedTxn[0].first, (unsigned)2);
}

TEST(bloom_tests, merkle_block_2_and_serialize)
{
    // block 94ba77c0d8a82eff26d12e4d551bd95fcd1212bf552c6e113aaf6b7d076ded2f
    std::string raw_block = "06000000025ed8783f8819e4a79c13fdbd19a0c2e8e11a6494877228c7f295a130799e77f3bb35c553013926977636d1df049cf1ef0795babef70a0e5d35c90622e0fa506f73435a5c35011d0000000007010000006f73435a010000000000000000000000000000000000000000000000000000000000000000ffffffff04039cad01ffffffff0100000000000000000000000000010000006f73435a01e6b6217bf9b3eb6295eb4c30599475a2e8b1400b6a1799845b809a314e211c190100000049483045022100b7ded9009cdab501b4ef9285872fab6a22196de75d0e9e6315ba0a9c7a1d2183022074e8d0fe2f981b4381b627d3590ecd9504ec7cdea0df190ebbda5fde1cef87fb01ffffffff02000000000000000000e5365aabb200000023210316f0ea8d277b6b771cc9bf5bb7e57b4c7ede9fde1175adadf1b4226b3aa400b3ac00000000010000004d73435a01d3db1c519251eeecc76b3c68e550988290aa1d7c63d6b396b0ad069ebe987335000000006b483045022100f06dc9beaca5ae1fceffdeb19cef066beb5cc8c3ef56061c16f53a086c52be420220603639d99a1da9f258b2a7875608258728a8ee0369c1d7701ccd5999cce32d980121027a4fab48b61923e4c18b1697b1c61481f1843e865d059a178e1940a7a4d10dbfffffffff02b592849f300000001976a91401854abf39d84762ebeeb332e3116be49f1c4dad88ac00e87648170000001976a91438f54f511df07214284de94d7cffda10b38004d988ac00000000010000002f73435a014fbb28867c487da9f0a9d3bbca92f78633f13147d66cd0ffe70c0b42ec92d5b1000000006b483045022100c8408d5b486e52fc195167abc1d9104521c27fde34809537cdb6642ce5d552c302206fe5db971a9b5523bb2c077b8d5544ad6fd60d1e998432198029be0b7088aa49012102d2277729673603bbe7a4bcbb25dca44fa33df181ac91bf8d72949e6970a701b5ffffffff0208028999000000001976a91481a30ef48add462cf3680d42db1ec55545ec265388ac0885e636000000001976a914d0fa16d2e97cd855f40e60eb92372d6f12af7a9c88ac0000000001000000cf72435a01315f26519ae722bf95a609ba2ef452874f0b197efb18fd1ec391c6e72d4a35a0000000006a47304402206c7147aed33ebd13339556c9a7200443d9f276bcfb618a5837d0fade730672af02207d936d3ae5dec97777f3caed97514f644f7cd50705a2409ffbdf1948c85f3baa012103c312dc2ada3230e975cc189ed52ed5291543e7ef1acd7e92cd687ae00bee7468ffffffff0270181922000000001976a91430fe08bbdad7b3f515a24326893577807663349d88acc0b6e804000000001976a914265f3c333eef8e8a3e352670e4260ff22de5c22088ac0000000001000000de72435a01004ffaed5d35b031db24f963fb31c8672010643a79891bb69825b82fdee419b1000000006a4730440220119a504a5f1f8bbac685004a60bdb17bedb659dc08f459c4aea09c49c4b0faf3022020abd94c40aa156c2e5d754ee11c86de0dfcb046693a79aad60654bd33cd34b20121030c753c1d9e7b95e9839421e09b31a702650a4590e860bce626cff60264f5aafdffffffff02d0cc3a1e000000001976a9149e2f86a3950b4e673d42d3bb0bd4131380e9f42188ac9024de03000000001976a9146cbb3d369c0b1a92613651af4935f12b61756b0888ac0000000001000000ee72435a01775b8a5162ce832946205dab9e9126177dd9bbba2052f688ed9b6e90ba1fda11000000006b4830450221008b3ca4a98d95db321568ca9c526353475925f44f8133cae912755fc21836f8430220164966c5ce1b32c0de3212a040a2c80408ebce5e67eded0d95ad6f19b93b36cd012103aa0a7ad5f30bd2f7bd872a32080ec3e57eb90d45e07c52c21f13fd64d8372cdaffffffff0210d2801a000000001976a9146b12765aa0037e96dd70f479a632dafb57e3155188acb0d3b903000000001976a91484c775d030a8a5b5b6ac89a9cf8999b449d6624f88ac00000000463044022027ca81a9753434f972ed663fb9c3932db6ded5795de711534d0be5590193b14f02205b7a87f6a167b267e5a86c385423b6705a05d4e4b49e3beb71a6480dcc9f126a";
    CDataStream stream(ParseHex(raw_block), SER_NETWORK, PROTOCOL_VERSION);
    CBlock block;
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the only transaction
    filter.insert(uint256("0xee1e4abd0c0240aebf450eed3e575b26440ab4202cf54f7ba0b12d514ebf646b"));

    CMerkleBlock merkleBlock(block, filter);
    EXPECT_EQ(merkleBlock.header.GetHash(), block.GetHash());

    ASSERT_EQ(merkleBlock.vMatchedTxn.size(), (unsigned)1);

    EXPECT_EQ(merkleBlock.vMatchedTxn[0].second, uint256("0xee1e4abd0c0240aebf450eed3e575b26440ab4202cf54f7ba0b12d514ebf646b"));
    EXPECT_EQ(merkleBlock.vMatchedTxn[0].first, (unsigned)2);

    vector<uint256> vMatched;
    EXPECT_EQ(merkleBlock.txn.ExtractMatches(vMatched), block.hashMerkleRoot);
    EXPECT_EQ(vMatched.size(), merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        EXPECT_EQ(vMatched[i], merkleBlock.vMatchedTxn[i].second);

    CDataStream merkleStream(SER_NETWORK, PROTOCOL_VERSION);
    merkleStream << merkleBlock;

    vector<unsigned char> vch = ParseHex(raw_block);
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    EXPECT_TRUE(std::equal(stream.begin(), stream.end(), expected.begin()));
}

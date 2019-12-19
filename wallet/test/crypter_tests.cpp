#include "googletest/googletest/include/gtest/gtest.h"

#include "base58.h"
#include "crypter.h"
#include "crypto_highlevel.h"
#include "transaction.h"
#include "util.h"

#include <boost/algorithm/hex.hpp>
#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

CTransaction TxFromHex_crypterTests(const std::string& hex)
{
    CDataStream  stream(ParseHex(hex), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;
    return tx;
}

TEST(crypter_tests, basic_tests)
{
    CCrypter                   crypter;
    std::string                toEncrypt("Hi, this is very secret!");
    std::vector<unsigned char> cipherText;
    SecureString               strWalletPassphrase("abc");
    CKeyingMaterial            decryptedText;
    std::string                salt("abcdefgh"); // length must be WALLET_CRYPTO_SALT_SIZE
    std::string                cipherHex;
    std::string expectedCipherHex = "44F5DFF1856115B80ED0E7359AACC2DDF4908787D891935C7E1469E29418C52E";
    // set encryption parameters
    EXPECT_TRUE(crypter.SetKeyFromPassphrase(
        strWalletPassphrase, std::vector<unsigned char>(salt.begin(), salt.end()), 100, 0));

    // encrypt
    EXPECT_TRUE(crypter.Encrypt(CKeyingMaterial(toEncrypt.begin(), toEncrypt.end()), cipherText));
    boost::algorithm::hex(cipherText.begin(), cipherText.end(), std::back_inserter(cipherHex));

    // make both serialized results same case for comparison
    std::transform(cipherHex.begin(), cipherHex.end(), cipherHex.begin(), ::toupper);
    std::transform(expectedCipherHex.begin(), expectedCipherHex.end(), expectedCipherHex.begin(),
                   ::toupper);

    // after ciphering, the string is not the same
    EXPECT_TRUE(std::equal(cipherHex.begin(), cipherHex.end(), expectedCipherHex.begin()));

    // decrypt
    EXPECT_TRUE(crypter.Decrypt(cipherText, decryptedText));
    std::string decryptedTextStr(decryptedText.begin(), decryptedText.end());
    EXPECT_EQ(decryptedTextStr, toEncrypt);
}

TEST(hash_calculator, sha256)
{
    Sha256Calculator calcSha256;
    calcSha256.push_data("Hello ");
    calcSha256.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcSha256.getHashAndReset()),
              "C0535E4BE2B79FFD93291305436BF889314E4A3FAEC05ECFFCBB7DF31AD9E51A");
    // to it again to ensure reset is working
    calcSha256.push_data("Hello ");
    calcSha256.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcSha256.getHashAndReset()),
              "C0535E4BE2B79FFD93291305436BF889314E4A3FAEC05ECFFCBB7DF31AD9E51A");
}

TEST(hash_calculator, sha1)
{
    Sha1Calculator calcSha;
    calcSha.push_data("Hello ");
    calcSha.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcSha.getHashAndReset()),
              "D3486AE9136E7856BC42212385EA797094475802");
    calcSha.push_data("Hello ");
    calcSha.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcSha.getHashAndReset()),
              "D3486AE9136E7856BC42212385EA797094475802");
}

TEST(hash_calculator, md5)
{
    Md5Calculator calcMd5;
    calcMd5.push_data("Hello ");
    calcMd5.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcMd5.getHashAndReset()), "86FB269D190D2C85F6E0468CECA42A20");
}

TEST(hash_calculator, sha512)
{
    Sha512Calculator calcSha512;
    calcSha512.push_data("Hello ");
    calcSha512.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcSha512.getHashAndReset()),
              "F6CDE2A0F819314CDDE55FC227D8D7DAE3D28CC556222A0A8AD66D91CCAD4AAD6094F517A2182360C9AACF6A3"
              "DC323162CB6FD8CDFFEDB0FE038F55E85FFB5B6");
}

TEST(hash_calculator, sha224)
{
    Sha224Calculator calcSha224;
    calcSha224.push_data("Hello ");
    calcSha224.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcSha224.getHashAndReset()),
              "7E81EBE9E604A0C97FEF0E4CFE71F9BA0ECBA13332BDE953AD1C66E4");
}

TEST(hash_calculator, sha384)
{
    Sha384Calculator calcSha384;
    calcSha384.push_data("Hello ");
    calcSha384.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcSha384.getHashAndReset()), "86255FA2C36E4B30969EAE17DC34C772CBEB"
                                                                   "DFC58B58403900BE87614EB1A34B8780263F"
                                                                   "255EB5E65CA9BBB8641CCCFE");
}

TEST(hash_calculator, ripemd160)
{
    Ripemd160HashCalculator calcRipemd160;
    calcRipemd160.push_data("Hello ");
    calcRipemd160.push_data("world!");
    EXPECT_EQ(boost::algorithm::hex(calcRipemd160.getHashAndReset()),
              "7F772647D88750ADD82D8E1A7A3E5C0902A346A3");
}

TEST(cryptography_tests, ecdh_test)
{
    std::function<void(EVP_PKEY*)> PkeyDeleter = [](EVP_PKEY* k) {
        if (!k)
            return;
        EVP_PKEY_free(k);
    };

    std::function<void(EC_KEY*)> EcKeyDeleter = [](EC_KEY* eckey) {
        if (!eckey)
            return;
        EC_KEY_free(eckey);
    };

    std::function<void(EC_POINT*)> EcPointDeleter = [](EC_POINT* ecpoint) {
        if (!ecpoint)
            return;
        EC_POINT_free(ecpoint);
    };

    std::function<void(BIGNUM*)> BnDeleter = [](BIGNUM* bn) {
        if (!bn)
            return;
        BN_free(bn);
    };

    std::function<void(BN_CTX*)> BnCtxDeleter = [](BN_CTX* bn) {
        if (!bn)
            return;
        BN_CTX_free(bn);
    };

    using EcKeyPtr   = std::unique_ptr<EC_KEY, decltype(EcKeyDeleter)>;
    using EcPointPtr = std::unique_ptr<EC_POINT, decltype(EcPointDeleter)>;
    using BnPtr      = std::unique_ptr<BIGNUM, decltype(BnDeleter)>;

    EcKeyPtr                   pkey1(EC_KEY_new_by_curve_name(NID_secp256k1), EcKeyDeleter);
    const EC_GROUP*            group1 = EC_KEY_get0_group(pkey1.get());
    EcPointPtr                 pub_key1(EC_POINT_new(group1), EcPointDeleter);
    std::vector<unsigned char> privkey1_bin = CHL::RandomBytes(32);
    BnPtr priv_key1 = BnPtr(BN_bin2bn(privkey1_bin.data(), privkey1_bin.size(), BN_new()), BnDeleter);
    EXPECT_NE(EC_POINT_mul(group1, pub_key1.get(), priv_key1.get(), nullptr, nullptr, nullptr), 0);
    EC_KEY_set_private_key(pkey1.get(), priv_key1.get());
    EC_KEY_set_public_key(pkey1.get(), pub_key1.get());

    EcKeyPtr                   pkey2(EC_KEY_new_by_curve_name(NID_secp256k1), EcKeyDeleter);
    const EC_GROUP*            group2 = EC_KEY_get0_group(pkey2.get());
    EcPointPtr                 pub_key2(EC_POINT_new(group2), EcPointDeleter);
    std::vector<unsigned char> privkey2_bin = CHL::RandomBytes(32);
    BnPtr priv_key2 = BnPtr(BN_bin2bn(privkey2_bin.data(), privkey2_bin.size(), BN_new()), BnDeleter);
    EXPECT_NE(EC_POINT_mul(group2, pub_key2.get(), priv_key2.get(), nullptr, nullptr, nullptr), 0);
    EC_KEY_set_private_key(pkey2.get(), priv_key2.get());
    EC_KEY_set_public_key(pkey2.get(), pub_key2.get());

    EXPECT_EQ(EC_GROUP_cmp(group1, group2, nullptr), 0);

    std::array<uint8_t, 32> sharedKey1;
    std::array<uint8_t, 32> sharedKey2;

    {
        int len = ECDH_compute_key(sharedKey1.data(), 32, pub_key2.get(), pkey1.get(), KDF_SHA256);
        ASSERT_EQ(len, SHA256_DIGEST_LENGTH);
    }
    {
        int len = ECDH_compute_key(sharedKey2.data(), 32, pub_key1.get(), pkey2.get(), KDF_SHA256);
        ASSERT_EQ(len, SHA256_DIGEST_LENGTH);
    }
    EXPECT_EQ(sharedKey1, sharedKey2);
}

TEST(cryptography_tests, ecdh_fromCKey)
{
    std::function<void(EC_KEY*)> EcKeyDeleter = [](EC_KEY* eckey) {
        if (!eckey)
            return;
        EC_KEY_free(eckey);
    };

    using EcKeyPtr = std::unique_ptr<EC_KEY, decltype(EcKeyDeleter)>;

    CKey k1;
    k1.MakeNewKey(true);
    EcKeyPtr        pkey_priv1 = k1.GetLowLevelPrivateKey();
    EcKeyPtr        pkey_pub1  = k1.GetLowLevelPublicKey();
    const EC_POINT* ecpoint1   = EC_KEY_get0_public_key(pkey_pub1.get());

    CKey k2;
    k2.MakeNewKey(true);
    EcKeyPtr        pkey_priv2 = k2.GetLowLevelPrivateKey();
    EcKeyPtr        pkey_pub2  = k2.GetLowLevelPublicKey();
    const EC_POINT* ecpoint2   = EC_KEY_get0_public_key(pkey_pub2.get());

    std::array<uint8_t, 32> sharedKey1;
    std::array<uint8_t, 32> sharedKey2;

    {
        int len = ECDH_compute_key(sharedKey1.data(), sharedKey1.size(), ecpoint1, pkey_priv2.get(),
                                   KDF_SHA256);
        ASSERT_EQ(len, SHA256_DIGEST_LENGTH);
    }
    {
        int len = ECDH_compute_key(sharedKey2.data(), sharedKey2.size(), ecpoint2, pkey_priv1.get(),
                                   KDF_SHA256);
        ASSERT_EQ(len, SHA256_DIGEST_LENGTH);
    }
    EXPECT_EQ(sharedKey1, sharedKey2);

    ////////////////////////// irrelevant to the derivation
    {
        // some comparison of points
        const EC_GROUP* group1 = EC_KEY_get0_group(pkey_priv1.get());
        const EC_GROUP* group2 = EC_KEY_get0_group(pkey_priv2.get());
        EXPECT_EQ(EC_GROUP_cmp(group1, group2, nullptr), 0);
        EXPECT_EQ(EC_POINT_cmp(group1, EC_KEY_get0_public_key(pkey_priv1.get()),
                               EC_KEY_get0_public_key(pkey_pub1.get()), nullptr),
                  0);
        EXPECT_EQ(EC_POINT_cmp(group1, EC_KEY_get0_public_key(pkey_priv2.get()),
                               EC_KEY_get0_public_key(pkey_pub2.get()), nullptr),
                  0);
    }
}

TEST(cryptography_tests, ecdh_fromCKey_HighLevel_ChosenKeys)
{
    std::string k1hex =
        "305402010104205F994E001DB6679A3E51B07DB00BA8DE2A085FFF9D4D8A0E6AACDD8F6C64391DA00706052B8104000"
        "AA12403220002C6E0B2B9211F3D8B677DBE435F7C365EBF53639F1761A5D7AA01BF02968E881F";
    std::string k2hex =
        "30540201010420F21B283C70F56F28CE71738E2D2E0B78BF7372E957270EE8D338737F2CFF3BBCA00706052B8104000"
        "AA12403220003C33BE206F9329B0F2302AB0DCD6A8AD58845132E1A0989C42AC2AB979D9B1589";

    std::string k1raw = boost::algorithm::unhex(k1hex);
    std::string k2raw = boost::algorithm::unhex(k2hex);

    CKey k1;
    k1.SetPrivKey(CPrivKey(k1raw.cbegin(), k1raw.cend()));
    CKey k2;
    k2.SetPrivKey(CPrivKey(k2raw.cbegin(), k2raw.cend()));

    std::array<uint8_t, 32> sharedKey1 = CHL::RandomBytesAs<std::array<uint8_t, 32>>();

    sharedKey1 = k1.GenerateSharedSecretFromThisPrivateKey(k2);

    std::array<uint8_t, 32> sharedKey2 = CHL::RandomBytesAs<std::array<uint8_t, 32>>();

    std::string sharedSecretHex = "5540734F33556194791AF1DE7C117014D0334D04FAE38DDD0057315C27937C27";

    sharedKey2 = k2.GenerateSharedSecretFromThisPrivateKey(k1);

    EXPECT_EQ(sharedKey1, sharedKey2);
    EXPECT_EQ(std::string(sharedKey1.cbegin(), sharedKey1.cend()),
              boost::algorithm::unhex(sharedSecretHex));
}

TEST(cryptography_tests, ecdh_fromCKey_HighLevel)
{
    CKey k1;
    k1.MakeNewKey(true);

    CKey k2;
    k2.MakeNewKey(true);

    std::array<uint8_t, 32> sharedKey1 = CHL::RandomBytesAs<std::array<uint8_t, 32>>();

    sharedKey1 = k1.GenerateSharedSecretFromThisPrivateKey(k2);

    std::array<uint8_t, 32> sharedKey2 = CHL::RandomBytesAs<std::array<uint8_t, 32>>();

    sharedKey2 = k2.GenerateSharedSecretFromThisPrivateKey(k1);

    EXPECT_EQ(sharedKey1, sharedKey2);
}

TEST(cryptography_tests, ecdh_fromCKeyShortened)
{
    std::function<void(EC_KEY*)> EcKeyDeleter = [](EC_KEY* eckey) {
        if (!eckey)
            return;
        EC_KEY_free(eckey);
    };

    using EcKeyPtr = std::unique_ptr<EC_KEY, decltype(EcKeyDeleter)>;

    CKey k1;
    k1.MakeNewKey(true);
    EcKeyPtr        pkey_pub1 = k1.GetLowLevelPublicKey();
    const EC_POINT* ecpoint1  = EC_KEY_get0_public_key(pkey_pub1.get());

    CKey                    k2;
    std::array<uint8_t, 32> sharedKey1;
    std::array<uint8_t, 32> sharedKey2;
    std::tie(k2, sharedKey1) = k1.GenerateEphemeralSharedSecretFromThisPublicKey();

    {
        // compute the shared secret the other way around
        int len = ECDH_compute_key(sharedKey2.data(), sharedKey2.size(), ecpoint1,
                                   k2.GetLowLevelPrivateKey().get(), KDF_SHA256);
        ASSERT_EQ(len, SHA256_DIGEST_LENGTH);
    }

    // both secrets should match
    EXPECT_EQ(sharedKey1, sharedKey2);
}

TEST(cryptography_tests, ecdh_get_scriptSig)
{

    std::string transaction =
        "010000001af29a5a012081139a3e0d764e9fb415bf1601c5bc24eba093c3f6a735aaa9d81d27d55dc5010000006"
        "b483045022100ea2baf384bb518ed939a1dfc02df634be2186c5e35d79a09fc7c1f1379987bc102200e286cc382"
        "9fbe574bda0cacfe8e918755574685bcb8af8a67b2d24f0087122d012103bd4c76349aae4b81011eddce127f36c"
        "ffd6b7beaf84c80d5d4e6cf06e5c8596cffffffff0310270000000000001976a9144e2a50f7e8c58ff9a0175f95"
        "616a1657b49a06a888ac1027000000000000456a434e5401014e4942424cab10c04e20e0aec73d58c8fbf2a9c26"
        "a6dc3ed666c7b80fef215620c817703b1e5d8b1870211ce7cdf50718b4789245fb80f58992019002019f0e073eb"
        "0b000000001976a9144e2a50f7e8c58ff9a0175f95616a1657b49a06a888ac00000000";
    CTransaction tx = TxFromHex_crypterTests(transaction);

    std::vector<std::vector<unsigned char>> stack;
    EXPECT_TRUE(EvalScript(stack, tx.vin[0].scriptSig, tx, 1, false, 0));

    boost::optional<CKey> pubKey = CTransaction::GetPublicKeyFromScriptSig(tx.vin[0].scriptSig);

    ASSERT_TRUE(pubKey.is_initialized());

    CBitcoinAddress addr(pubKey->GetPubKey().GetID());
    EXPECT_EQ(addr.ToString(), "NT3GauUgR5NyCY5z4hQ37A9ULXPSmYGzPL");
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_all_invalid_sizes)
{
    {
        // size 0 is invalid
        CHL::Bytes message;

        CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k =
            CHL::GenXSalsa20poly1305RandomKey();
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n = CHL::GenSalsa20poly1305RandomNonce();

        EXPECT_ANY_THROW(auto res = CHL::XSalsa20poly1305_EncryptBlock(message, n, k));
    }
    {
        // more than maximum size by 1
        CHL::Bytes random_message =
            CHL::RandomBytes(CHL::MAX_CRYPTOSECRETBOX_MSG_SIZE - crypto_secretbox_ZEROBYTES + 1);
        CHL::Bytes message(std::make_move_iterator(random_message.begin()),
                           std::make_move_iterator(random_message.end()));

        CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k =
            CHL::GenXSalsa20poly1305RandomKey();
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n = CHL::GenSalsa20poly1305RandomNonce();

        EXPECT_ANY_THROW(auto res = CHL::XSalsa20poly1305_EncryptBlock(message, n, k));
    }
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_all_possible_sizes)
{
    for (unsigned i = 1; i < CHL::MAX_CRYPTOSECRETBOX_MSG_SIZE - crypto_secretbox_ZEROBYTES; i++) {
        CHL::Bytes random_message = CHL::RandomBytes(i);
        CHL::Bytes message(std::make_move_iterator(random_message.begin()),
                           std::make_move_iterator(random_message.end()));

        CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k =
            CHL::GenXSalsa20poly1305RandomKey();
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n = CHL::GenSalsa20poly1305RandomNonce();

        EXPECT_EQ(message.size(), i);

        CHL::Bytes cipher = CHL::XSalsa20poly1305_EncryptBlock(message, n, k);
        EXPECT_EQ(cipher.size(), crypto_secretbox_MACBYTES + message.size());
        CHL::Bytes message_to_compare = CHL::XSalsa20poly1305_DecryptBlock(cipher, n, k);
        ASSERT_EQ(message_to_compare, message)
            << "; Failed to verify and decrypt message at size (" << i
            << ") and message: " << boost::algorithm::hex(std::string(message.cbegin(), message.cend()));
    }
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_select_messages_1)
{
    // test derived from: https://tweetnacl.js.org/#/secretbox
    std::string random_message = "xxxyyyzzz";
    CHL::Bytes  message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));

    std::string kStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef";
    std::string nStr = "abcdefghijklmnopqrstuvwx";

    ASSERT_EQ(kStr.size(), crypto_secretbox_KEYBYTES);
    ASSERT_EQ(nStr.size(), crypto_secretbox_NONCEBYTES);

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k;
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n;

    std::copy(kStr.begin(), kStr.end(), k.begin());
    std::copy(nStr.begin(), nStr.end(), n.begin());

    CHL::Bytes cipher = CHL::XSalsa20poly1305_EncryptBlock(message, n, k);
    EXPECT_EQ(boost::algorithm::hex(std::string(cipher.begin(), cipher.end())),
              "34A05A7D660A969508AC6558EE64D9C52E18F65BAE61C27912");
    EXPECT_EQ(cipher.size(), crypto_secretbox_MACBYTES + message.size());
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_select_messages_2)
{
    // test derived from: https://tweetnacl.js.org/#/secretbox
    std::string random_message = "aaabbbccc";
    CHL::Bytes  message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));

    std::string kStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF";
    std::string nStr = "ABCDEFghijklmnopqrstuvwx";

    ASSERT_EQ(kStr.size(), crypto_secretbox_KEYBYTES);
    ASSERT_EQ(nStr.size(), crypto_secretbox_NONCEBYTES);

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k;
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n;

    std::copy(kStr.begin(), kStr.end(), k.begin());
    std::copy(nStr.begin(), nStr.end(), n.begin());

    CHL::Bytes cipher = CHL::XSalsa20poly1305_EncryptBlock(message, n, k);
    EXPECT_EQ(boost::algorithm::hex(std::string(cipher.begin(), cipher.end())),
              "035484A7781A1588D341BFB7551140F82C7E83037A9BE83AC9");
    EXPECT_EQ(cipher.size(), crypto_secretbox_MACBYTES + message.size());
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_select_messages_3)
{
    // test derived from: https://tweetnacl.js.org/#/secretbox
    std::string random_message = "abcabcabc";
    CHL::Bytes  message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));

    std::string kStr = "abcdefGHIJKLMNOPQRSTUVWXYZABCDEF";
    std::string nStr = "ABCDEFghijklmnopqrSTUVWX";

    ASSERT_EQ(kStr.size(), crypto_secretbox_KEYBYTES);
    ASSERT_EQ(nStr.size(), crypto_secretbox_NONCEBYTES);

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k;
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n;

    std::copy(kStr.begin(), kStr.end(), k.begin());
    std::copy(nStr.begin(), nStr.end(), n.begin());

    CHL::Bytes cipher = CHL::XSalsa20poly1305_EncryptBlock(message, n, k);
    EXPECT_EQ(boost::algorithm::hex(std::string(cipher.begin(), cipher.end())),
              "A5E8AEDC3D988E98C93ADAD78B44920307525B7126B63111AE");
    EXPECT_EQ(cipher.size(), crypto_secretbox_MACBYTES + message.size());
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_select_messages_boundary)
{
    // test derived from: https://tweetnacl.js.org/#/secretbox
    std::string random_message =
        "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";
    CHL::Bytes message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));

    std::string kStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF";
    std::string nStr = "ABCDEFghijklmnopqrstuvwx";

    ASSERT_EQ(kStr.size(), crypto_secretbox_KEYBYTES);
    ASSERT_EQ(nStr.size(), crypto_secretbox_NONCEBYTES);

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k;
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n;

    std::copy(kStr.begin(), kStr.end(), k.begin());
    std::copy(nStr.begin(), nStr.end(), n.begin());

    CHL::Bytes cipher = CHL::XSalsa20poly1305_EncryptBlock(message, n, k);
    EXPECT_EQ(
        boost::algorithm::hex(std::string(cipher.begin(), cipher.end())),
        "AACD353F8F890B738B3D8E919A35C3601427D52454B3E914CF97455BF21386B9CE6CBED372E2FCABA72978FFE77F149"
        "87C37C16C2C6850AAA3C0A9509D3D773DB2ABAC47108BDFA16437F3F84E49C9FE83D60E3C54675255616C97B97CA0CA"
        "DC4FBA91875D49B91B639CBEE3D2CE61788AAE920BEAE55B617C7BBF2822749845A1745CAB060C0E1361301A8CA2CC7"
        "22BD2DBF3B488B8708B8F7E0C542E1CAAB5B933B374E99B1D08B113F0E1ABBF497355B86E611C8A793EBF9AC00DB87C"
        "55DC21F824E87298915C13068090193D7051AE9744DD7E4B4AC9A865C6D1919503E2F64490F2CE8E178C49749245E45"
        "558105D497AE42D589B11722EA9D0923AA42861D881D9DD90D49EDE961CF083E0D6411F887B6201A2F5E5C511B1769B"
        "AB4C1B50FF88079720A62B4550D2CBE6A7D8ABB24AA2127A26D0A8743E4009B95B9398EB64C77F2FBB8621B048F7CB1"
        "ACA7C1297341F995F5E587030DB13905CE7EFB42F4FBA5A22F4E53F84517B391F975CB44BB0748620E7384EDB92A3C4"
        "DC491DA9F4B8DEB8590F7C3B8A396981F6E26BC9960840B238FF9F002D759836EFBF99E6DE2B1FDB57B5E27B00E74DE"
        "DA2E2CFE49F26DA83153CEA8C843CF104784B3AA02052024EABC865D8153E530EA389AC79427BE1586AFAC98755A79D"
        "6A727084CBB4239C776CD9E7A92B56CA445D0F685CED2E06FFB48C5A1552AC55ED2EA9E90A909D79AF9F54915320F2E"
        "10CD2E64A87E140385C220CF38734C2CF29B9E7C648997824318AD2609AAB5179DCB2261525F6E06AD0279BC87DD1D6"
        "C0EC6C42ABF0DE2967957B3F82B86D668ECADDEB458BDFC7BAA9CA5D04D179233D3319384B1E1BED68F148EFA433D87"
        "B646BCEE3CD670483AAC22D5702493FA8EEA977BA517A7029FEAEFF4B3391B2193097FBE3BD782A1F421F95D8906460"
        "9EA44F41B8650552781ACFEEEBBC4E08EBAEF0D8AF18F445246CAB342FB3CC599EF3EE4E57E14DF651B304F56BF54D2"
        "5A2B9509005FCF7DD7DB5C671547FBC12BB6477A91E2B7B36CFDE578BA7395DA8C73E146F2B09766E4874E44CC8F5CF"
        "A96BE0B861ECFEAEE891511D360FC536B792BB9C1579DD479FCEF1F1E7C57EC62CF9AFE5B52FB0DABE80D4D45A9AC3A"
        "17A09F862D0C2C566E483BD3EB8672A2FF7116DDF93993ED7C0CBFF6587DCF9F0ED06813509203CA679635895BF77DC"
        "47F1EA0AB528F1F08201F629EBE96D49F91495DF9B2373F00799BC4172D953B16D60B056934E64E86C12C3B79E16409"
        "C0C43A87D0512F5B9882E5D3A6FB78799389B302F814896FE45529B72CE4918BA092EB75D258C057BC1FB2625B18C72"
        "6E7119D3B67602FD05C30FA1CEA017AD9E9CF69ECDE270C4B90D6BE47A5F5700B0F6EEFB632046F4A4400CC72B666DF"
        "316B6216B343638232C7E8FB9F816FC9BDCA4432E7D2CDF6324A49A0855911DE86054566D4142519A0778D6A991835D"
        "5DA96CC71AE0FA24B3BCAE1E09A4F4C244357E2D60E0D17E58EB3F97850A59EF32DEF7E50F35AD05787C42F9B44A3FA"
        "32CEF097F745C1609C4CF12654A2E8F2FF7F277328358E01A5FA0E3787F57F75DDE023F08C3FCF0F61AF1BA1C85DC4C"
        "A62B23975E8452A5AB00D186498C250EC9B964B6D8D3DBB6CD22BD45AA4A3F42B950CABFC86CFB12CE1015CC32BDFB5"
        "CEE7D1EEE7E957D26814032C9162BF313D75C9C421DB569B645EDF2468C944761129EB6B61F9B8DFA50BC61BD3596E4"
        "C807D738955A7869C25F3724670D0E528728B74D9E136C067EE70F06909A4DB1817A2AC2FCB9B34F26639165DC70A72"
        "289127F4A0501A9993329FA249330848AFFE8F525FB6C3E271D40D36EEC30C110371B9445B5D6B5C2BC2E4B1A3CDDC3"
        "72398CA9DB6DD74732034060106EFBD5C14DB760DD2ECE1E4145DA3A5DA9C8D6221F4F16B5EC0C85AD3BBC1179B6BD0"
        "2238437AC4B1B23BB1ECEDD204CFEB51434A693A35D861CDD01D187B3B5EC2D5049FC013C3ECA3CECE96E082032E1D7"
        "4E7CE60B5400D77E69B3BE40AA2B7490DFBAA5849992BD2FD7B6E4F3BA328AE7ECF30B05924B8E621FE1CDE5C8B2B92"
        "0F8576235985D1C511D9118CABD6DCBF08A335F7FD0282B67A7BF4250429E108D17FAA79C949746220B3B5A5DE15D98"
        "97CCD4B349E0BB954103F2232CA014DD9DAD8EFC00DFA64582F1305A2784F31BC392B12B502A8CD5BB940A59653FDCE"
        "F178DF07917CBFCEC1A351911043CD9AE385AC0E5AB3F2D8FB47A45438CDD04E4C08C41E0F69129CE6A4EE0A7D53D47"
        "BE237ED4237271A0EFE9DD33FCAC3A2C333F1A92475FCA31CC639A14316B04F70F133BFDB0A5BAAE94D5BFBD55E0631"
        "3F725DCB47E4BCFF96692B62ADC6DFB5B018C82989742197A189A6B1EA567CBDEE0CCD9DADD9844713DA105FA9D8B0E"
        "A4751433FF1916C23FD75F26423C2A124D7B581E53F764256C4FDFB244B2A8699ADFE6ECF1DF9772E533C9332714B60"
        "579D8A89C8AD18A3F37E52D710CB9B73FABD8BE95BE74F8A769D906EC5638574BDC311F1B20849AE1AC80F7234501B3"
        "16E81B740CCCDBB9CC13FFA0A345D6AF899A4052FC3A900D3EC47D46082289ABA097746DC20D36E63831FAD0D3ACFD0"
        "F7F715ED471BBF486C12B45BEF9CB7027456930517FB440F0FD3DFE5D71C383C3458083FCC72D20789CCFDFFDB5156D"
        "9BC670B77644CBAEB492542DB31DD6F2306E9EFC6C1944166E8C4A65EA7CAA3159995FA05B7D748A2B5E541E9438F82"
        "BF2A760E37BA1E5B8C29C38E93C614C8937265BA6B2BE22167C1FEC0FCC00D2D6679C33528B0FE7C1247AB9B35B61CD"
        "062A42DE3187D3CF278C966CDCAFF8831D2815E9D3D0BEA9334CA8750670BB8914E41F3E325717B84BD664933E2C941"
        "66D140BAC102749C66C071CC04A8D4544C84BAD467ABC8E812748ABAEB2937E6D4A91FD753127BCA28946A0DF795E77"
        "31274C8E61962732E6D77D4CEE25BA6386E6290738653985A2DCCA8760C0CE551C6242E1999D8FA058F1934BF676E9B"
        "887096315DB9AC665FB1519D17630785B77E37A48C12A95BD8989D8A267DAEED5DC1F28C996D1281424EE86F2C371B3"
        "1B1A42670C44A5C0F67C19C0E39D4239328E3BA582503DB48529F2A7EB629FCBA30ADFF90E0F2E74C4CFC3DA15D1AAD"
        "2A02C5C9D3C0AC003E8DE1D0BCEC356CABF9F052696A92BBE50C5CAB36F4F5461A0D8A5DF502AB66DD3B5936361B169"
        "0B82308AB50382FBE06F15A13E2A89632F93403E03428C141EA2E596FF8C54A0A14EF374EBB2E11A490512C32DB2A2E"
        "83BF0EA94FC6A91E5FFAC5ADD4A49DE5724508FAD224A049EE2F6374B6DA8C71D6FE1986B9EC2EC109E40E1855070AE"
        "85341918C53227C99AC9A7F66BFADADF4643161CF0454F626602D30FEE9201925F2CE266F8661517D4DF05083879A97"
        "20EB51AD39E7B96144C1120E86DDB32C919FD1B29D2D7EE44AB8E22728C8DC1F6179D494D3281B60AC0676E1C8D36A8"
        "3F571B6C7E94075D77EA373E9C1F412C8D069B8FD01EF72C10B57A3C82BB2DF4E9BF71EB436983AC3A482F753261B47"
        "06CAB6488DE3BE873C3F391941C94AEBA6E42F061F0F4AD883F8240BF3CC1F4CBBA9101B77E4CA2F64B89168BFEB2DC"
        "4CD9CDB5436160B7DF08DA1F8C00F14394A6E40503737F0F8512C79B70321F692DFDE9F3DC4DD75D01E3D39178C1C57"
        "C119182E360C90EFEF8F15A708E86EEA2226E14EBE67270E70E78155768D27D153146F45CD84ED54FE2696D6385B417"
        "123168B7EA017EDF093ABDFF136FB517E7B57272380AF69C96B4BE481FF842F1B24A2A298E4A9717AC68A8DBEAD2C84"
        "D7002F72662FC6F7DC3A7825949CC34188BB28D5622D2ED29916C375D90D068DDA9BD3D4B393E9481A43CCC9B79ACFF"
        "36942D46BEE2948C67B379F1FE3A0704DFA85C739E33D91F245A29A6BA8D1166E5CC82B589217A9940B4E0ACEA6CD98"
        "B56403417712BB2E4188718BBA8C805D2A97CE10FBA8EC355D70404748646A26D09CE04E0699D56801DBB0D4EBA7CA8"
        "A1CBE07E2B82219002FE04C443D23196A048D632E9DE1188F468D3F71825B8B3B7264510E25C7F3EA5ED429DDBCBBEF"
        "5327641723A12413742E01141F9FFDEB162B96DDE6C264CD183FCD9B58F292C94B67E42843C7FB47F10C81AE0AE96B4"
        "81898F2707265BB4B7D4E6E3494C0A482C6A6B1249695DC0F99DF91F878105CC7734A650C3F83D845C47A8AACE69645"
        "2C0878BBCCDC22C567DC797E1F71A00885B19B35F5CFCFFC5676F54D90D06738EA81BBFB8C3F7EC15B8555E0BD3F7D1"
        "4AF2621F91CAF4737CC3BA8E1A3597678885FB050EF965448806CF48D89EC964AA8726B8A7EF56C576682E6FDD3B8CE"
        "7542403AA519B10306F64D1279A21E33A0827B7EAE3CF62623BCE38DAA47F2FA4F6654A632B0098DAD9D1D00A859D83"
        "6F38ADFD7568A33985BBD694537325B95FD8D2CEB2F78865771F57E8AA0AE95CA6E8F475F4A83DCB759A6CD509DC66A"
        "81F151807E641E1F0F0EBE8C72CFE1C612DB9A3C4A1413C0FBD3813A02F7000955AEBC76D28057E0AD0255FB8362B01"
        "1BAB2739E75C9A07CBA4076E28964BBF90E144EA9B511C5EAB016B2856F0DA30394B0CE33991C71FB0F45333C17ECF6"
        "C9C61C9436E80A184D66A21164F5F04DEA1AFD10C046CD66E5DDD81C11EBDE9521FBB46D6E7E383E5C139A35FC97C65"
        "F3D4C24611B9E6164956A9B16BDBCACFCEEA8617266BF9C42F80C294BFA5F6C7AC56CF706794DD70B88626330BC78F1"
        "124A2DC028208199244B511E5B20786F63DDA40DD529AE77F2D2D8A672E99ADE00E5FC7292AFEDE3A3016C4A091CBC4"
        "597265BFC5A15BB050DFF4E4CEC07D734EF582AAD7D705A3A2392A00330843D443F3DACEE0D2F629582533ADEFADBBE"
        "62813144226788C8EDAC0F3184713AC2C49E85F6EBC9F5F8B6F21008D70463209299C3B35532B44891E89F38C195962"
        "6F5E08C1E5AAFF207406D7CB9CDC168103B92E2865046F4A187F07E3A81CCD7E78C19911DE862C3C2C088BE7643045B"
        "5B9C12B39A591239027E33254C2D813E45CAF459D121E5FE4345EDD7E43B5E5187096E60FA4EEAED1AC8933F2E2616C"
        "BB981ECDEA6D10BB87055D79795ED69381262502445E634FE0974AB130D6482B9B70B742FDB5AF38912C8482597EDC1"
        "40842BD6FFCBF3A9859FBE760A9DEBC0B3AAB76B078EEDEB660CDF197AC94565DCFD95C840C4610F82832C7E5D14DD6"
        "608B5C953366806CA5A615E52289F251FAF048D46B4433FA48A4BC2C7588398479390D61E9F4290F8CB0B08D537AC2A"
        "980ACEEF56B5514FF7C0EB37EB6A97941D15B3ED7B8424C8C715FAAFC5970390B4197259C77797E0E8ADA5D6B899E60"
        "E604FC790B8520E1A90968F9BE0B3EE87761A6A615E3EAACFEFA7B146DA6ABB8434B0A38918436D1AC692CCE5BDA8D6"
        "D89EA76F94AC7DE63F65561F8BB0B64CB12486459454027697027E456804F66663286E89298BF601400EB691F9A04FB"
        "34A9BE52E032FC04321F94F23A515864C3634F17BB813886A13FB8008CC0365ADCC5C219F482665987E599A9613AA28"
        "FBC77D8E4E24E6F472972F7AF927703F5F8423A0AC1FB511EEA2D26FE6AE1A37D8061D4F09F2D63003ABC72972B5946"
        "2857D5DC4CCA7E0117D622C3F6418E4B0D0624F3DCD22ED879A99BE756A99A54DFA692BFFC988A1E0B23CA5CC9EC141"
        "B0FCFA4B05800222D1E2E006FC93D76E7F546AD3601016819EFF759C31FC79A9B75EE5A053AA226E9A104");
    EXPECT_EQ(cipher.size(), crypto_secretbox_MACBYTES + message.size());
}

TEST(cryptography_tests, nonce_incrementor_compare_to_sodium_increment)
{
    {
        // 0xFFFFFF... + 1 (unsigned char)
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n1;
        std::memset(n1.data(), 0xFF, n1.size());
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n_expect = n1;
        EXPECT_EQ(n1, n_expect);
        SwapEndianness(n_expect);
        CHL::IncrementNonce(n1);
        EXPECT_NE(n1, n_expect);
        sodium_increment(n_expect.data(), n_expect.size());
        SwapEndianness(n_expect);
        EXPECT_EQ(n1, n_expect);
    }
    {
        // 0x00000... + 1 (unsigned char)
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n1;
        std::memset(n1.data(), 0, n1.size());
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n_expect = n1;
        EXPECT_EQ(n1, n_expect);
        SwapEndianness(n_expect);
        CHL::IncrementNonce(n1);
        EXPECT_NE(n1, n_expect);
        sodium_increment(n_expect.data(), n_expect.size());
        SwapEndianness(n_expect);
        EXPECT_EQ(n1, n_expect);
    }
    {
        // 0xFFFFF...FFE + 1 (unsigned char)
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n1;
        std::memset(n1.data(), 0xFF, n1.size());
        n1[crypto_secretbox_NONCEBYTES - 1]                             = 0xFE;
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n_expect = n1;
        EXPECT_EQ(n1, n_expect);
        SwapEndianness(n_expect);
        CHL::IncrementNonce(n1);
        EXPECT_NE(n1, n_expect);
        sodium_increment(n_expect.data(), n_expect.size());
        SwapEndianness(n_expect);
        EXPECT_EQ(n1, n_expect);
    }
    {
        // 0x000000..FFFFFFF + 1 = 0x000000...1...00000000 (unsigned char)
        // i is the number of bytes that are set
        for (unsigned i = 1; i < crypto_secretbox_NONCEBYTES - 1; i++) {
            std::array<unsigned char, crypto_secretbox_NONCEBYTES> n;
            static_assert(crypto_secretbox_NONCEBYTES % 2 == 0, "");
            std::memset(n.data(), 0, n.size());
            std::memset(n.data() + n.size() - i, 0xFF, i);
            std::array<unsigned char, crypto_secretbox_NONCEBYTES> n_expect = n;
            EXPECT_EQ(n, n_expect);
            SwapEndianness(n_expect);
            CHL::IncrementNonce(n);
            EXPECT_NE(n, n_expect);
            sodium_increment(n_expect.data(), n_expect.size());
            SwapEndianness(n_expect);
            EXPECT_EQ(n, n_expect);
        }
    }
    {
        // 0x000000..FFFFFFF + 1 = 0x000000...1...00000000
        // i is the number of bytes that are set (char)
        for (unsigned i = 1; i < crypto_secretbox_NONCEBYTES - 1; i++) {
            std::array<uint8_t, crypto_secretbox_NONCEBYTES> n;
            static_assert(crypto_secretbox_NONCEBYTES % 2 == 0, "");
            std::memset(n.data(), 0, n.size());
            std::memset(n.data() + n.size() - i, 0xFF, i);
            std::array<unsigned char, crypto_secretbox_NONCEBYTES> n_expect = n;
            EXPECT_EQ(n, n_expect);
            SwapEndianness(n_expect);
            CHL::IncrementNonce(n);
            EXPECT_NE(n, n_expect);
            sodium_increment(n_expect.data(), n_expect.size());
            SwapEndianness(n_expect);
            EXPECT_EQ(n, n_expect);
        }
    }
}

TEST(cryptography_tests, nonce_incrementor_random)
{
    static constexpr const int TESTS_COUNT = 1e5;
    for (int i = 0; i < TESTS_COUNT; i++) {
        std::array<uint8_t, crypto_secretbox_NONCEBYTES> n =
            CHL::RandomBytesAs<std::array<uint8_t, crypto_secretbox_NONCEBYTES>>();
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n_expect = n;
        EXPECT_EQ(n, n_expect);
        // sodium_increment incremenets in little_endian mode
        SwapEndianness(n_expect);
        CHL::IncrementNonce(n);
        EXPECT_NE(n, n_expect);
        sodium_increment(n_expect.data(), n_expect.size());
        SwapEndianness(n_expect);
        EXPECT_EQ(n, n_expect);
    }
}

TEST(cryptography_tests, nonce_incrementor)
{
    {
        // 0xFFFFFF... + 1 (unsigned char)
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n1;
        std::memset(n1.data(), 0xFF, n1.size());
        CHL::IncrementNonce(n1);
        EXPECT_TRUE(std::all_of(n1.cbegin(), n1.cend(), [](unsigned char c) { return c == 0; }));
    }
    {
        // 0x00000... + 1 (unsigned char)
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n1;
        std::memset(n1.data(), 0, n1.size());
        CHL::IncrementNonce(n1);
        EXPECT_TRUE(std::all_of(n1.cbegin(), n1.cend() - 1, [](unsigned char c) { return c == 0; }));
        EXPECT_EQ(n1[crypto_secretbox_NONCEBYTES - 1], 1);
    }
    {
        // 0xFFFFF...FFE + 1 (unsigned char)
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n1;
        std::memset(n1.data(), 0xFF, n1.size());
        n1[crypto_secretbox_NONCEBYTES - 1] = 0xFE;
        CHL::IncrementNonce(n1);
        EXPECT_TRUE(std::all_of(n1.cbegin(), n1.cend(), [](unsigned char c) { return c == 0xFF; }));
    }
    {
        // 0x000000..FFFFFFF + 1 = 0x000000...1...00000000 (unsigned char)
        // i is the number of bytes that are set
        for (unsigned i = 1; i < crypto_secretbox_NONCEBYTES - 1; i++) {
            std::array<unsigned char, crypto_secretbox_NONCEBYTES> n;
            static_assert(crypto_secretbox_NONCEBYTES % 2 == 0, "");
            std::memset(n.data(), 0, n.size());
            std::memset(n.data() + n.size() - i, 0xFF, i);
            CHL::IncrementNonce(n);
            EXPECT_TRUE(std::all_of(n.cbegin(), n.cbegin() + n.size() - i - 1,
                                    [](unsigned char c) { return c == 0; }));
            EXPECT_EQ(n[crypto_secretbox_NONCEBYTES - i - 1], 1); // the one bit that is == 1
            EXPECT_TRUE(std::all_of(n.cbegin() + n.size() - i + 1, n.cend(),
                                    [](unsigned char c) { return c == 0; }));
        }
    }
    {
        // 0x000000..FFFFFFF + 1 = 0x000000...1...00000000
        // i is the number of bytes that are set (char)
        for (unsigned i = 1; i < crypto_secretbox_NONCEBYTES - 1; i++) {
            std::array<uint8_t, crypto_secretbox_NONCEBYTES> n;
            static_assert(crypto_secretbox_NONCEBYTES % 2 == 0, "");
            std::memset(n.data(), 0, n.size());
            std::memset(n.data() + n.size() - i, 0xFF, i);
            CHL::IncrementNonce(n);
            EXPECT_TRUE(
                std::all_of(n.cbegin(), n.cbegin() + n.size() - i - 1, [](char c) { return c == 0; }));
            EXPECT_EQ(n[crypto_secretbox_NONCEBYTES - i - 1], 1); // the one bit that is == 1
            EXPECT_TRUE(
                std::all_of(n.cbegin() + n.size() - i + 1, n.cend(), [](char c) { return c == 0; }));
        }
    }
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_long_message_basic)
{
    std::string messageStr("abc");
    CHL::Bytes  message(std::make_move_iterator(messageStr.begin()),
                       std::make_move_iterator(messageStr.end()));

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k = CHL::GenXSalsa20poly1305RandomKey();
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n = CHL::GenSalsa20poly1305RandomNonce();

    CHL::Bytes cipher             = CHL::XSalsa20poly1305_EncryptLongMsg_CTR(message, n, k);
    CHL::Bytes message_to_compare = CHL::XSalsa20poly1305_DecryptLongMsg_CTR(cipher, k);

    EXPECT_EQ(message_to_compare, message);
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_long_message_empty)
{
    std::string messageStr;
    CHL::Bytes  message(std::make_move_iterator(messageStr.begin()),
                       std::make_move_iterator(messageStr.end()));

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k = CHL::GenXSalsa20poly1305RandomKey();
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n = CHL::GenSalsa20poly1305RandomNonce();

    EXPECT_ANY_THROW(auto res = CHL::XSalsa20poly1305_EncryptLongMsg_CTR(message, n, k));
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_long_message_two_packets)
{
    CHL::Bytes random_message = CHL::RandomBytes(5000);
    CHL::Bytes message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k = CHL::GenXSalsa20poly1305RandomKey();
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n = CHL::GenSalsa20poly1305RandomNonce();

    CHL::Bytes cipher             = CHL::XSalsa20poly1305_EncryptLongMsg_CTR(message, n, k);
    CHL::Bytes message_to_compare = CHL::XSalsa20poly1305_DecryptLongMsg_CTR(cipher, k);

    EXPECT_EQ(message_to_compare, message);
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_long_message_three_packets)
{
    CHL::Bytes random_message = CHL::RandomBytes(10000);
    CHL::Bytes message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k = CHL::GenXSalsa20poly1305RandomKey();
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n = CHL::GenSalsa20poly1305RandomNonce();

    CHL::Bytes cipher             = CHL::XSalsa20poly1305_EncryptLongMsg_CTR(message, n, k);
    CHL::Bytes message_to_compare = CHL::XSalsa20poly1305_DecryptLongMsg_CTR(cipher, k);

    EXPECT_EQ(message_to_compare, message);
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_long_message_all_sizes)
{
    for (int i = 1; i < 17000; i++) {
        CHL::Bytes random_message = CHL::RandomBytes(i);
        CHL::Bytes message(std::make_move_iterator(random_message.begin()),
                           std::make_move_iterator(random_message.end()));

        CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k =
            CHL::GenXSalsa20poly1305RandomKey();
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> n = CHL::GenSalsa20poly1305RandomNonce();

        EXPECT_EQ(message.size(), i);

        CHL::Bytes cipher             = CHL::XSalsa20poly1305_EncryptLongMsg_CTR(message, n, k);
        CHL::Bytes message_to_compare = CHL::XSalsa20poly1305_DecryptLongMsg_CTR(cipher, k);

        EXPECT_EQ(message_to_compare, message)
            << "; Failed to verify and decrypt chained message at size (" << i
            << ") and message: " << boost::algorithm::hex(std::string(message.cbegin(), message.cend()));
    }
}

TEST(cryptography_tests, crypto_secretbox_salsa20poly1305_long_message_cropped_sizes)
{
    CHL::Bytes random_message = CHL::RandomBytes(4045);
    CHL::Bytes message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));

    CHL::SecureArray<unsigned char, crypto_secretbox_KEYBYTES> k = CHL::GenXSalsa20poly1305RandomKey();
    std::array<unsigned char, crypto_secretbox_NONCEBYTES>     n = CHL::GenSalsa20poly1305RandomNonce();

    CHL::Bytes cipher = CHL::XSalsa20poly1305_EncryptLongMsg_CTR(message, n, k);
    EXPECT_NO_THROW(auto r = CHL::XSalsa20poly1305_DecryptLongMsg_CTR(cipher, k));
    // crop the cipher and test
    for (int i = cipher.size() - 1; i >= 0; i--) {
        cipher.resize(i);
        EXPECT_ANY_THROW(auto r = CHL::XSalsa20poly1305_DecryptLongMsg_CTR(cipher, k))
            << "Size " << i << " didn't throw an exception";
    }
}

TEST(cryptography_tests, serialize)
{
    // unsigned
    {
        uint32_t n1   = 0x12345678;
        auto     data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0x78);
        EXPECT_EQ(data[1], 0x56);
        EXPECT_EQ(data[2], 0x34);
        EXPECT_EQ(data[3], 0x12);
        EXPECT_EQ(data.size(), 4);
        uint32_t n2 = CHL::DeserializeSimple<uint32_t>(data);
        EXPECT_EQ(n1, n2);
    }
    {
        uint16_t n1   = 0x1234;
        auto     data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0x34);
        EXPECT_EQ(data[1], 0x12);
        EXPECT_EQ(data.size(), 2);
        uint16_t n2 = CHL::DeserializeSimple<uint16_t>(data);
        EXPECT_EQ(n1, n2);
    }
    {
        uint64_t n1   = 0x123456789ABCDEF0;
        auto     data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0xF0);
        EXPECT_EQ(data[1], 0xDE);
        EXPECT_EQ(data[2], 0xBC);
        EXPECT_EQ(data[3], 0x9A);
        EXPECT_EQ(data[4], 0x78);
        EXPECT_EQ(data[5], 0x56);
        EXPECT_EQ(data[6], 0x34);
        EXPECT_EQ(data[7], 0x12);
        EXPECT_EQ(data.size(), 8);
        uint64_t n2 = CHL::DeserializeSimple<uint64_t>(data);
        EXPECT_EQ(n1, n2);
    }
    // signed positive
    {
        int32_t n1   = 0x12345678;
        auto    data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0x78);
        EXPECT_EQ(data[1], 0x56);
        EXPECT_EQ(data[2], 0x34);
        EXPECT_EQ(data[3], 0x12);
        EXPECT_EQ(data.size(), 4);
        int32_t n2 = CHL::DeserializeSimple<int32_t>(data);
        EXPECT_EQ(n1, n2);
    }
    {
        int16_t n1   = 0x1234;
        auto    data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0x34);
        EXPECT_EQ(data[1], 0x12);
        EXPECT_EQ(data.size(), 2);
        int16_t n2 = CHL::DeserializeSimple<int16_t>(data);
        EXPECT_EQ(n1, n2);
    }
    {
        int64_t n1   = 0x123456789ABCDEF0;
        auto    data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0xF0);
        EXPECT_EQ(data[1], 0xDE);
        EXPECT_EQ(data[2], 0xBC);
        EXPECT_EQ(data[3], 0x9A);
        EXPECT_EQ(data[4], 0x78);
        EXPECT_EQ(data[5], 0x56);
        EXPECT_EQ(data[6], 0x34);
        EXPECT_EQ(data[7], 0x12);
        EXPECT_EQ(data.size(), 8);
        int64_t n2 = CHL::DeserializeSimple<int64_t>(data);
        EXPECT_EQ(n1, n2);
    }
    // signed negative
    {
        int32_t n1   = 0xF2345678;
        auto    data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0x78);
        EXPECT_EQ(data[1], 0x56);
        EXPECT_EQ(data[2], 0x34);
        EXPECT_EQ(data[3], 0xF2);
        EXPECT_EQ(data.size(), 4);
        int32_t n2 = CHL::DeserializeSimple<int32_t>(data);
        EXPECT_EQ(n1, n2);
    }
    {
        int16_t n1   = 0xF234;
        auto    data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0x34);
        EXPECT_EQ(data[1], 0xF2);
        EXPECT_EQ(data.size(), 2);
        int16_t n2 = CHL::DeserializeSimple<int16_t>(data);
        EXPECT_EQ(n1, n2);
    }
    {
        int64_t n1   = 0xF23456789ABCDEF0;
        auto    data = CHL::SerializeSimple(n1);
        EXPECT_EQ(data[0], 0xF0);
        EXPECT_EQ(data[1], 0xDE);
        EXPECT_EQ(data[2], 0xBC);
        EXPECT_EQ(data[3], 0x9A);
        EXPECT_EQ(data[4], 0x78);
        EXPECT_EQ(data[5], 0x56);
        EXPECT_EQ(data[6], 0x34);
        EXPECT_EQ(data[7], 0xF2);
        EXPECT_EQ(data.size(), 8);
        uint64_t n2 = CHL::DeserializeSimple<uint64_t>(data);
        EXPECT_EQ(n1, n2);
    }
}

TEST(cryptography_tests, poly1305_1)
{
    auto        key            = CHL::RandomBytesAsSecureArray<crypto_onetimeauth_poly1305_KEYBYTES>();
    std::string random_message = "xxxyyyzzz";
    CHL::Bytes  message(std::make_move_iterator(random_message.begin()),
                       std::make_move_iterator(random_message.end()));
    auto        authenticator = CHL::Poly1305AuthenticateMessage(message, key);
    EXPECT_TRUE(CHL::Poly1305VerifyMessage(message, authenticator, key));
    authenticator[0]++;
    EXPECT_FALSE(CHL::Poly1305VerifyMessage(message, authenticator, key));
}

TEST(cryptography_tests, poly1305_2)
{
    auto       key           = CHL::RandomBytesAsSecureArray<crypto_onetimeauth_poly1305_KEYBYTES>();
    CHL::Bytes message       = CHL::RandomBytes(123000);
    auto       authenticator = CHL::Poly1305AuthenticateMessage(message, key);
    EXPECT_TRUE(CHL::Poly1305VerifyMessage(message, authenticator, key));
    authenticator[0]++;
    EXPECT_FALSE(CHL::Poly1305VerifyMessage(message, authenticator, key));
}

TEST(cryptography_tests, poly1305_3)
{
    auto       key           = CHL::RandomBytesAsSecureArray<crypto_onetimeauth_poly1305_KEYBYTES>();
    CHL::Bytes message       = CHL::RandomBytes(1123000);
    auto       authenticator = CHL::Poly1305AuthenticateMessage(message, key);
    EXPECT_TRUE(CHL::Poly1305VerifyMessage(message, authenticator, key));
    authenticator[0]++;
    EXPECT_FALSE(CHL::Poly1305VerifyMessage(message, authenticator, key));
}

TEST(cryptography_tests, names_and_variations)
{
    for (int i = 0; i < static_cast<int>(CHL::EncryptionAlgorithm::Enc_Size); i++) {
        auto v = static_cast<CHL::EncryptionAlgorithm>(i);
        EXPECT_EQ(CHL::GetEncryptionAlgoFromName(CHL::GetEncryptionAlgoName(v).get()), v);
    }
    for (int i = 0; i < static_cast<int>(CHL::AuthenticationAlgorithm::Auth_Size); i++) {
        auto v = static_cast<CHL::AuthenticationAlgorithm>(i);
        EXPECT_EQ(CHL::GetAuthAlgoFromName(CHL::GetAuthAlgoName(v).get()), v);
    }
    for (int i = 0; i < static_cast<int>(CHL::AuthKeyRatchetAlgorithm::Ratchet_Size); i++) {
        auto v = static_cast<CHL::AuthKeyRatchetAlgorithm>(i);
        EXPECT_EQ(CHL::GetRatchetAlgoFromName(CHL::GetRatchetAlgoName(v).get()), v);
    }

    EXPECT_EQ(CHL::GetEncryptionAlgoKeyLength(CHL::EncryptionAlgorithm::Enc_XSalsa20Poly1305).get(),
              crypto_secretbox_xsalsa20poly1305_KEYBYTES);
    EXPECT_EQ(CHL::GetRatchetAlgoOutputLength(CHL::AuthKeyRatchetAlgorithm::Ratchet_Sha256).get(),
              SHA256_DIGEST_LENGTH);
    EXPECT_EQ(CHL::GetRatchetAlgoOutputLength(CHL::AuthKeyRatchetAlgorithm::Ratchet_Sha384).get(),
              SHA384_DIGEST_LENGTH);
    EXPECT_EQ(CHL::GetRatchetAlgoOutputLength(CHL::AuthKeyRatchetAlgorithm::Ratchet_Sha512).get(),
              SHA512_DIGEST_LENGTH);
    EXPECT_EQ(CHL::GetAuthAlgoKeyLength(CHL::AuthenticationAlgorithm::Auth_Poly1305).get(),
              crypto_onetimeauth_poly1305_KEYBYTES);
}

void TestGenericEncryption(const CHL::Bytes& message, CHL::EncryptionAlgorithm encAlgo,
                           CHL::AuthenticationAlgorithm authAlgo,
                           CHL::AuthKeyRatchetAlgorithm ratchetAlgo, bool changeSomethingInCipher)
{
    CHL::SecureBytes key = CHL::RandomBytes_Secure(CHL::GetEncryptionAlgoKeyLength(encAlgo).get());
    CHL::EncryptMessageOutput cipherData =
        CHL::EncryptMessage(message, key, encAlgo, ratchetAlgo, authAlgo);
    if (changeSomethingInCipher) {
        uint64_t randomByteCount = CHL::RandomBytesAs<uint64_t>() % cipherData.cipher.size();
        cipherData.cipher.at(randomByteCount) += 1;
    }
    CHL::Bytes recoveredMessage = CHL::DecryptMessage(cipherData, key);
    EXPECT_EQ(message, recoveredMessage);
}

void TestGenericEncryptionRandom(uint64_t messageLength, CHL::EncryptionAlgorithm encAlgo,
                                 CHL::AuthenticationAlgorithm authAlgo,
                                 CHL::AuthKeyRatchetAlgorithm ratchetAlgo, bool changeSomethingInCipher)
{
    CHL::Bytes message = CHL::RandomBytes(messageLength);
    TestGenericEncryption(message, encAlgo, authAlgo, ratchetAlgo, changeSomethingInCipher);
}

TEST(cryptography_tests, generic_encryption)
{
    static constexpr const int NumberOfTestsPerCombination = 25;

    for (int i = 0; i < static_cast<int>(CHL::EncryptionAlgorithm::Enc_Size); i++) {
        for (int j = 0; j < static_cast<int>(CHL::AuthenticationAlgorithm::Auth_Size); j++) {
            for (int k = 0; k < static_cast<int>(CHL::AuthKeyRatchetAlgorithm::Ratchet_Size); k++) {
                CHL::EncryptionAlgorithm     encAlgo     = static_cast<CHL::EncryptionAlgorithm>(i);
                CHL::AuthenticationAlgorithm authAlgo    = static_cast<CHL::AuthenticationAlgorithm>(j);
                CHL::AuthKeyRatchetAlgorithm ratchetAlgo = static_cast<CHL::AuthKeyRatchetAlgorithm>(k);
                EXPECT_ANY_THROW(
                    TestGenericEncryption(CHL::Bytes(), encAlgo, authAlgo, ratchetAlgo, false));
                for (int count = 0; count < NumberOfTestsPerCombination; count++) {
                    uint64_t messageLength = 1 + (CHL::RandomBytesAs<uint64_t>() % 100000);
                    EXPECT_NO_THROW(TestGenericEncryptionRandom(messageLength, encAlgo, authAlgo,
                                                                ratchetAlgo, false));
                }
            }
        }
    }

    for (int i = 0; i < static_cast<int>(CHL::EncryptionAlgorithm::Enc_Size); i++) {
        for (int j = 0; j < static_cast<int>(CHL::AuthenticationAlgorithm::Auth_Size); j++) {
            for (int k = 0; k < static_cast<int>(CHL::AuthKeyRatchetAlgorithm::Ratchet_Size); k++) {
                CHL::EncryptionAlgorithm     encAlgo     = static_cast<CHL::EncryptionAlgorithm>(i);
                CHL::AuthenticationAlgorithm authAlgo    = static_cast<CHL::AuthenticationAlgorithm>(j);
                CHL::AuthKeyRatchetAlgorithm ratchetAlgo = static_cast<CHL::AuthKeyRatchetAlgorithm>(k);
                for (int count = 0; count < NumberOfTestsPerCombination; count++) {
                    uint64_t messageLength = 1 + (CHL::RandomBytesAs<uint64_t>() % 100000);
                    EXPECT_ANY_THROW(TestGenericEncryptionRandom(messageLength, encAlgo, authAlgo,
                                                                 ratchetAlgo, true));
                }
            }
        }
    }
}

TEST(cryptography_tests, encryption_serialization)
{
    static constexpr const int NumberOfTestsPerCombination = 25;

    for (int i = 0; i < static_cast<int>(CHL::EncryptionAlgorithm::Enc_Size); i++) {
        for (int j = 0; j < static_cast<int>(CHL::AuthenticationAlgorithm::Auth_Size); j++) {
            for (int k = 0; k < static_cast<int>(CHL::AuthKeyRatchetAlgorithm::Ratchet_Size); k++) {
                CHL::EncryptionAlgorithm     encAlgo     = static_cast<CHL::EncryptionAlgorithm>(i);
                CHL::AuthenticationAlgorithm authAlgo    = static_cast<CHL::AuthenticationAlgorithm>(j);
                CHL::AuthKeyRatchetAlgorithm ratchetAlgo = static_cast<CHL::AuthKeyRatchetAlgorithm>(k);

                for (int count = 0; count < NumberOfTestsPerCombination; count++) {
                    uint64_t         messageLength = 1 + (CHL::RandomBytesAs<uint64_t>() % 100000);
                    CHL::Bytes       message       = CHL::RandomBytes(messageLength);
                    CHL::SecureBytes key =
                        CHL::RandomBytes_Secure(CHL::GetEncryptionAlgoKeyLength(encAlgo).get());

                    CHL::EncryptMessageOutput cipherData =
                        CHL::EncryptMessage(message, key, encAlgo, ratchetAlgo, authAlgo);
                    CHL::Bytes serialized = CHL::EncryptMessageOutput::Serialize(cipherData);
                    CHL::EncryptMessageOutput cipherDataDeserialized =
                        CHL::EncryptMessageOutput::Deserialize(serialized);

                    CHL::Bytes recoveredMessage1 = CHL::DecryptMessage(cipherData, key);
                    CHL::Bytes recoveredMessage2 = CHL::DecryptMessage(cipherDataDeserialized, key);
                    EXPECT_EQ(message, recoveredMessage1);
                    EXPECT_EQ(message, recoveredMessage2);
                }
            }
        }
    }
}

TEST(cryptography_tests, encrypt_metadata)
{
    static constexpr const int NumberOfTestsPerCombination = 25;

    for (int i = 0; i < static_cast<int>(CHL::EncryptionAlgorithm::Enc_Size); i++) {
        for (int j = 0; j < static_cast<int>(CHL::AuthenticationAlgorithm::Auth_Size); j++) {
            for (int k = 0; k < static_cast<int>(CHL::AuthKeyRatchetAlgorithm::Ratchet_Size); k++) {
                CHL::EncryptionAlgorithm     encAlgo     = static_cast<CHL::EncryptionAlgorithm>(i);
                CHL::AuthenticationAlgorithm authAlgo    = static_cast<CHL::AuthenticationAlgorithm>(j);
                CHL::AuthKeyRatchetAlgorithm ratchetAlgo = static_cast<CHL::AuthKeyRatchetAlgorithm>(k);

                for (int count = 0; count < NumberOfTestsPerCombination; count++) {
                    CKey k1;
                    k1.MakeNewKey(true);

                    uint64_t   messageLength = 1 + (CHL::RandomBytesAs<uint64_t>() % 100);
                    CHL::Bytes message       = CHL::RandomBytes(messageLength);

                    std::string encrypted = NTP1Script::EncryptMetadataWithEphemeralKey(
                        std::string(message.cbegin(), message.cend()), k1, encAlgo, ratchetAlgo,
                        authAlgo);
                    std::string decryptedData = NTP1Script::DecryptMetadata(encrypted, k1);
                    EXPECT_EQ(decryptedData, std::string(message.cbegin(), message.cend()));
                }
            }
        }
    }
}

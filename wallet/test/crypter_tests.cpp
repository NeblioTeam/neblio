#include "googletest/googletest/include/gtest/gtest.h"

#include "crypter.h"
#include "util.h"

#include <boost/algorithm/hex.hpp>

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

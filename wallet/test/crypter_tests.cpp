#include "googletest/googletest/include/gtest/gtest.h"

#include "crypter.h"

#include <boost/algorithm/hex.hpp>

TEST(crypter_tests, basic_tests)
{
    CCrypter crypter;
    std::string toEncrypt("Hi, this is very secret!");
    std::vector<unsigned char> cipherText;
    SecureString strWalletPassphrase("abc");
    CKeyingMaterial decryptedText;
    std::string salt("abcdefgh"); // length must be WALLET_CRYPTO_SALT_SIZE
    std::string cipherHex;
    std::string expectedCipherHex = "44F5DFF1856115B80ED0E7359AACC2DDF4908787D891935C7E1469E29418C52E";
    // set encryption parameters
    EXPECT_TRUE(crypter.SetKeyFromPassphrase(strWalletPassphrase,
                                             std::vector<unsigned char>(salt.begin(), salt.end()),
                                             100,
                                             0));

    // encrypt
    EXPECT_TRUE(crypter.Encrypt(CKeyingMaterial(toEncrypt.begin(), toEncrypt.end()), cipherText));
    boost::algorithm::hex(cipherText.begin(), cipherText.end(), std::back_inserter(cipherHex));

    // make both serialized results same case for comparison
    std::transform(cipherHex.begin(), cipherHex.end(), cipherHex.begin(), ::toupper);
    std::transform(expectedCipherHex.begin(), expectedCipherHex.end(), expectedCipherHex.begin(), ::toupper);

    // after ciphering, the string is not the same
    EXPECT_TRUE(std::equal(cipherHex.begin(), cipherHex.end(), expectedCipherHex.begin()));

    // decrypt
    EXPECT_TRUE(crypter.Decrypt(cipherText, decryptedText));
    std::string decryptedTextStr(decryptedText.begin(), decryptedText.end());
    EXPECT_EQ(decryptedTextStr, toEncrypt);
}

#include "googletest/googletest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "key.h"
#include "base58.h"
#include "uint256.h"
#include "util.h"

using namespace std;

static const string strSecret1C    ("TtnutkcnaPcu3zmjWcrJazf42fp1YAKRpm8grKRRuYjtiykmGuM7");
static const string strSecret2C    ("TnNwg92Wpw8iuBRwaeJydzw2c6MMqTe2c6cA5hn3NBBdqFWvpViF");
static const CBitcoinAddress addr1C("NVFdK9ik6mBCG6syVw2gD1gBwJzKF5me5i");
static const CBitcoinAddress addr2C("NSTdV7BgFeYXR61ywiMyAoof2ihwUPDPpj");


static const string strAddressBad("NSTdV7BgFeYXR61ywiMyAoof2ihwUPDPp");


#ifdef KEY_TESTS_DUMPINFO
void dumpKeyInfo(uint256 privkey)
{
    CSecret secret;
    secret.resize(32);
    memcpy(&secret[0], &privkey, 32);
    vector<unsigned char> sec;
    sec.resize(32);
    memcpy(&sec[0], &secret[0], 32);
    printf("  * secret (hex): %s\n", HexStr(sec).c_str());

    for (int nCompressed=0; nCompressed<2; nCompressed++)
    {
        bool fCompressed = nCompressed == 1;
        printf("  * %s:\n", fCompressed ? "compressed" : "uncompressed");
        CBitcoinSecret bsecret;
        bsecret.SetSecret(secret, fCompressed);
        printf("    * secret (base58): %s\n", bsecret.ToString().c_str());
        CKey key;
        key.SetSecret(secret, fCompressed);
        vector<unsigned char> vchPubKey = key.GetPubKey();
        printf("    * pubkey (hex): %s\n", HexStr(vchPubKey).c_str());
        printf("    * address (base58): %s\n", CBitcoinAddress(vchPubKey).ToString().c_str());
    }
}
#endif

TEST(key_tests, key_test1)
{
    CBitcoinSecret bsecret1C, bsecret2C, baddress1;
    EXPECT_TRUE( bsecret1C.SetString(strSecret1C));
    EXPECT_TRUE( bsecret2C.SetString(strSecret2C));
    EXPECT_TRUE(!baddress1.SetString(strAddressBad));

    bool fCompressed;
    CSecret secret1C = bsecret1C.GetSecret(fCompressed);
    EXPECT_TRUE(fCompressed == true);
    CSecret secret2C = bsecret2C.GetSecret(fCompressed);
    EXPECT_TRUE(fCompressed == true);

    CKey key1C, key2C;
    key1C.SetSecret(secret1C, true);
    key2C.SetSecret(secret2C, true);

    EXPECT_TRUE(addr1C.Get() == CTxDestination(key1C.GetPubKey().GetID()));
    EXPECT_TRUE(addr2C.Get() == CTxDestination(key2C.GetPubKey().GetID()));

    for (int n=0; n<16; n++)
    {
        string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());

        // normal signatures

        vector<unsigned char> sign1C, sign2C;

        EXPECT_TRUE(key1C.Sign(hashMsg, sign1C));
        EXPECT_TRUE(key2C.Sign(hashMsg, sign2C));

        EXPECT_TRUE( key1C.Verify(hashMsg, sign1C));
        EXPECT_TRUE(!key1C.Verify(hashMsg, sign2C));

        EXPECT_TRUE(!key2C.Verify(hashMsg, sign1C));
        EXPECT_TRUE( key2C.Verify(hashMsg, sign2C));

        EXPECT_TRUE( key1C.Verify(hashMsg, sign1C));
        EXPECT_TRUE(!key1C.Verify(hashMsg, sign2C));

        EXPECT_TRUE(!key2C.Verify(hashMsg, sign1C));
        EXPECT_TRUE( key2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)

        vector<unsigned char> csign1C, csign2C;

        EXPECT_TRUE(key1C.SignCompact(hashMsg, csign1C));
        EXPECT_TRUE(key2C.SignCompact(hashMsg, csign2C));

        CKey rkey1C, rkey2C;

        EXPECT_TRUE(rkey1C.SetCompactSignature(hashMsg, csign1C));
        EXPECT_TRUE(rkey2C.SetCompactSignature(hashMsg, csign2C));

        EXPECT_TRUE(rkey1C.GetPubKey() == key1C.GetPubKey());
        EXPECT_TRUE(rkey2C.GetPubKey() == key2C.GetPubKey());
    }
}

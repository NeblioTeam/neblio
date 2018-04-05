#include "googletest/googletest/include/gtest/gtest.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "base58.h"
#include "util.h"

using namespace json_spirit;
extern Array read_json(const std::string& filename);

// Goal: test low-level base58 encoding functionality
TEST(base58_tests, base58_EncodeBase58)
{
    Array tests = read_json("base58_encode_decode.json");

    BOOST_FOREACH(Value& tv, tests)
    {
        Array test = tv.get_array();
        std::string strTest = write_string(tv, false);
        if (test.size() < 2) // Allow for extra stuff (useful for comments)
        {
            ADD_FAILURE() << "Bad test: " << strTest;
            continue;
        }
        std::vector<unsigned char> sourcedata = ParseHex(test[0].get_str());
        std::string base58string = test[1].get_str();
        EXPECT_TRUE(EncodeBase58(&sourcedata[0], &sourcedata[sourcedata.size()]) == base58string) << strTest;
    }
}

// Goal: test low-level base58 decoding functionality
TEST(base58_tests, base58_DecodeBase58)
{
    Array tests = read_json("base58_encode_decode.json");
    std::vector<unsigned char> result;

    BOOST_FOREACH(Value& tv, tests)
    {
        Array test = tv.get_array();
        std::string strTest = write_string(tv, false);
        if (test.size() < 2) // Allow for extra stuff (useful for comments)
        {
            ADD_FAILURE() << "Bad test: " << strTest;
            continue;
        }
        std::vector<unsigned char> expected = ParseHex(test[0].get_str());
        std::string base58string = test[1].get_str();
        EXPECT_TRUE(DecodeBase58(base58string, result)) << strTest;
        EXPECT_TRUE(result.size() == expected.size() && std::equal(result.begin(), result.end(), expected.begin())) << strTest;
    }

    EXPECT_TRUE(!DecodeBase58("invalid", result));
}

// Visitor to check address type
class TestAddrTypeVisitor : public boost::static_visitor<bool>
{
private:
    std::string exp_addrType;
public:
    TestAddrTypeVisitor(const std::string &exp_addrType) : exp_addrType(exp_addrType) { }
    bool operator()(const CKeyID &id) const
    {
        return (exp_addrType == "pubkey");
    }
    bool operator()(const CScriptID &id) const
    {
        return (exp_addrType == "script");
    }
    bool operator()(const CNoDestination &no) const
    {
        return (exp_addrType == "none");
    }
};

// Visitor to check address payload
class TestPayloadVisitor : public boost::static_visitor<bool>
{
private:
    std::vector<unsigned char> exp_payload;
public:
    TestPayloadVisitor(std::vector<unsigned char> &exp_payload) : exp_payload(exp_payload) { }
    bool operator()(const CKeyID &id) const
    {
        uint160 exp_key(exp_payload);
        return exp_key == id;
    }
    bool operator()(const CScriptID &id) const
    {
        uint160 exp_key(exp_payload);
        return exp_key == id;
    }
    bool operator()(const CNoDestination &no) const
    {
        return exp_payload.size() == 0;
    }
};

// Goal: check that parsed keys match test payload
TEST(base58_tests, base58_keys_valid_parse)
{
    Array tests = read_json("base58_keys_valid.json");
    std::vector<unsigned char> result;
    CBitcoinSecret secret;
    CBitcoinAddress addr;
    // Save global state
    bool fTestNet_stored = fTestNet;

    BOOST_FOREACH(Value& tv, tests)
    {
        Array test = tv.get_array();
        std::string strTest = write_string(tv, false);
        if (test.size() < 3) // Allow for extra stuff (useful for comments)
        {
            ADD_FAILURE() << "Bad test: " << strTest;
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        std::vector<unsigned char> privkey_bin_from_hex = ParseHex(test[1].get_str());
        const Object &metadata = test[2].get_obj();
        bool isPrivkey = find_value(metadata, "isPrivkey").get_bool();
        bool isTestnet = find_value(metadata, "isTestnet").get_bool();
        fTestNet = isTestnet; // Override testnet flag
        if(isPrivkey)
        {
            bool isCompressed = find_value(metadata, "isCompressed").get_bool();
            // Must be valid private key
            EXPECT_TRUE(secret.SetString(exp_base58string)) << "!SetString:"+ strTest;
            EXPECT_TRUE(secret.IsValid()) << "!IsValid:" + strTest;
            bool fCompressedOut = false;
            CSecret privkey = secret.GetSecret(fCompressedOut);
            EXPECT_TRUE(fCompressedOut == isCompressed) << "compressed mismatch:" + strTest;
            EXPECT_TRUE(privkey.size() == privkey_bin_from_hex.size() && std::equal(privkey.begin(), privkey.end(), privkey_bin_from_hex.begin())) << "key mismatch:" + strTest;

            // Private key must be invalid public key
            addr.SetString(exp_base58string);
            EXPECT_TRUE(!addr.IsValid()) << "IsValid privkey as pubkey:" + strTest;
        }
        else
        {
            std::string exp_addrType = find_value(metadata, "addrType").get_str(); // "script" or "pubkey"
            // Must be valid public key
            EXPECT_TRUE(addr.SetString(exp_base58string)) << "SetString:" + strTest;
            EXPECT_TRUE(addr.IsValid()) << "!IsValid:" + strTest;
            EXPECT_TRUE(addr.IsScript() == (exp_addrType == "script")) << "isScript mismatch" + strTest;
            CTxDestination dest = addr.Get();
            EXPECT_TRUE(boost::apply_visitor(TestAddrTypeVisitor(exp_addrType), dest)) << "addrType mismatch" + strTest;

            // Public key must be invalid private key
            secret.SetString(exp_base58string);
            EXPECT_TRUE(!secret.IsValid()) << "IsValid pubkey as privkey:" + strTest;
        }
    }
    // Restore global state
    fTestNet = fTestNet_stored;
}

// Goal: check that base58 parsing code is robust against a variety of corrupted data
TEST(base58_tests, base58_keys_invalid)
{
    Array tests = read_json("base58_keys_invalid.json"); // Negative testcases
    std::vector<unsigned char> result;
    CBitcoinSecret secret;
    CBitcoinAddress addr;

    BOOST_FOREACH(Value& tv, tests)
    {
        Array test = tv.get_array();
        std::string strTest = write_string(tv, false);
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            ADD_FAILURE() << "Bad test: " << strTest;
            continue;
        }
        std::string exp_base58string = test[0].get_str();

        // must be invalid as public and as private key
        addr.SetString(exp_base58string);
        EXPECT_TRUE(!addr.IsValid()) << "IsValid pubkey:" + strTest;
        secret.SetString(exp_base58string);
        EXPECT_TRUE(!secret.IsValid()) << "IsValid privkey:" + strTest;
    }
}

template<typename TInputIter>
std::string make_hex_string(TInputIter first, TInputIter last, bool use_uppercase = true, bool insert_spaces = false)
{
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    if (use_uppercase)
        ss << std::uppercase;
    while (first != last)
    {
        ss << std::setw(2) << static_cast<int>(*first++);
        if (insert_spaces && first != last)
            ss << " ";
    }
    return ss.str();
}

void test_priv_key_vs_address(const std::string& privkey, std::string pubkey, const std::string& address)
{
    CBitcoinSecret vchSecret;
    EXPECT_TRUE(vchSecret.SetString(privkey));
    bool fCompressed;
    CKey key;
    CSecret secret = vchSecret.GetSecret(fCompressed);
    EXPECT_TRUE(fCompressed);
    key.SetSecret(secret, fCompressed);
    EXPECT_TRUE(key.GetPubKey().IsValid());
    std::vector<unsigned char> rawPubKey = key.GetPubKey().Raw();
    std::transform(pubkey.begin(), pubkey.end(), pubkey.begin(), ::tolower); // make pubkey lower-case
    EXPECT_EQ(make_hex_string(rawPubKey.begin(),rawPubKey.end(), false), pubkey);
    CKeyID keyid = key.GetPubKey().GetID();
    EXPECT_EQ(CBitcoinAddress(keyid).ToString(), address);
}

void test_random_key_generation()
{
    // create private key from scratch
    CKey key;
    key.MakeNewKey(true);

    // create private key string from raw
    CBitcoinSecret vchSecret;
    bool compressed = false;
    vchSecret.SetSecret(key.GetSecret(compressed), true);
    EXPECT_TRUE(compressed); // currently keys are only compressed

    // validate public key
    EXPECT_TRUE(key.GetPubKey().IsValid());

    // get raw public key
    std::vector<unsigned char> rawPubKey = key.GetPubKey().Raw();

    // get address
    CKeyID keyid = key.GetPubKey().GetID();
    CBitcoinAddress address(keyid);

    //test
    test_priv_key_vs_address(vchSecret.ToString(),
                             make_hex_string(rawPubKey.begin(), rawPubKey.end(), false),
                             address.ToString());
}

TEST(base58_tests, base58_keys_generation)
{
    // Save global test-net state
    bool fTestNet_stored = fTestNet;

    // real-net
    test_priv_key_vs_address("TtnutkcnaPcu3zmjWcrJazf42fp1YAKRpm8grKRRuYjtiykmGuM7",
                             "037f41ae8b46979087562e65494eb3a3b9d8addde9b9568ef5cbb8197fd26c0ff2",
                             "NVFdK9ik6mBCG6syVw2gD1gBwJzKF5me5i");
    test_priv_key_vs_address("TnNwg92Wpw8iuBRwaeJydzw2c6MMqTe2c6cA5hn3NBBdqFWvpViF",
                             "03c7f8863df49735b1a1906a5f5939beb8622074b3ddf8fccc5462362271145f09",
                             "NSTdV7BgFeYXR61ywiMyAoof2ihwUPDPpj");
    test_priv_key_vs_address("TpWmAWxNCGN7tj218djRJjegAVy34K2eEx8Zbt7xbf2H9GNUBgci",
                             "029cbf0da830b83a457877fcf009160e9de9f0383fe0c97769ce9a4c2d52949f4b",
                             "NdLGazEn51ofFuztenM7bNfquNBV1FWMGG");

    for(int i = 0; i < 10; i++) {
        test_random_key_generation();
    }

    // test-net
    fTestNet = true;
    test_priv_key_vs_address("Vgg5VL2TW1NMNKt4wkazRkUygpnPiQXnztA2h3ALQxGUhk1tQUag",
                             "0243bab8b87abbd42493ca577dee9befc15cc5565f2792e18c52eb90cdfa13dc2a",
                             "TVPDsVw4vSbNkRPkfnwCJmbCygEuJEVpwW");

    test_priv_key_vs_address("VfCCG4Ew6XAtxpEEnuWTTfUgX92fvyq2kPh7cghHtPDQs7PTmwSn",
                             "02f20e5d83d939edc1169296250503b71cc37438ab608fbffb0ea11539d9341c7f",
                             "TPiGYtUnB3qCjYBuXAj6QX7CiW5sJQ7Sdk");

    test_priv_key_vs_address("VfAy2E8BhcFat6dLGaZotaZJReEU2jWHzZi6a5XQqD9q5qFAWzuK",
                             "023ecc7eee129e8b009461b67b9f55120675c61957a7bb7f02726f73215051cb76",
                             "THs3Lec52yQPfErz7Z32Yi3KJnBTzicEiz");

    for(int i = 0; i < 10; i++) {
        test_random_key_generation();
    }

    // Restore global state
    fTestNet = fTestNet_stored;
}

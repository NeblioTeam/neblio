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

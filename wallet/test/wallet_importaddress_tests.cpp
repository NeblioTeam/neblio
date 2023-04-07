#include "googletest/googletest/include/gtest/gtest.h"

#include "wallet.h"
#include "environment.h"
#include "txdb-lmdb.h"

class WalletTestFixture : public ::testing::Test
{
public:
    std::string walletPath = std::string(TEST_ROOT_PATH) + "/data/wallet.dat";
    CWallet* wallet_;

    virtual void SetUp()
    {
        wallet_         = new CWallet(walletPath);
        bool fFirstRun = true;
        DBErrors nLoadWalletRet = wallet_->LoadWallet(fFirstRun);
        if (nLoadWalletRet != DB_LOAD_OK) {
            if (nLoadWalletRet == DB_CORRUPT)
                FAIL() << _("Error loading wallet.dat: Wallet corrupted") << "\n";
            else if (nLoadWalletRet == DB_NONCRITICAL_ERROR) {
                std::string msg(
                    _("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                      " or address book entries might be missing or incorrect."));
                std::cerr << msg;
            } else if (nLoadWalletRet == DB_TOO_NEW)
                FAIL() << _("Error loading wallet.dat: Wallet requires newer version of neblio") << "\n";
            else if (nLoadWalletRet == DB_NEED_REWRITE) {
                FAIL() << _("Wallet needed to be rewritten: restart neblio to complete") << "\n";
            } else
                FAIL() << _("Error loading wallet.dat") << "\n";
        }
    }

    virtual void TearDown() {
        if (boost::filesystem::exists(walletPath)) {
            boost::filesystem::remove(walletPath);
        }
    }

    CScript getCScript(const CPubKey& k) {
        const std::vector<unsigned char>& vch = k.Raw();
        return CScript() << std::vector<unsigned char>(vch.begin(), vch.end()) << OP_CHECKSIG;
    }

    void test_watch_only(CWallet& pwallet, const CPubKey& pubkeyToAdd)
    {
        CScript p2pk = getCScript(pubkeyToAdd);
        CKeyID addAddress = pubkeyToAdd.GetID();
        CPubKey foundPubkey;
        LOCK(pwallet.cs_wallet);

        EXPECT_TRUE(!pwallet.HaveWatchOnly(p2pk));
        pwallet.LoadWatchOnly(p2pk, CKeyMetadata{GetTime()});

        EXPECT_TRUE(pwallet.HaveWatchOnly(p2pk));

        bool isPubkeyValid = pubkeyToAdd.IsValid();
        if (isPubkeyValid) {
            EXPECT_TRUE(pwallet.GetWatchPubKey(addAddress, foundPubkey));
            EXPECT_EQ(foundPubkey, pubkeyToAdd);
        } else {
            EXPECT_FALSE(pwallet.GetWatchPubKey(addAddress, foundPubkey));
            EXPECT_EQ(foundPubkey, CPubKey()); // passed key is unchanged
        }

        pwallet.RemoveWatchOnly(p2pk);
        EXPECT_EQ(pwallet.HaveWatchOnly(p2pk), false);

        if (isPubkeyValid) {
            EXPECT_TRUE(!pwallet.GetWatchPubKey(addAddress, foundPubkey));
            EXPECT_EQ(foundPubkey, pubkeyToAdd); // passed key is unchanged
        }
    }
};


TEST_F(WalletTestFixture, import_scripts) {

     CKey key;
     CPubKey pubkey;
     key.MakeNewKey(false);
     pubkey = key.GetPubKey();

     EXPECT_TRUE(wallet_->mapKeyWatchOnlyMetadata.empty());

     CScript script = getCScript(pubkey);
     auto time = GetTime();
     EXPECT_TRUE(wallet_->ImportScripts({script}, time));

     EXPECT_TRUE(wallet_->mapKeyWatchOnlyMetadata.size() == 1);

     CKeyMetadata meta;
     EXPECT_NO_THROW(meta = wallet_->mapKeyWatchOnlyMetadata.at(script.GetID()));
     EXPECT_EQ(meta.nCreateTime, time);

     // Add existing script
     EXPECT_TRUE(!wallet_->mapKeyWatchOnlyMetadata.empty());
     EXPECT_TRUE(wallet_->ImportScripts({script}, time));
     EXPECT_TRUE(wallet_->mapKeyWatchOnlyMetadata.size() == 1);
}

TEST_F(WalletTestFixture, import_scripts_pub_key) {

     CKey key;
     CPubKey pubkey;
     key.MakeNewKey(false);
     pubkey = key.GetPubKey();

     EXPECT_TRUE(wallet_->mapKeyWatchOnlyMetadata.empty());
     EXPECT_TRUE(wallet_->setWatchOnly.empty());
     EXPECT_TRUE(wallet_->mapWatchKeys.empty());

     CScript script = getCScript(pubkey);
     auto time = GetTime();
     EXPECT_TRUE(wallet_->ImportScriptPubKeys("testLabel", {script}, false, true, time));

     EXPECT_TRUE(wallet_->mapKeyWatchOnlyMetadata.size() == 1);
     EXPECT_TRUE(wallet_->setWatchOnly.size() == 1);
     EXPECT_TRUE(wallet_->mapWatchKeys.size() == 1);
     EXPECT_TRUE(wallet_->mapAddressBook.size() == 1);

     CTxDestination dest;
     ExtractDestination(CTxDB(), script, dest);
     boost::optional<AddressBook::CAddressBookData> v = wallet_->mapAddressBook.get(dest);

     if (v.is_initialized()) {
         auto p = v.get();
         EXPECT_EQ(p.purpose, "receive");
         EXPECT_EQ(p.name, "testLabel");
     }
}

TEST_F(WalletTestFixture, watch_only_addresses) {
    CKey key;
    CPubKey pubkey;

    EXPECT_TRUE(wallet_->setWatchOnly.empty());

    // uncompressed valid PubKey
    key.MakeNewKey(false);
    pubkey = key.GetPubKey();
    assert(!pubkey.IsCompressed());
    test_watch_only(*wallet_, pubkey);

    // uncompressed cryptographically invalid PubKey
    auto pubRaw = pubkey.Raw();
    // std::fill(pubRaw.begin()+1, pubRaw.end(), 0);
    pubRaw.push_back('0');
    auto invalidPubKey = CPubKey(pubRaw);

    test_watch_only(*wallet_, invalidPubKey);

    // compressed valid PubKey
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();
    assert(pubkey.IsCompressed());
    test_watch_only(*wallet_, pubkey);

    // compressed cryptographically invalid PubKey
    pubRaw = pubkey.Raw();
    // std::fill(pubRaw.begin()+1, pubRaw.end(), 0);
    pubRaw.erase(pubRaw.begin(), pubRaw.begin()+1);

    invalidPubKey = CPubKey(pubRaw);
    test_watch_only(*wallet_, invalidPubKey);

}

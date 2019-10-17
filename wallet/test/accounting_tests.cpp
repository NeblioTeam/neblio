#include "googletest/googletest/include/gtest/gtest.h"

#include <boost/foreach.hpp>

#include "init.h"
#include "wallet.h"
#include "walletdb.h"

static void GetResults(CWallet* wallet, CWalletDB& walletdb,
                       std::map<int64_t, CAccountingEntry>& results)
{
    std::list<CAccountingEntry> aes;

    results.clear();
    EXPECT_TRUE(walletdb.ReorderTransactions(wallet) == DB_LOAD_OK);
    walletdb.ListAccountCreditDebit("", aes);
    BOOST_FOREACH (CAccountingEntry& ae, aes) {
        results[ae.nOrderPos] = ae;
    }
}

TEST(accounting_tests, acc_orderupgrade)
{
    // (By Sam): Open a new wallet if it doesn't exist, and remove it if it already exists
    bool        fFirstRun  = true;
    std::string walletPath = std::string(TEST_ROOT_PATH) + "/data/wallet.dat";
    if (boost::filesystem::exists(walletPath)) {
        ASSERT_TRUE(boost::filesystem::remove(walletPath));
    }
    // Do not use smart pointers here; other tests use this
    CWallet* wallet         = new CWallet(walletPath);
    DBErrors nLoadWalletRet = wallet->LoadWallet(fFirstRun);
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
    // At this point a wallet should be there
    ///////////////////

    CWalletDB                           walletdb(walletPath);
    std::vector<CWalletTx*>             vpwtx;
    CWalletTx                           wtx;
    CAccountingEntry                    ae;
    std::map<int64_t, CAccountingEntry> results;

    ae.strAccount      = "";
    ae.nCreditDebit    = 1;
    ae.nTime           = 1333333333;
    ae.strOtherAccount = "b";
    ae.strComment      = "";
    walletdb.WriteAccountingEntry(ae);

    wtx.mapValue["comment"] = "z";
    wallet->AddToWallet(wtx);
    vpwtx.push_back(&wallet->mapWallet[wtx.GetHash()]);
    vpwtx[0]->nTimeReceived = (unsigned int)1333333335;
    vpwtx[0]->nOrderPos     = -1;

    ae.nTime           = 1333333336;
    ae.strOtherAccount = "c";
    walletdb.WriteAccountingEntry(ae);

    GetResults(wallet, walletdb, results);

    EXPECT_TRUE(wallet->nOrderPosNext == 3);
    EXPECT_TRUE(2 == results.size());
    EXPECT_TRUE(results[0].nTime == 1333333333);
    EXPECT_TRUE(results[0].strComment.empty());
    EXPECT_TRUE(1 == vpwtx[0]->nOrderPos);
    EXPECT_TRUE(results[2].nTime == 1333333336);
    EXPECT_TRUE(results[2].strOtherAccount == "c");

    ae.nTime           = 1333333330;
    ae.strOtherAccount = "d";
    ae.nOrderPos       = wallet->IncOrderPosNext();
    walletdb.WriteAccountingEntry(ae);

    GetResults(wallet, walletdb, results);

    EXPECT_TRUE(results.size() == 3);
    EXPECT_TRUE(wallet->nOrderPosNext == 4);
    EXPECT_TRUE(results[0].nTime == 1333333333);
    EXPECT_TRUE(1 == vpwtx[0]->nOrderPos);
    EXPECT_TRUE(results[2].nTime == 1333333336);
    EXPECT_TRUE(results[3].nTime == 1333333330);
    EXPECT_TRUE(results[3].strComment.empty());

    wtx.mapValue["comment"] = "y";
    --wtx.nLockTime; // Just to change the hash :)
    wallet->AddToWallet(wtx);
    vpwtx.push_back(&wallet->mapWallet[wtx.GetHash()]);
    vpwtx[1]->nTimeReceived = (unsigned int)1333333336;

    wtx.mapValue["comment"] = "x";
    --wtx.nLockTime; // Just to change the hash :)
    wallet->AddToWallet(wtx);
    vpwtx.push_back(&wallet->mapWallet[wtx.GetHash()]);
    vpwtx[2]->nTimeReceived = (unsigned int)1333333329;
    vpwtx[2]->nOrderPos     = -1;

    GetResults(wallet, walletdb, results);

    EXPECT_TRUE(results.size() == 3);
    EXPECT_TRUE(wallet->nOrderPosNext == 6);
    EXPECT_TRUE(0 == vpwtx[2]->nOrderPos);
    EXPECT_TRUE(results[1].nTime == 1333333333);
    EXPECT_TRUE(2 == vpwtx[0]->nOrderPos);
    EXPECT_TRUE(results[3].nTime == 1333333336);
    EXPECT_TRUE(results[4].nTime == 1333333330);
    EXPECT_TRUE(results[4].strComment.empty());
    EXPECT_TRUE(5 == vpwtx[1]->nOrderPos);

    ae.nTime           = 1333333334;
    ae.strOtherAccount = "e";
    ae.nOrderPos       = -1;
    walletdb.WriteAccountingEntry(ae);

    GetResults(wallet, walletdb, results);

    EXPECT_TRUE(results.size() == 4);
    EXPECT_TRUE(wallet->nOrderPosNext == 7);
    EXPECT_TRUE(0 == vpwtx[2]->nOrderPos);
    EXPECT_TRUE(results[1].nTime == 1333333333);
    EXPECT_TRUE(2 == vpwtx[0]->nOrderPos);
    EXPECT_TRUE(results[3].nTime == 1333333336);
    EXPECT_TRUE(results[3].strComment.empty());
    EXPECT_TRUE(results[4].nTime == 1333333330);
    EXPECT_TRUE(results[4].strComment.empty());
    EXPECT_TRUE(results[5].nTime == 1333333334);
    EXPECT_TRUE(6 == vpwtx[1]->nOrderPos);
}

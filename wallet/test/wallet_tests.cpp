#include "googletest/googletest/include/gtest/gtest.h"

#include "main.h"
#include "wallet.h"

// how many times to run all the tests to have a chance to catch errors that only show up with particular
// random shuffles
#define RUN_TESTS 100

// some tests fail 1% of the time due to bad luck.
// we repeat those tests this many times and only complain if all iterations of the test fail
#define RANDOM_REPEATS 5

using namespace std;

typedef set<pair<const CWalletTx*, unsigned int>> CoinSet;

static CWallet         wallet;
static vector<COutput> vCoins;

static void add_coin(int64_t nValue, int nAge = 6 * 24, bool fIsFromMe = false, int nInput = 0)
{
    static int   i;
    CTransaction tx;
    tx.nLockTime = i++; // so all transactions get different hashes
    tx.vout.resize(nInput + 1);
    tx.vout[nInput].nValue = nValue;
    CWalletTx* wtx         = new CWalletTx(&wallet, tx);
    if (fIsFromMe) {
        // IsFromMe() returns (GetDebit() > 0), and GetDebit() is 0 if vin.empty(),
        // so stop vin being empty, and cache a non-zero Debit to fake out IsFromMe()
        wtx->vin.resize(1);
        wtx->fDebitCached = true;
        wtx->nDebitCached = 1;
    }
    COutput output(wtx, nInput, nAge);
    vCoins.push_back(output);
}

static void empty_wallet(void)
{
    for (COutput output : vCoins)
        delete output.tx;
    vCoins.clear();
}

static bool equal_sets(CoinSet a, CoinSet b)
{
    pair<CoinSet::iterator, CoinSet::iterator> ret = mismatch(a.begin(), a.end(), b.begin());
    return ret.first == a.end() && ret.second == b.end();
}

TEST(wallet_tests, coin_selection_tests)
{
    static CoinSet setCoinsRet, setCoinsRet2;
    static int64_t nValueRet;

    // test multiple times to allow for differences in the shuffle order
    for (int i = 0; i < RUN_TESTS; i++) {
        empty_wallet();

        // with an empty wallet we can't even pay one cent
        EXPECT_TRUE(!wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 6, vCoins, setCoinsRet,
                                               nValueRet));

        add_coin(1 * CENT, 4); // add a new 1 cent coin

        // with a new 1 cent coin, we still can't find a mature 1 cent
        EXPECT_TRUE(!wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 6, vCoins, setCoinsRet,
                                               nValueRet));

        // but we can find a new 1 cent
        EXPECT_TRUE(wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1 * CENT);

        add_coin(2 * CENT); // add a mature 2 cent coin

        // we can't make 3 cents of mature coins
        EXPECT_TRUE(!wallet.SelectCoinsMinConf(3 * CENT, GetAdjustedTime(), 1, 6, vCoins, setCoinsRet,
                                               nValueRet));

        // we can make 3 cents of new  coins
        EXPECT_TRUE(wallet.SelectCoinsMinConf(3 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 3 * CENT);

        add_coin(5 * CENT);           // add a mature 5 cent coin,
        add_coin(10 * CENT, 3, true); // a new 10 cent coin sent from one of our own addresses
        add_coin(20 * CENT);          // and a mature 20 cent coin

        // now we have new: 1+10=11 (of which 10 was self-sent), and mature: 2+5+20=27.  total = 38

        // we can't make 38 cents only if we disallow new coins:
        EXPECT_TRUE(!wallet.SelectCoinsMinConf(38 * CENT, GetAdjustedTime(), 1, 6, vCoins, setCoinsRet,
                                               nValueRet));
        // we can't even make 37 cents if we don't allow new coins even if they're from us
        EXPECT_TRUE(!wallet.SelectCoinsMinConf(38 * CENT, GetAdjustedTime(), 6, 6, vCoins, setCoinsRet,
                                               nValueRet));
        // but we can make 37 cents if we accept new coins from ourself
        EXPECT_TRUE(wallet.SelectCoinsMinConf(37 * CENT, GetAdjustedTime(), 1, 6, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 37 * CENT);
        // and we can make 38 cents if we accept all new coins
        EXPECT_TRUE(wallet.SelectCoinsMinConf(38 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 38 * CENT);

        // try making 34 cents from 1,2,5,10,20 - we can't do it exactly
        EXPECT_TRUE(wallet.SelectCoinsMinConf(34 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_GT(nValueRet, 34 * CENT);            // but should get more than 34 cents
        EXPECT_EQ(setCoinsRet.size(), (unsigned)3); // the best should be 20+10+5.  it's incredibly
                                                    // unlikely the 1 or 2 got included (but possible)

        // when we try making 7 cents, the smaller coins (1,2,5) are enough.  We should see just 2+5
        EXPECT_TRUE(wallet.SelectCoinsMinConf(7 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 7 * CENT);
        EXPECT_EQ(setCoinsRet.size(), (unsigned)2);

        // when we try making 8 cents, the smaller coins (1,2,5) are exactly enough.
        EXPECT_TRUE(wallet.SelectCoinsMinConf(8 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_TRUE(nValueRet == 8 * CENT);
        EXPECT_EQ(setCoinsRet.size(), (unsigned)3);

        // when we try making 9 cents, no subset of smaller coins is enough, and we get the next bigger
        // coin (10)
        EXPECT_TRUE(wallet.SelectCoinsMinConf(9 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 10 * CENT);
        EXPECT_EQ(setCoinsRet.size(), (unsigned)1);

        // now clear out the wallet and start again to test choosing between subsets of smaller coins and
        // the next biggest coin
        empty_wallet();

        add_coin(6 * CENT);
        add_coin(7 * CENT);
        add_coin(8 * CENT);
        add_coin(20 * CENT);
        add_coin(30 * CENT); // now we have 6+7+8+20+30 = 71 cents total

        // check that we have 71 and not 72
        EXPECT_TRUE(wallet.SelectCoinsMinConf(71 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_TRUE(!wallet.SelectCoinsMinConf(72 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                               nValueRet));

        // now try making 16 cents.  the best smaller coins can do is 6+7+8 = 21; not as good at the next
        // biggest coin, 20
        EXPECT_TRUE(wallet.SelectCoinsMinConf(16 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 20 * CENT); // we should get 20 in one coin
        EXPECT_EQ(setCoinsRet.size(), (unsigned)1);

        add_coin(5 * CENT); // now we have 5+6+7+8+20+30 = 75 cents total

        // now if we try making 16 cents again, the smaller coins can make 5+6+7 = 18 cents, better than
        // the next biggest coin, 20
        EXPECT_TRUE(wallet.SelectCoinsMinConf(16 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 18 * CENT); // we should get 18 in 3 coins
        EXPECT_EQ(setCoinsRet.size(), (unsigned)3);

        add_coin(18 * CENT); // now we have 5+6+7+8+18+20+30

        // and now if we try making 16 cents again, the smaller coins can make 5+6+7 = 18 cents, the same
        // as the next biggest coin, 18
        EXPECT_TRUE(wallet.SelectCoinsMinConf(16 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 18 * CENT); // we should get 18 in 1 coin
        EXPECT_EQ(setCoinsRet.size(),
                  (unsigned)1); // because in the event of a tie, the biggest coin wins

        // now try making 11 cents.  we should get 5+6
        EXPECT_TRUE(wallet.SelectCoinsMinConf(11 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 11 * CENT);
        EXPECT_EQ(setCoinsRet.size(), (unsigned)2);

        // check that the smallest bigger coin is used
        add_coin(1 * COIN);
        add_coin(2 * COIN);
        add_coin(3 * COIN);
        add_coin(4 * COIN); // now we have 5+6+7+8+18+20+30+100+200+300+400 = 1094 cents
        EXPECT_TRUE(wallet.SelectCoinsMinConf(95 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1 * COIN); // we should get 1 BTC in 1 coin
        EXPECT_EQ(setCoinsRet.size(), (unsigned)1);

        EXPECT_TRUE(wallet.SelectCoinsMinConf(195 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 2 * COIN); // we should get 2 BTC in 1 coin
        EXPECT_EQ(setCoinsRet.size(), (unsigned)1);

        // empty the wallet and start again, now with fractions of a cent, to test sub-cent change
        // avoidance
        empty_wallet();
        add_coin(0.1 * CENT);
        add_coin(0.2 * CENT);
        add_coin(0.3 * CENT);
        add_coin(0.4 * CENT);
        add_coin(0.5 * CENT);

        // try making 1 cent from 0.1 + 0.2 + 0.3 + 0.4 + 0.5 = 1.5 cents
        // we'll get sub-cent change whatever happens, so can expect 1.0 exactly
        EXPECT_TRUE(wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1 * CENT);

        // but if we add a bigger coin, making it possible to avoid sub-cent change, things change:
        add_coin(1111 * CENT);

        // try making 1 cent from 0.1 + 0.2 + 0.3 + 0.4 + 0.5 + 1111 = 1112.5 cents
        EXPECT_TRUE(wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1 * CENT); // we should get the exact amount

        // if we add more sub-cent coins:
        add_coin(0.6 * CENT);
        add_coin(0.7 * CENT);

        // and try again to make 1.0 cents, we can still make 1.0 cents
        EXPECT_TRUE(wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1 * CENT); // we should get the exact amount

        // run the 'mtgox' test (see
        // http://blockexplorer.com/tx/29a3efd3ef04f9153d47a990bd7b048a4b2d213daaa5fb8ed670fb85f13bdbcf)
        // they tried to consolidate 10 50k coins into one 500k coin, and ended up with 50k in change
        empty_wallet();
        for (int i = 0; i < 20; i++)
            add_coin(50000 * COIN);

        EXPECT_TRUE(wallet.SelectCoinsMinConf(500000 * COIN, GetAdjustedTime(), 1, 1, vCoins,
                                              setCoinsRet, nValueRet));
        EXPECT_EQ(nValueRet, 500000 * COIN);         // we should get the exact amount
        EXPECT_EQ(setCoinsRet.size(), (unsigned)10); // in ten coins

        // if there's not enough in the smaller coins to make at least 1 cent change (0.5+0.6+0.7
        // < 1.0+1.0), we need to try finding an exact subset anyway

        // sometimes it will fail, and so we use the next biggest coin:
        empty_wallet();
        add_coin(0.5 * CENT);
        add_coin(0.6 * CENT);
        add_coin(0.7 * CENT);
        add_coin(1111 * CENT);
        EXPECT_TRUE(wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1111 * CENT); // we get the bigger coin
        EXPECT_EQ(setCoinsRet.size(), (unsigned)1);

        // but sometimes it's possible, and we use an exact subset (0.4 + 0.6 = 1.0)
        empty_wallet();
        add_coin(0.4 * CENT);
        add_coin(0.6 * CENT);
        add_coin(0.8 * CENT);
        add_coin(1111 * CENT);
        EXPECT_TRUE(wallet.SelectCoinsMinConf(1 * CENT, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1 * CENT);             // we should get the exact amount
        EXPECT_EQ(setCoinsRet.size(), (unsigned)2); // in two coins 0.4+0.6

        // test avoiding sub-cent change
        empty_wallet();
        add_coin(0.0005 * COIN);
        add_coin(0.01 * COIN);
        add_coin(1 * COIN);

        // trying to make 1.0001 from these three coins
        EXPECT_TRUE(wallet.SelectCoinsMinConf(1.0001 * COIN, GetAdjustedTime(), 1, 1, vCoins,
                                              setCoinsRet, nValueRet));
        EXPECT_EQ(nValueRet, 1.0105 * COIN); // we should get all coins
        EXPECT_EQ(setCoinsRet.size(), (unsigned)3);

        // but if we try to make 0.999, we should take the bigger of the two small coins to avoid
        // sub-cent change
        EXPECT_TRUE(wallet.SelectCoinsMinConf(0.999 * COIN, GetAdjustedTime(), 1, 1, vCoins, setCoinsRet,
                                              nValueRet));
        EXPECT_EQ(nValueRet, 1.01 * COIN); // we should get 1 + 0.01
        EXPECT_EQ(setCoinsRet.size(), (unsigned)2);

        // test randomness
        {
            empty_wallet();
            for (int i2 = 0; i2 < 100; i2++)
                add_coin(COIN);

            // picking 50 from 100 coins doesn't depend on the shuffle,
            // but does depend on randomness in the stochastic approximation code
            EXPECT_TRUE(wallet.SelectCoinsMinConf(50 * COIN, GetAdjustedTime(), 1, 6, vCoins,
                                                  setCoinsRet, nValueRet));
            EXPECT_TRUE(wallet.SelectCoinsMinConf(50 * COIN, GetAdjustedTime(), 1, 6, vCoins,
                                                  setCoinsRet2, nValueRet));
            EXPECT_TRUE(!equal_sets(setCoinsRet, setCoinsRet2));

            int fails = 0;
            for (int i = 0; i < RANDOM_REPEATS; i++) {
                // selecting 1 from 100 identical coins depends on the shuffle; this test will fail 1% of
                // the time run the test RANDOM_REPEATS times and only complain if all of them fail
                EXPECT_TRUE(wallet.SelectCoinsMinConf(COIN, GetAdjustedTime(), 1, 6, vCoins, setCoinsRet,
                                                      nValueRet));
                EXPECT_TRUE(wallet.SelectCoinsMinConf(COIN, GetAdjustedTime(), 1, 6, vCoins,
                                                      setCoinsRet2, nValueRet));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            EXPECT_NE(fails, RANDOM_REPEATS);

            // add 75 cents in small change.  not enough to make 90 cents,
            // then try making 90 cents.  there are multiple competing "smallest bigger" coins,
            // one of which should be picked at random
            add_coin(5 * CENT);
            add_coin(10 * CENT);
            add_coin(15 * CENT);
            add_coin(20 * CENT);
            add_coin(25 * CENT);

            fails = 0;
            for (int i = 0; i < RANDOM_REPEATS; i++) {
                // selecting 1 from 100 identical coins depends on the shuffle; this test will fail 1% of
                // the time run the test RANDOM_REPEATS times and only complain if all of them fail
                EXPECT_TRUE(wallet.SelectCoinsMinConf(90 * CENT, GetAdjustedTime(), 1, 6, vCoins,
                                                      setCoinsRet, nValueRet));
                EXPECT_TRUE(wallet.SelectCoinsMinConf(90 * CENT, GetAdjustedTime(), 1, 6, vCoins,
                                                      setCoinsRet2, nValueRet));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            EXPECT_NE(fails, RANDOM_REPEATS);
        }
        empty_wallet();
    }
}

#include "main.h"
#include "txdb.h"

CBlock BlockFromHex(const std::string& hex)
{
    CDataStream stream(ParseHex(hex), SER_NETWORK, PROTOCOL_VERSION);
    CBlock      block;
    stream >> block;
    return block;
}

static CBlockIndexSmartPtr InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return nullptr;

    // Return existing
    map<uint256, CBlockIndexSmartPtr>::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return mi->second;

    // Create new
    CBlockIndexSmartPtr pindexNew = boost::make_shared<CBlockIndex>();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi                    = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

CBlockIndexSmartPtr BlockIndexFromHex(const std::string& hex, bool lastBlock = false)
{
    CDataStream     stream(ParseHex(hex), SER_NETWORK, PROTOCOL_VERSION);
    CDiskBlockIndex diskindex;
    stream >> diskindex;

    uint256 blockHash = diskindex.GetBlockHash();

    // Construct block index object
    CBlockIndexSmartPtr pindexNew = InsertBlockIndex(blockHash);
    pindexNew->pprev              = InsertBlockIndex(diskindex.hashPrev);
    if (!lastBlock) {
        pindexNew->pnext = InsertBlockIndex(diskindex.hashNext);
    } else {
        pindexNew->pnext = nullptr;
    }
    pindexNew->blockKeyInDB   = diskindex.blockKeyInDB;
    pindexNew->nHeight        = diskindex.nHeight;
    pindexNew->nMint          = diskindex.nMint;
    pindexNew->nMoneySupply   = diskindex.nMoneySupply;
    pindexNew->nFlags         = diskindex.nFlags;
    pindexNew->nStakeModifier = diskindex.nStakeModifier;
    pindexNew->prevoutStake   = diskindex.prevoutStake;
    pindexNew->nStakeTime     = diskindex.nStakeTime;
    pindexNew->hashProof      = diskindex.hashProof;
    pindexNew->nVersion       = diskindex.nVersion;
    pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
    pindexNew->nTime          = diskindex.nTime;
    pindexNew->nBits          = diskindex.nBits;
    pindexNew->nNonce         = diskindex.nNonce;

    hashBestChain = blockHash;

    return pindexNew;
}

TEST(blockindex_tests, create)
{
    fTestNet = true;

    // testnet genesis
    std::string genesisBlockHex =
        "010000000000000000000000000000000000000000000000000000000000000000000000e7ae9132c789d33c38b735a"
        "e562ef57c9780c7328b0d1cb0121a321432d13f20137a7259ffff7f20252100000101000000137a7259010000000000"
        "000000000000000000000000000000000000000000000000000000ffffffff2900012a2532316a756c32303137202d2"
        "04e65626c696f204669727374204e6574204c61756e63686573ffffffff010000000000000000000000000000";

    std::string genesisBlockIndexHex =
        "80841E00CEA03111D302B814C0EFFCEE90A08324D969E72D7EBDAB96AB0EDB99ADEC2E0ECC8BD29C351D1873BEB9CA2"
        "2E257652E251C47B74960253D46C1DBE42B978672000000000000000000000000000000000000000004000000000000"
        "0000000000CC8BD29C351D1873BEB9CA22E257652E251C47B74960253D46C1DBE42B978672010000000000000000000"
        "000000000000000000000000000000000000000000000000000E7AE9132C789D33C38B735AE562EF57C9780C7328B0D"
        "1CB0121A321432D13F20137A7259FFFF7F2025210000CC8BD29C351D1873BEB9CA22E257652E251C47B74960253D46C"
        "1DBE42B978672";

    CBlock genesisBlock = BlockFromHex(genesisBlockHex);

    /*CBlockIndex* genesisBlockIndex = */ BlockIndexFromHex(genesisBlockIndexHex, true);

    EXPECT_EQ(mapBlockIndex.size(), 1u);

    //    EXPECT_TRUE(ProcessBlock(nullptr, &genesisBlock));

    //    // can't import twice
    //    EXPECT_FALSE(ProcessBlock(nullptr, &genesisBlock));

    CBlock block;
    block.nVersion       = CBlock::CURRENT_VERSION;
    block.nTime          = GetAdjustedTime();
    block.hashMerkleRoot = block.BuildMerkleTree();
    block.nBits          = CBigNum(~uint256(0) >> 1).GetCompact();
    block.nNonce         = 0;
    block.vtx            = {};
    block.vchBlockSig    = {}; // for proof of work
    block.hashPrevBlock  = hashBestChain;
    EXPECT_TRUE(block.IsProofOfWork());

    /*uint256 bhash = */ block.GetHash();
    auto it = mapBlockIndex.find(hashBestChain);
    EXPECT_NE(it, mapBlockIndex.cend());

    //    IMPLEMENT_SERIALIZE(READWRITE(this->nVersion); nVersion = this->nVersion;
    //    READWRITE(hashPrevBlock);
    //                        READWRITE(hashMerkleRoot); READWRITE(nTime); READWRITE(nBits);
    //                        READWRITE(nNonce);

    //                        // ConnectBlock depends on vtx following header to generate CDiskTxPos
    //                        if (!(nType & (SER_GETHASH | SER_BLOCKHEADERONLY))) {
    //                            READWRITE(vtx);
    //                            READWRITE(vchBlockSig);
    //                        } else if (fRead) {
    //                            const_cast<CBlock*>(this)->vtx.clear();
    //                            const_cast<CBlock*>(this)->vchBlockSig.clear();
    //                        })
}

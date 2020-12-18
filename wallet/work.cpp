#include "work.h"

#include "globals.h"
#include "txdb-lmdb.h"
#include "util.h"

// miner's coin base reward
int64_t GetProofOfWorkReward(int64_t nFees)
{
    // Miner reward: 2000 coin for 500 Blocks = 1,000,000 coin
    int64_t nSubsidy = 2000 * COIN;

    const int bestHeight = CTxDB().GetBestChainHeight().value_or(0);

    if (bestHeight == 0) {
        // Total premine coin, after the first 501 blocks are mined there will be a total of 125,000,000
        nSubsidy = 124000000 * COIN;
    }

    // 0 reward for PoW blocks after 500
    if (bestHeight > 500) {
        nSubsidy = 0;
    }

    if (fDebug)
        printf("GetProofOfWorkReward() : create=%s nSubsidy=%" PRId64 "\n",
               FormatMoney(nSubsidy).c_str(), nSubsidy);

    return nSubsidy + nFees;
}

// miner's coin stake reward based on coin age spent (coin-days)
int64_t GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees)
{
    // CBlockLocator locator;

    int64_t nRewardCoinYear = COIN_YEAR_REWARD; // 10% reward up to end

    printf("Block Number %d \n", CTxDB().GetBestChainHeight().value_or(0));

    int64_t nSubsidy = nCoinAge * nRewardCoinYear * 33 / (365 * 33 + 8);
    printf("coin-Subsidy %" PRId64 "\n", nSubsidy);
    printf("coin-Age %" PRId64 "\n", nCoinAge);
    printf("Coin Reward %" PRId64 "\n", nRewardCoinYear);
    if (fDebug)
        printf("GetProofOfStakeReward(): create=%s nCoinAge=%" PRId64 "\n",
               FormatMoney(nSubsidy).c_str(), nCoinAge);

    return nSubsidy + nFees;
}

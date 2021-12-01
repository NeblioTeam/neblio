#include "work.h"

#include "globals.h"
#include "txdb-lmdb.h"
#include "util.h"

// miner's coin base reward
int64_t GetProofOfWorkReward(const ITxDB& txdb, int64_t nFees)
{
    // Miner reward: 2000 coin for 500 Blocks = 1,000,000 coin
    int64_t nSubsidy = 2000 * COIN;

    const int bestHeight = txdb.GetBestChainHeight().value_or(0);

    if (bestHeight == 0) {
        // Total premine coin, after the first 501 blocks are mined there will be a total of 125,000,000
        nSubsidy = 124000000 * COIN;
    }

    // 0 reward for PoW blocks after 500
    if (bestHeight > 500) {
        nSubsidy = 0;
    }

    if (fDebug)
        NLog.write(b_sev::debug, "GetProofOfWorkReward() : create={} nSubsidy={}", FormatMoney(nSubsidy),
                   nSubsidy);

    return nSubsidy + nFees;
}

// miner's coin stake reward based on coin age spent (coin-days)
int64_t GetProofOfStakeReward(const ITxDB& txdb, int64_t nCoinAge, int64_t nFees)
{
    // CBlockLocator locator;

    int64_t nRewardCoinYear = COIN_YEAR_REWARD; // 10% reward up to end

    NLog.write(b_sev::info, "Block Number {}", txdb.GetBestChainHeight().value_or(0));

    int64_t nSubsidy = nCoinAge * nRewardCoinYear * 33 / (365 * 33 + 8);
    NLog.write(b_sev::info, "coin-Subsidy {}", nSubsidy);
    NLog.write(b_sev::info, "coin-Age {}", nCoinAge);
    NLog.write(b_sev::info, "Coin Reward {}", nRewardCoinYear);
    if (fDebug)
        NLog.write(b_sev::debug, "GetProofOfStakeReward(): create={} nCoinAge={}", FormatMoney(nSubsidy),
                   nCoinAge);

    return nSubsidy + nFees;
}

//
// maximum nBits value could possible be required nTime after
//
unsigned int ComputeMaxBits(CBigNum bnTargetLimit, unsigned int nBase, int64_t nTime)
{
    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    bnResult *= 2;
    while (nTime > 0 && bnResult < bnTargetLimit) {
        // Maximum 200% adjustment per day...
        bnResult *= 2;
        nTime -= 24 * 60 * 60;
    }
    if (bnResult > bnTargetLimit)
        bnResult = bnTargetLimit;
    return bnResult.GetCompact();
}

//
// minimum amount of work that could possibly be required nTime after
// minimum proof-of-work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    return ComputeMaxBits(Params().PoWLimit(), nBase, nTime);
}

//
// minimum amount of stake that could possibly be required nTime after
// minimum proof-of-stake required was nBase
//
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int /*nBlockTime*/)
{
    return ComputeMaxBits(Params().PoSLimit(), nBase, nTime);
}

bool CheckProofOfWork(const uint256& hash, unsigned int nBits, bool silent)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > Params().PoWLimit()) {
        if (silent) {
            return false;
        } else {
            return NLog.error("CheckProofOfWork() : nBits below minimum work");
        }
    }

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256()) {
        if (silent) {
            return false;
        } else {
            return NLog.error("CheckProofOfWork() : hash doesn't match nBits");
        }
    }

    return true;
}

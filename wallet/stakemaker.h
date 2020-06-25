#ifndef STAKEMAKER_H
#define STAKEMAKER_H

#include "amount.h"
#include "key.h"
#include "script.h"
#include "transaction.h"
#include "txin.h"
#include <boost/optional.hpp>

class CWallet;
class CWalletTx;
class CKeyStore;

struct StakeKernelData
{
    CScript          kernelScriptPubKey;
    CScript          stakeOutputScriptPubKey;
    CKey             key;
    CAmount          credit = 0;
    CTxIn            kernelInput;
    int64_t          kernelBlockTime = 0;
    const CWalletTx* kernelTx        = nullptr;
    int64_t          stakeTxTime     = 0;
};

struct KernelScriptPubKeyResult
{
    KernelScriptPubKeyResult(const CScript& ScriptPubKey, const CKey& Key)
        : scriptPubKey(ScriptPubKey), key(Key)
    {
    }
    KernelScriptPubKeyResult(CScript&& ScriptPubKey, CKey&& Key)
        : scriptPubKey(std::move(ScriptPubKey)), key(std::move(Key))
    {
    }
    CScript scriptPubKey;
    CKey    key;
};

struct CoinStakeData
{
    CTransaction coinStakeTx;
    CKey         key;
};

struct CoinStakeInputsResult
{
    std::vector<CTxIn>            inputs;
    std::vector<const CWalletTx*> inputsPrevouts;
    CAmount                       nInputsTotalCredit = 0;
};

class StakeMaker
{
    boost::atomic_int64_t nLastCoinStakeSearchTime{0};
    boost::atomic_int64_t nLastCoinStakeSearchInterval{0};

public:
    StakeMaker() = default;
    boost::optional<CoinStakeData> CreateCoinStake(const CWallet& wallet, const unsigned int nBits,
                                                   const CAmount nFees, const CAmount reservedBalance);
    boost::optional<StakeKernelData>
    FindStakeKernel(const CKeyStore& keystore, const unsigned int nBits,
                    const int64_t                                              nCoinstakeInitialTxTime,
                    const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins);
    boost::optional<KernelScriptPubKeyResult>
    CalculateScriptPubKeyForStakeOutput(const CKeyStore& keystore, const CScript& scriptPubKeyKernel);
    CoinStakeInputsResult
                        CollectInputsForStake(const StakeKernelData&                                     kernelData,
                                              const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins,
                                              const int64_t txTime, const bool splitStake, const CAmount nBalance,
                                              const CAmount reservedBalance);
    std::vector<CTxOut> MakeStakeOutputs(const StakeKernelData& kernelData, const CAmount totalCredit,
                                         const bool splitStake);
    void                UpdateStakeSearchTimes(const int64_t nSearchTime);
    void                resetLastCoinStakeSearchInterval();
    int64_t             getLastCoinStakeSearchInterval() const;
    int64_t             getLastCoinStakeSearchTime() const;
};

#endif // STAKEMAKER_H

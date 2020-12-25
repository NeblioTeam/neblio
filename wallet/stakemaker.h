#ifndef STAKEMAKER_H
#define STAKEMAKER_H

#include "amount.h"
#include "key.h"
#include "script.h"
#include "transaction.h"
#include "txin.h"
#include <boost/optional.hpp>
#include <mutex>

class CWallet;
class CWalletTx;
class CKeyStore;

struct StakeKernelData
{
    CScript             kernelScriptPubKey;
    CScript             stakeOutputScriptPubKey;
    CKey                key;
    CAmount             credit = 0;
    CTxIn               kernelInput;
    int64_t             kernelBlockTime = 0;
    const CTransaction* kernelTx        = nullptr;
    int64_t             stakeTxTime     = 0;
};

struct CoinStakeInputsResult
{
    std::vector<CTxIn>               inputs;
    std::vector<const CTransaction*> inputsPrevouts;
    CAmount                          nInputsTotalCredit = 0;
};

class StakeMaker
{
    boost::atomic_int64_t nLastCoinStakeSearchTime{0};
    boost::atomic_int64_t nLastCoinStakeSearchInterval{0};
    std::once_flag        timeSetterOnceFlag;

    CachedKeyValueWithOptions<uint256, CAmount> cachedBalance;
    // cached outputs with their total value
    CachedKeyValueWithOptions<uint256,
                              std::pair<CAmount, std::set<std::pair<const CWalletTx*, unsigned int>>>>
                                             cachedSelectedOutputs;
    boost::atomic<boost::optional<uint64_t>> cachedStakeWeight;

    void updateStakeWeight(const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins);

public:
    StakeMaker() = default;

    using KeyGetterFunctorType = std::function<boost::optional<CKey>(const CKeyID&)>;

    struct DefaultKeyGetter
    {
        const CKeyStore& keystore;
        DefaultKeyGetter(const CKeyStore& Keystore) : keystore(Keystore) {}

        boost::optional<CKey> operator()(const CKeyID& keyID)
        {
            CKey result;
            if (!keystore.GetKey(keyID, result)) {
                return boost::none;
            }
            return boost::make_optional(std::move(result));
        }
    };

    boost::optional<CTransaction> CreateCoinStake(
        const CWallet& wallet, unsigned int nBits, CAmount nFees, CAmount reservedBalance,
        const boost::optional<std::set<std::pair<uint256, unsigned>>>& customInputs        = boost::none,
        CAmount                                                        extraPayoutForTests = 0);

    boost::optional<CTransaction> CreateCoinStakeFromSpecificOutput(const COutPoint& output,
                                                                    const CKey&      spendKeyOfOutput,
                                                                    unsigned int nBits, CAmount nFees);

    boost::optional<StakeKernelData>
    FindStakeKernel(const CKeyStore& keystore, unsigned int nBits, int64_t nCoinstakeInitialTxTime,
                    const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins);
    static boost::optional<CScript>
                CalculateScriptPubKeyForStakeOutput(const KeyGetterFunctorType& keyGetter,
                                                    const CScript&              scriptPubKeyKernel);
    static bool SignAndVerify(const CKeyStore& keystore, const CoinStakeInputsResult inputs,
                              CTransaction& stakeTx);
    static CoinStakeInputsResult
                               CollectInputsForStake(const StakeKernelData&                                     kernelData,
                                                     const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins,
                                                     int64_t txTime, bool splitStake, CAmount nBalance, CAmount reservedBalance);
    static std::vector<CTxOut> MakeStakeOutputs(const CScript& outputScriptPubKey, CAmount totalCredit,
                                                bool splitStake);
    void                       UpdateStakeSearchTimes(int64_t nSearchTime);
    void                       resetLastCoinStakeSearchInterval();
    int64_t                    getLastCoinStakeSearchInterval() const;
    int64_t                    getLastCoinStakeSearchTime() const;
    boost::optional<uint64_t>  getLatestStakeWeight() const;
};

#endif // STAKEMAKER_H

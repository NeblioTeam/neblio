#include "stakemaker.h"

#include "block.h"
#include "kernel.h"
#include "wallet.h"
#include "work.h"
#include <mutex>

std::once_flag flag;

int64_t StakeMaker::getLastCoinStakeSearchInterval() const { return nLastCoinStakeSearchInterval; }

int64_t StakeMaker::getLastCoinStakeSearchTime() const { return nLastCoinStakeSearchTime; }

boost::optional<CTransaction> StakeMaker::CreateCoinStake(const CWallet&     wallet,
                                                          const unsigned int nBits, const CAmount nFees,
                                                          const CAmount reservedBalance)
{
    // we set the startup time only once
    std::call_once(flag, [&]() { nLastCoinStakeSearchTime = GetAdjustedTime(); });

    const bool fEnableColdStaking = GetBoolArg("-coldstaking", true);

    // Choose coins to use
    const CAmount nBalance = wallet.GetStakingBalance(fEnableColdStaking);

    if (nBalance <= reservedBalance)
        return boost::none;

    const int64_t nCoinstakeInitialTxTime = GetAdjustedTime();

    // no point in searching times that we aleady visited (this is zero interval)
    if (nCoinstakeInitialTxTime <= nLastCoinStakeSearchTime) {
        UpdateStakeSearchTimes(nCoinstakeInitialTxTime);
        return boost::none;
    }

    // Select coins with suitable depth
    std::set<std::pair<const CWalletTx*, unsigned int>> setCoins;
    CAmount                                             nValueIn = 0;
    if (!wallet.SelectCoinsForStaking(nBalance - reservedBalance, nCoinstakeInitialTxTime, setCoins,
                                      nValueIn, fEnableColdStaking, false))
        return boost::none;

    if (setCoins.empty())
        return boost::none;

    // since time search goes backwards, and there's potential for tx time to go back, we store it to
    // use it later in UpdateStakeSearchTimes()
    boost::optional<StakeKernelData> kernelData =
        FindStakeKernel(wallet, nBits, nCoinstakeInitialTxTime, setCoins);
    UpdateStakeSearchTimes(nCoinstakeInitialTxTime);

    // stake was not found
    if (!kernelData) {
        return boost::none;
    }

    /** stake found! */

    if (kernelData->credit == 0 || kernelData->credit > nBalance - reservedBalance)
        return boost::none;

    CTransaction stakeTx;
    stakeTx.nTime = kernelData->stakeTxTime;

    const bool splitStake =
        GetWeight(kernelData->kernelBlockTime, kernelData->stakeTxTime) < Params().StakeSplitAge();

    const CoinStakeInputsResult inputs = CollectInputsForStake(*kernelData, setCoins, stakeTx.nTime,
                                                               splitStake, nBalance, reservedBalance);

    stakeTx.vin = inputs.inputs;

    // Calculate coin age and reward
    CAmount nFinalCredit = inputs.nInputsTotalCredit;
    {
        uint64_t nCoinAge;
        CTxDB    txdb("r");
        if (!stakeTx.GetCoinAge(txdb, nCoinAge)) {
            printf("CreateCoinStake : failed to calculate coin age");
            return boost::none;
        }

        const CAmount nReward = GetProofOfStakeReward(nCoinAge, nFees);
        if (nReward <= 0)
            return boost::none;

        // add reward to total credit
        nFinalCredit += nReward;
    }

    stakeTx.vout = MakeStakeOutputs(kernelData->stakeOutputScriptPubKey, nFinalCredit, splitStake);

    // Sign
    for (unsigned i = 0; i < inputs.inputsPrevouts.size(); i++) {
        const CWalletTx* pcoin = inputs.inputsPrevouts[i];
        if (!SignSignature(wallet, *pcoin, stakeTx, i, SIGHASH_ALL, true)) {
            printf("CreateCoinStake : failed to sign coinstake");
            return boost::none;
        }
    }

    // Limit size
    unsigned int nBytes = ::GetSerializeSize(stakeTx, SER_NETWORK, PROTOCOL_VERSION);
    if (nBytes >= OLD_MAX_BLOCK_SIZE / 5) {
        printf("CreateCoinStake : exceeded coinstake size limit");
        return boost::none;
    }

    // Successfully generated coinstake
    return stakeTx;
}

boost::optional<CScript>
StakeMaker::CalculateScriptPubKeyForStakeOutput(const CKeyStore& keystore,
                                                const CScript&   scriptPubKeyKernel)
{
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    CKey                 key;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions)) {
        if (fDebug)
            printf("CalculateScriptPubKeyForStakeOutput : failed to parse kernel\n");
        return boost::none;
    }
    if (fDebug)
        printf("CalculateScriptPubKeyForStakeOutput : parsed kernel type=%d\n", whichType);
    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH && whichType != TX_COLDSTAKE) {
        if (fDebug)
            printf("CalculateScriptPubKeyForStakeOutput : no support for kernel type=%d\n", whichType);
        return boost::none; // only support pay to public key and pay to address
    }

    switch (whichType) {
    case TX_PUBKEYHASH: // pay to address type
    {
        // convert to pay to public key type
        if (!keystore.GetKey(uint160(vSolutions[0]), key)) {
            if (fDebug)
                printf("CalculateScriptPubKeyForStakeOutput : failed to get key for kernel type=%d\n",
                       whichType);
            return boost::none; // unable to find corresponding public key
        }
        return CScript() << key.GetPubKey() << OP_CHECKSIG;
    }

    case TX_PUBKEY: // pay to public key
    {
        if (!Params().IsColdStakingEnabled()) {
            return boost::none;
        }
        const valtype& vchPubKey = vSolutions[0];
        if (!keystore.GetKey(Hash160(vchPubKey), key)) {
            if (fDebug)
                printf("CalculateScriptPubKeyForStakeOutput : failed to get key for kernel type=%d\n",
                       whichType);
            return boost::none; // unable to find corresponding public key
        }

        if (key.GetPubKey() != vchPubKey) {
            if (fDebug)
                printf("CalculateScriptPubKeyForStakeOutput : invalid key for kernel P2PK type=%d\n",
                       whichType);
            return boost::none; // keys mismatch
        }
        return scriptPubKeyKernel;
    }
    case TX_COLDSTAKE: {
        if (!keystore.GetKey(CKeyID(uint160(vSolutions[0])), key)) {
            printf(
                "CalculateScriptPubKeyForStakeOutput : failed to get key for kernel coldstake type=%d\n",
                whichType);
            return boost::none;
        }
        return scriptPubKeyKernel;
    }
    case TX_SCRIPTHASH:
    case TX_MULTISIG:
    case TX_NULL_DATA:
    case TX_NONSTANDARD:
        break;
    }

    if (fDebug)
        printf(
            "CalculateScriptPubKeyForStakeOutput : Unsupported scriptPubKey type for staking type=%d\n",
            whichType);
    return boost::none;
}

boost::optional<StakeKernelData>
StakeMaker::FindStakeKernel(const CKeyStore& keystore, const unsigned int nBits,
                            const int64_t nCoinstakeInitialTxTime,
                            const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins)
{
    StakeKernelData coinStake;

    const int64_t nSearchInterval = nCoinstakeInitialTxTime - nLastCoinStakeSearchTime;

    CBlockIndexSmartPtr pindexPrev = boost::atomic_load(&pindexBest);

    CTxDB txdb("r");

    for (const auto& pcoin : setCoins) {
        CTxIndex txindex;
        CBlock   kernelBlock;
        {
            LOCK(cs_main);

            if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
                continue;

            // Read block header
            if (!kernelBlock.ReadFromDisk(txindex.pos.nBlockPos, false))
                continue;
        }

        const int          nMaxStakeSearchInterval = Params().MaxStakeSearchInterval();
        const unsigned int nSMA                    = Params().StakeMinAge();
        if (kernelBlock.GetBlockTime() + nSMA > nCoinstakeInitialTxTime - nMaxStakeSearchInterval)
            continue; // only count coins meeting min age requirement

        for (unsigned int n = 0; n < std::min(nSearchInterval, (int64_t)nMaxStakeSearchInterval) &&
                                 !fShutdown && pindexPrev == pindexBest;
             n++) {
            // Search backward in time from the given tx timestamp
            // Search nSearchInterval seconds back up to nMaxStakeSearchInterval
            uint256       hashProofOfStake = 0, targetProofOfStake = 0;
            COutPoint     prevoutStake    = COutPoint(pcoin.first->GetHash(), pcoin.second);
            const int64_t txCoinstakeTime = nCoinstakeInitialTxTime - n;
            if (!CheckStakeKernelHash(nBits, kernelBlock, txindex.pos.nTxPos, *pcoin.first, prevoutStake,
                                      txCoinstakeTime, hashProofOfStake, targetProofOfStake)) {
                continue;
            }

            // Found a kernel
            if (fDebug)
                printf("FindStakeKernel : kernel found\n");

            const CScript& kernelScriptPubKey = pcoin.first->vout[pcoin.second].scriptPubKey;

            const boost::optional<CScript> spkKernel =
                CalculateScriptPubKeyForStakeOutput(keystore, kernelScriptPubKey);

            if (!spkKernel) {
                if (fDebug)
                    printf("FindStakeKernel : failed to get scriptPubKey for kernel");
                continue;
            }

            // Fill coin stake transaction
            coinStake.kernelScriptPubKey      = kernelScriptPubKey;
            coinStake.credit                  = pcoin.first->vout[pcoin.second].nValue;
            coinStake.kernelTx                = pcoin.first;
            coinStake.kernelBlockTime         = kernelBlock.GetBlockTime();
            coinStake.kernelInput             = CTxIn(pcoin.first->GetHash(), pcoin.second);
            coinStake.stakeTxTime             = txCoinstakeTime;
            coinStake.stakeOutputScriptPubKey = *spkKernel;

            return coinStake;
        }
    }
    return boost::none;
}

CoinStakeInputsResult
StakeMaker::CollectInputsForStake(const StakeKernelData&                                     kernelData,
                                  const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins,
                                  const int64_t txTime, const bool splitStake, const CAmount nBalance,
                                  const CAmount reservedBalance)
{
    CoinStakeInputsResult result;

    bool isColdStake = kernelData.kernelScriptPubKey.IsPayToColdStaking();

    // add the kernel input
    result.inputs.push_back(kernelData.kernelInput);
    result.inputsPrevouts.push_back(kernelData.kernelTx);
    result.nInputsTotalCredit = kernelData.credit;

    if (!splitStake && !isColdStake) {
        // Attempt to add more inputs
        for (PAIRTYPE(const CWalletTx*, unsigned int) pcoin : setCoins) {
            // Only add coins of the same key/address as kernel
            const unsigned int nSMA    = Params().StakeMinAge();
            const CTxOut&      prevout = pcoin.first->vout[pcoin.second];

            const bool sameScriptPubKeyAsKernelOrStakeOutput =
                prevout.scriptPubKey == kernelData.kernelScriptPubKey ||
                prevout.scriptPubKey == kernelData.stakeOutputScriptPubKey;

            const bool isTheKernelWeAlreadyHave =
                pcoin.first->GetHash() == result.inputs[0].prevout.hash;

            if (sameScriptPubKeyAsKernelOrStakeOutput && !isTheKernelWeAlreadyHave) {

                const int64_t nTimeWeight = GetWeight((int64_t)pcoin.first->nTime, txTime);

                // Stop adding more inputs if already too many inputs
                if (result.inputs.size() >= Params().MaxInputsInStake())
                    break;
                // Stop adding more inputs if value is already pretty significant
                if (result.nInputsTotalCredit >= Params().StakeCombineThreshold())
                    break;
                // Stop adding inputs if reached reserve limit
                if (result.nInputsTotalCredit + prevout.nValue > nBalance - reservedBalance)
                    break;
                // Do not add additional significant input
                if (prevout.nValue >= Params().StakeCombineThreshold())
                    continue;
                // Do not add input that is still too young
                if (nTimeWeight < nSMA)
                    continue;

                result.inputs.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
                result.nInputsTotalCredit += prevout.nValue;
                result.inputsPrevouts.push_back(pcoin.first);
            }
        }
    }
    return result;
}

std::vector<CTxOut> StakeMaker::MakeStakeOutputs(const CScript& outputScriptPubKey,
                                                 const CAmount totalCredit, const bool splitStake)
{
    std::vector<CTxOut> result;

    // add coinstake marker
    result.push_back(CTxOut(0, CScript()));

    // add outputs
    if (splitStake) {
        const CAmount amount1 = (totalCredit / 2 / CENT) * CENT;
        const CAmount amount2 = totalCredit - amount1;
        result.push_back(CTxOut(amount1, outputScriptPubKey));
        result.push_back(CTxOut(amount2, outputScriptPubKey));
    } else {
        result.push_back(CTxOut(totalCredit, outputScriptPubKey));
    }

    return result;
}

void StakeMaker::UpdateStakeSearchTimes(const int64_t nSearchTime)
{
    nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
    nLastCoinStakeSearchTime     = nSearchTime;
}

void StakeMaker::resetLastCoinStakeSearchInterval() { nLastCoinStakeSearchInterval = 0; }

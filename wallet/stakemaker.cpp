#include "stakemaker.h"

#include "block.h"
#include "kernel.h"
#include "wallet.h"
#include "work.h"

int64_t StakeMaker::getLastCoinStakeSearchInterval() const { return nLastCoinStakeSearchInterval; }

int64_t StakeMaker::getLastCoinStakeSearchTime() const { return nLastCoinStakeSearchTime; }

boost::optional<uint64_t> StakeMaker::getLatestStakeWeight() const { return cachedStakeWeight; }

CoinStakeInputsResult MakeInitialStakeInputsResult(const StakeKernelData& kernelData)
{
    CoinStakeInputsResult result;

    // add the kernel input
    result.inputs.push_back(kernelData.kernelInput);
    result.inputsPrevouts.push_back(kernelData.kernelTx);
    result.nInputsTotalCredit = kernelData.credit;

    return result;
}

bool IsStakeTxSizeValid(const CTransaction& stakeTx)
{
    const unsigned int nBytes = ::GetSerializeSize(stakeTx, SER_NETWORK, PROTOCOL_VERSION);
    if (nBytes >= OLD_MAX_BLOCK_SIZE / 5) {
        return false;
    }
    return true;
}

boost::optional<StakeKernelData>
TestAndCreateStakeKernel(const CTxDB& txdb, const StakeMaker::KeyGetterFunctorType& keyGetter,
                         const unsigned int nBits, const int64_t nCoinstakeInitialTxTime,
                         const int64_t                                       lastCoinStakeSearchTime,
                         const ConstCBlockIndexSmartPtr&                     pindexPrev,
                         const std::pair<const CTransaction*, unsigned int>& pcoin)
{
    CTxIndex txindex;
    CBlock   kernelBlock;
    {
        // LOCK(cs_main); // Seems unnecessary, since we only read from DB

        if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
            return boost::none;

        // Read block header
        if (!kernelBlock.ReadFromDisk(txindex.pos.nBlockPos, false))
            return boost::none;
    }

    const int64_t nSearchInterval = nCoinstakeInitialTxTime - lastCoinStakeSearchTime;

    const int          nMaxStakeSearchInterval = Params().MaxStakeSearchInterval();
    const unsigned int nSMA                    = Params().StakeMinAge(txdb);
    if (kernelBlock.GetBlockTime() + nSMA > nCoinstakeInitialTxTime - nMaxStakeSearchInterval)
        return boost::none; // only count coins meeting min age requirement

    for (unsigned int n = 0; n < std::min(nSearchInterval, (int64_t)nMaxStakeSearchInterval) &&
                             !fShutdown && pindexPrev == txdb.GetBestBlockIndex();
         n++) {
        // Search backward in time from the given tx timestamp
        // Search nSearchInterval seconds back up to nMaxStakeSearchInterval
        uint256       hashProofOfStake = 0, targetProofOfStake = 0;
        COutPoint     prevoutStake    = COutPoint(pcoin.first->GetHash(), pcoin.second);
        const int64_t txCoinstakeTime = nCoinstakeInitialTxTime - n;
        if (!CheckStakeKernelHash(txdb, nBits, kernelBlock, txindex.pos.nTxPos, *pcoin.first,
                                  prevoutStake, txCoinstakeTime, hashProofOfStake, targetProofOfStake)) {
            continue;
        }

        // Found a kernel
        if (fDebug)
            printf("FindStakeKernel : kernel found\n");

        const CScript& kernelScriptPubKey = pcoin.first->vout[pcoin.second].scriptPubKey;

        const boost::optional<CScript> spkKernel =
            StakeMaker::CalculateScriptPubKeyForStakeOutput(txdb, keyGetter, kernelScriptPubKey);

        if (!spkKernel) {
            if (fDebug)
                printf("FindStakeKernel : failed to get scriptPubKey for kernel");
            continue;
        }

        StakeKernelData coinStake;

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
    return boost::none;
}

boost::optional<CAmount> CalculateStakeReward(const ITxDB& txdb, const CTransaction& stakeTx,
                                              CAmount nFees, CAmount extraPayoutForTests = 0)
{
    CAmount result = 0;

    uint64_t nCoinAge;
    if (!stakeTx.GetCoinAge(txdb, nCoinAge)) {
        printf("CreateCoinStake : failed to calculate coin age");
        return boost::none;
    }

    const CAmount nReward = GetProofOfStakeReward(nCoinAge, nFees);
    if (nReward <= 0)
        return boost::none;

    // add reward to total credit
    result += nReward;
    result += extraPayoutForTests;

    return boost::make_optional(result);
}

void StakeMaker::updateStakeWeight(const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins)
{
    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    if (CWallet::GetStakeWeight(setCoins, nMinWeight, nMaxWeight, nWeight)) {
        cachedStakeWeight = nWeight;
    } else {
        cachedStakeWeight = boost::none;
    }
}

boost::optional<CTransaction>
StakeMaker::CreateCoinStake(const ITxDB& txdb, const CWallet& wallet, const unsigned int nBits,
                            const CAmount nFees, const CAmount reservedBalance,
                            const boost::optional<std::set<std::pair<uint256, unsigned>>>& customInputs,
                            const CAmount extraPayoutForTests)
{
    // we set the startup time only once
    std::call_once(timeSetterOnceFlag, [&]() { nLastCoinStakeSearchTime = GetAdjustedTime(); });

    const bool fEnableColdStaking = GetBoolArg("-coldstaking", true);

    const uint256 currentBestBlock = txdb.GetBestBlockHash();

    // Choose coins to use
    const CAmount nBalance = [&]() {
        boost::optional<CAmount> cachedBalanceValue = cachedBalance.getValue(currentBestBlock);
        if (cachedBalanceValue) {
            return *cachedBalanceValue;
        } else {
            const CAmount res = wallet.GetStakingBalance(fEnableColdStaking);
            cachedBalance.update(currentBestBlock, res);
            return res;
        }
    }();

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
    const auto cachedOutputs = cachedSelectedOutputs.getValue(currentBestBlock);
    if (cachedOutputs) {
        std::tie(nValueIn, setCoins) = *cachedOutputs;
    } else {
        if (!wallet.SelectCoinsForStaking(nBalance - reservedBalance, nCoinstakeInitialTxTime, setCoins,
                                          nValueIn, fEnableColdStaking, false)) {
            // failure to get coins means they're spent. We reset stake weight
            cachedStakeWeight = boost::none;
            return boost::none;
        }
        cachedSelectedOutputs.update(currentBestBlock, std::make_pair(nValueIn, setCoins));
    }

    updateStakeWeight(setCoins);

    // we can choose custom inputs to use (by filtering the ones we get from the wallet) for testing
    // purposes
    if (customInputs.is_initialized()) {
        decltype(setCoins) toErase;
        for (const auto& pcoin : setCoins) {
            auto inputIt = customInputs->find(std::make_pair(pcoin.first->GetHash(), pcoin.second));
            if (inputIt == customInputs->cend()) {
                toErase.insert(pcoin);
            }
        }
        for (const auto& pcoin : toErase) {
            setCoins.erase(pcoin);
        }
    }

    if (setCoins.empty())
        return boost::none;

    // since time search goes backwards, and there's potential for tx time to go back, we store it to
    // use it later in UpdateStakeSearchTimes()
    const boost::optional<StakeKernelData> kernelData =
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
        GetWeight(txdb, kernelData->kernelBlockTime, kernelData->stakeTxTime) < Params().StakeSplitAge();

    const CoinStakeInputsResult inputs = CollectInputsForStake(
        txdb, *kernelData, setCoins, stakeTx.nTime, splitStake, nBalance, reservedBalance);

    stakeTx.vin = inputs.inputs;

    // Calculate coin age and reward
    CAmount nFinalCredit = inputs.nInputsTotalCredit;

    const boost::optional<CAmount> oReward =
        CalculateStakeReward(txdb, stakeTx, nFees, extraPayoutForTests);
    if (!oReward) {
        return boost::none;
    }
    nFinalCredit += *oReward;

    stakeTx.vout = MakeStakeOutputs(kernelData->stakeOutputScriptPubKey, nFinalCredit, splitStake);

    if (!SignAndVerify(wallet, inputs, stakeTx)) {
        printf("CreateCoinStake : SignAndVerify() failed");
        return boost::none;
    }

    // Limit size
    if (!IsStakeTxSizeValid(stakeTx)) {
        printf("CreateCoinStake : exceeded coinstake size limit");
        return boost::none;
    }

    // Successfully generated coinstake
    return stakeTx;
}

boost::optional<CTransaction> StakeMaker::CreateCoinStakeFromSpecificOutput(const COutPoint& output,
                                                                            const CKey& spendKeyOfOutput,
                                                                            unsigned int nBits,
                                                                            CAmount      nFees)
{
    // we set the startup time only once
    std::call_once(timeSetterOnceFlag, [&]() {
        nLastCoinStakeSearchTime = GetAdjustedTime() - Params().MaxStakeSearchInterval();
    });

    const int64_t nCoinstakeInitialTxTime = GetAdjustedTime();

    // no point in searching times that we aleady visited (this is zero interval)
    if (nCoinstakeInitialTxTime <= nLastCoinStakeSearchTime) {
        UpdateStakeSearchTimes(nCoinstakeInitialTxTime);
        return boost::none;
    }

    const CTxDB              txdb("r");
    ConstCBlockIndexSmartPtr pindexPrev = txdb.GetBestBlockIndex();

    const auto keyGetter = [&spendKeyOfOutput](const CKeyID&) {
        return boost::make_optional(spendKeyOfOutput);
    };

    CTxIndex     txindex;
    CTransaction outputTx;
    {
        LOCK(cs_main);

        if (!txdb.ReadTxIndex(output.hash, txindex))
            return boost::none;

        if (!txdb.ReadTx(txindex.pos, outputTx))
            return boost::none;
    }

    if (output.n >= outputTx.vout.size()) {
        printf("Invalid output index %u >= %zu", output.n, outputTx.vout.size());
        return boost::none;
    }

    const boost::optional<StakeKernelData> kernelData = TestAndCreateStakeKernel(
        txdb, keyGetter, nBits, nCoinstakeInitialTxTime, nLastCoinStakeSearchTime, pindexPrev,
        std::make_pair(&outputTx, output.n));

    // stake was not found
    if (!kernelData) {
        return boost::none;
    }

    CTransaction stakeTx;
    stakeTx.nTime = kernelData->stakeTxTime;

    const bool splitStake =
        GetWeight(txdb, kernelData->kernelBlockTime, kernelData->stakeTxTime) < Params().StakeSplitAge();

    const CoinStakeInputsResult inputs = MakeInitialStakeInputsResult(*kernelData);

    stakeTx.vin = inputs.inputs;

    // Calculate coin age and reward
    CAmount nFinalCredit = inputs.nInputsTotalCredit;

    const boost::optional<CAmount> oReward = CalculateStakeReward(txdb, stakeTx, nFees, 0);
    if (!oReward) {
        return boost::none;
    }
    nFinalCredit += *oReward;

    stakeTx.vout = MakeStakeOutputs(kernelData->stakeOutputScriptPubKey, nFinalCredit, splitStake);

    // create a temporary key store and store our key in it
    CBasicKeyStore keyStore;
    if (!keyStore.AddKey(spendKeyOfOutput)) {
        printf("Failed to add key to temporary key store");
        return boost::none;
    }

    if (!SignAndVerify(keyStore, inputs, stakeTx)) {
        printf("CreateCoinStake : SignAndVerify() failed");
        return boost::none;
    }

    // Limit size
    if (!IsStakeTxSizeValid(stakeTx)) {
        printf("CreateCoinStake : exceeded coinstake size limit");
        return boost::none;
    }

    // Successfully generated coinstake
    return stakeTx;
}

boost::optional<CScript>
StakeMaker::CalculateScriptPubKeyForStakeOutput(const ITxDB& txdb, const KeyGetterFunctorType& keyGetter,
                                                const CScript& scriptPubKeyKernel)
{
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    if (!Solver(txdb, scriptPubKeyKernel, whichType, vSolutions)) {
        if (fDebug)
            printf("CalculateScriptPubKeyForStakeOutput : failed to parse kernel\n");
        return boost::none;
    }
    if (fDebug)
        printf("CalculateScriptPubKeyForStakeOutput : parsed kernel type=%d\n", whichType);

    switch (whichType) {
    case TX_PUBKEYHASH: // pay to address type
    {
        // convert to pay to public key type
        const boost::optional<CKey> key = keyGetter(uint160(vSolutions[0]));
        if (!key) {
            if (fDebug)
                printf("CalculateScriptPubKeyForStakeOutput : failed to get key for kernel type=%d\n",
                       whichType);
            return boost::none; // unable to find corresponding public key
        }
        return CScript() << key->GetPubKey() << OP_CHECKSIG;
    }
    case TX_PUBKEY: // pay to public key
    {
        const valtype&              vchPubKey = vSolutions[0];
        const boost::optional<CKey> key       = keyGetter(Hash160(vchPubKey));
        if (!key) {
            if (fDebug)
                printf("CalculateScriptPubKeyForStakeOutput : failed to get key for kernel type=%d\n",
                       whichType);
            return boost::none; // unable to find corresponding public key
        }

        if (key->GetPubKey() != vchPubKey) {
            if (fDebug)
                printf("CalculateScriptPubKeyForStakeOutput : invalid key for kernel P2PK type=%d\n",
                       whichType);
            return boost::none; // keys mismatch
        }
        return scriptPubKeyKernel;
    }
    case TX_COLDSTAKE: {
        const boost::optional<CKey> key = keyGetter(CKeyID(uint160(vSolutions[0])));
        if (!key) {
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

bool StakeMaker::SignAndVerify(const CKeyStore& keystore, const CoinStakeInputsResult inputs,
                               CTransaction& stakeTx)
{
    // Sign
    std::vector<SignatureState> sigStates;
    for (unsigned i = 0; i < inputs.inputsPrevouts.size(); i++) {
        const CTransaction* pcoin = inputs.inputsPrevouts[i];
        SignatureState      sigState{SignatureState::Failed};
        if ((sigState = SignSignature(keystore, *pcoin, stakeTx, i, SIGHASH_ALL, true)) ==
            SignatureState::Failed) {
            printf("CreateCoinStake : failed to sign coinstake");
            return false;
        } else {
            sigStates.push_back(sigState);
        }
    }

    // ensure all signatures are successful (even though we checked already, no harm in double-checking)
    if (std::any_of(sigStates.cbegin(), sigStates.cend(),
                    [](const SignatureState& state) { return state == SignatureState::Failed; })) {
        printf("CreateCoinStake : failed to sign coinstake - WARNING: THIS SHOULD NEVER HAPPEN");
        return false;
    }

    // after signatures are done, we verify them if they're not verified
    for (unsigned i = 0; i < inputs.inputsPrevouts.size(); i++) {
        const CTransaction* pcoin = inputs.inputsPrevouts[i];
        const CTxIn&        txin  = stakeTx.vin[i];
        if (sigStates[i] == SignatureState::Verified) {
            continue;
        }
        if (VerifyScript(stakeTx.vin[i].scriptSig, pcoin->vout[txin.prevout.n].scriptPubKey, stakeTx, i,
                         true, true, 0)
                .isErr()) {
            printf("CreateCoinStake : Signature verification failed");
            return false;
        }
    }
    return true;
}

boost::optional<StakeKernelData>
StakeMaker::FindStakeKernel(const CKeyStore& keystore, const unsigned int nBits,
                            const int64_t nCoinstakeInitialTxTime,
                            const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins)
{
    CTxDB txdb("r");

    ConstCBlockIndexSmartPtr pindexPrev = txdb.GetBestBlockIndex();

    for (const auto& pcoin : setCoins) {
        if (boost::optional<StakeKernelData> res = TestAndCreateStakeKernel(
                txdb, StakeMaker::DefaultKeyGetter(keystore), nBits, nCoinstakeInitialTxTime,
                nLastCoinStakeSearchTime, pindexPrev, pcoin)) {
            return res;
        }
    }
    return boost::none;
}

CoinStakeInputsResult
StakeMaker::CollectInputsForStake(const ITxDB& txdb, const StakeKernelData& kernelData,
                                  const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins,
                                  const int64_t txTime, const bool splitStake, const CAmount nBalance,
                                  const CAmount reservedBalance)
{
    CoinStakeInputsResult result = MakeInitialStakeInputsResult(kernelData);

    if (!splitStake) {
        // Attempt to add more inputs
        for (PAIRTYPE(const CWalletTx*, unsigned int) pcoin : setCoins) {
            // Only add coins of the same key/address as kernel
            const unsigned int nSMA    = Params().StakeMinAge(txdb);
            const CTxOut&      prevout = pcoin.first->vout[pcoin.second];

            const bool sameScriptPubKeyAsKernelOrStakeOutput =
                prevout.scriptPubKey == kernelData.kernelScriptPubKey ||
                prevout.scriptPubKey == kernelData.stakeOutputScriptPubKey;

            const bool isTheKernelWeAlreadyHave =
                pcoin.first->GetHash() == result.inputs[0].prevout.hash;

            if (sameScriptPubKeyAsKernelOrStakeOutput && !isTheKernelWeAlreadyHave) {

                const int64_t nTimeWeight = GetWeight(txdb, (int64_t)pcoin.first->nTime, txTime);

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

bool StakeMaker::IsStakingActive()
{
    // returns true if we have weight and we tried to stake in the last 120 seconds
    return cachedStakeWeight.load().value_or(0) > 0 && (nLastCoinStakeSearchTime + 120) >= GetAdjustedTime();
}

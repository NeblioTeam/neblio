#include "coldstakedelegation.h"

#include "init.h"
#include "json_spirit.h"

json_spirit::Object CoinStakeDelegationResult::AddressesToJsonObject() const
{
    json_spirit::Object result;
    result.push_back(json_spirit::Pair("owner_address", ownerAddress.ToString()));
    result.push_back(json_spirit::Pair("staker_address", stakerAddress.ToString()));
    return result;
}

Result<CoinStakeDelegationResult, ColdStakeDelegationErrorCode>
CreateColdStakeDelegation(const std::string& stakeAddress, CAmount nValue,
                          const boost::optional<std::string>& ownerAddress, bool fForceExternalAddr,
                          bool fUseDelegated, bool fForceNotEnabled)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!Params().IsColdStakingEnabled() && !fForceNotEnabled)
        return Err(ColdStakingDisabled);

    // Get Staking Address
    CBitcoinAddress stakeAddr(stakeAddress);
    CKeyID          stakeKey;
    if (!stakeAddr.IsValid())
        return Err(InvalidStakerAddress);
    if (!stakeAddr.GetKeyID(stakeKey))
        return Err(StakerAddressPubKeyHashError);

    // Get Amount
    if (nValue < Params().MinColdStakingAmount())
        return Err(InvalidAmount);

    // Check amount
    const CAmount currBalance =
        pwalletMain->GetBalance() - (fUseDelegated ? 0 : pwalletMain->GetDelegatedBalance());
    if (nValue > currBalance)
        return Err(InsufficientBalance);

    if (pwalletMain->IsLocked() || fWalletUnlockStakingOnly)
        return Err(WalletLocked);

    // Get Owner Address
    CBitcoinAddress ownerAddr;
    CKeyID          ownerKey;
    if (ownerAddress) {
        // Address provided
        ownerAddr.SetString(*ownerAddress);
        if (!ownerAddr.IsValid())
            return Err(InvalidOwnerAddress);
        if (!ownerAddr.GetKeyID(ownerKey))
            return Err(OwnerAddressPubKeyHashError);
        // Check that the owner address belongs to this wallet, or fForceExternalAddr is true
        if (!fForceExternalAddr && !pwalletMain->HaveKey(ownerKey)) {
            return Err(OwnerAddressNotInWallet);
        }

    } else {
        // Get new owner address from keypool
        if (!pwalletMain->IsLocked())
            pwalletMain->TopUpKeyPool();

        // Generate a new key that is added to wallet
        CPubKey newKey;
        if (!pwalletMain->GetKeyFromPool(newKey))
            return Err(KeyPoolEmpty);
        CKeyID keyID = newKey.GetID();
        ownerAddr    = CBitcoinAddress(keyID);

        // set address book entry
        if (pwalletMain) {
            pwalletMain->SetAddressBookEntry(ownerAddr.Get(), "Delegator address");
        }

        if (!ownerAddr.GetKeyID(ownerKey))
            return Err(GeneratedOwnerAddressPubKeyHashError);
    }

    CoinStakeDelegationResult result;
    result.ownerAddress  = ownerAddr;
    result.stakerAddress = stakeAddr;
    // Get P2CS script for addresses
    result.scriptPubKey = GetScriptForStakeDelegation(stakeKey, ownerKey);
    return Ok(result);
}

std::string ColdStakeDelegationErrorStr(const ColdStakeDelegationErrorCode errorCode)
{
    switch (errorCode) {
    case ColdStakingDisabled:
        return "Cold Staking disabled. "
               "You may force the stake delegation to true. "
               "WARNING: If relayed before activation, this tx will be rejected resulting in a ban. ";
    case InvalidStakerAddress:
        return "Invalid staker address";
    case StakerAddressPubKeyHashError:
        return "Unable to get stake pubkey hash from stakingaddress";
    case InvalidAmount:
        return strprintf("Invalid amount. Min amount: %zd", Params().MinColdStakingAmount());
    case InsufficientBalance:
        return "Insufficient funds";
    case WalletLocked:
        return "Error: Please unlock the wallet first.";
    case InvalidOwnerAddress:
        return "Invalid neblio spending/owner address";
    case OwnerAddressPubKeyHashError:
        return "Unable to get spend pubkey hash from owneraddress";
    case OwnerAddressNotInWallet:
        return "The provided owneraddress is not present in this wallet.";
    case KeyPoolEmpty:
        return "Error: Keypool ran out, please call keypoolrefill first";
    case GeneratedOwnerAddressPubKeyHashError:
        return "Unable to get spend pubkey hash from owneraddress";
    }
}

#ifndef COLDSTAKEDELEGATION_H
#define COLDSTAKEDELEGATION_H

#include "base58.h"
#include "result.h"
#include <string>

struct CoinStakeDelegationResult
{
    CBitcoinAddress ownerAddress;
    CBitcoinAddress stakerAddress;
    CScript         scriptPubKey;

    json_spirit::Object AddressesToJsonObject() const;
};

enum ColdStakeDelegationErrorCode
{
    ColdStakingDisabled,
    InvalidStakerAddress,
    StakerAddressPubKeyHashError,
    InvalidAmount,
    InsufficientBalance,
    WalletLocked,
    InvalidOwnerAddress,
    OwnerAddressPubKeyHashError,
    OwnerAddressNotInWallet,
    KeyPoolEmpty,
    GeneratedOwnerAddressPubKeyHashError,
};

std::string ColdStakeDelegationErrorStr(const ColdStakeDelegationErrorCode errorCode);

Result<CoinStakeDelegationResult, ColdStakeDelegationErrorCode>
CreateColdStakeDelegation(const std::string& stakeAddress, CAmount nValue,
                          const boost::optional<std::string>& ownerAddress, bool fForceExternalAddr,
                          bool fUseDelegated, bool fForceNotEnabled);

#endif // COLDSTAKEDELEGATION_H

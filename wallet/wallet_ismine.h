// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2020-2020 The Neblio developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_ISMINE_H
#define BITCOIN_WALLET_ISMINE_H

#include "key.h"

class CKeyStore;
class CScript;

/** IsMine() return codes */
enum isminetype : uint_fast16_t
{
    ISMINE_NO = 0,
    //! Indicates that we dont know how to create a scriptSig that would solve this if we were given the
    //! appropriate private keys
    ISMINE_WATCH_ONLY = 1,
    //! Indicates that we know how to create a scriptSig that would solve this if we were given the
    //! appropriate private keys
    ISMINE_MULTISIG  = 2,
    ISMINE_SPENDABLE = 4,
    //! Indicates that we have the staking key of a P2CS
    ISMINE_COLD = 8,
    //! Indicates that we have the spending key of a P2CS
    ISMINE_SPENDABLE_DELEGATED = 16,
    //! Indicates that this wallet belongs to the Ledger HW wallet
    ISMINE_LEDGER              = 32,
    ISMINE_SPENDABLE_ALL       = ISMINE_SPENDABLE_DELEGATED | ISMINE_SPENDABLE | ISMINE_LEDGER,
    ISMINE_SPENDABLE_AVAILABLE = ISMINE_SPENDABLE | ISMINE_LEDGER,
    ISMINE_SPENDABLE_STAKEABLE = ISMINE_SPENDABLE_DELEGATED | ISMINE_COLD,
    ISMINE_ALL =
        ISMINE_WATCH_ONLY | ISMINE_SPENDABLE | ISMINE_COLD | ISMINE_SPENDABLE_DELEGATED | ISMINE_LEDGER
};
/** used for bitflags of isminetype */
using isminefilter = uint8_t;

bool IsMineCheck(isminetype var, isminetype toCheckFor);

#endif // BITCOIN_WALLET_ISMINE_H

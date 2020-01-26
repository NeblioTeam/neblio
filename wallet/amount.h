// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_AMOUNT_H
#define BITCOIN_AMOUNT_H

#include <limits>
#include <stdint.h>
#include <string>

/** Amount in satoshis (Can be negative) */
using CAmount = int64_t;

static const int64_t COIN = 100000000;
static const int64_t CENT = 1000000;

/** Fees smaller than this (in satoshi) are considered zero fee (for transaction creation) */
static const int64_t MIN_TX_FEE = 10000;

// static const CAmount PERKB_TX_FEE = CENT;
// static const CAmount MIN_TXOUT_AMOUNT = CENT;
// static const CAmount MAX_MINT_PROOF_OF_WORK = 9999 * COIN;
// static const std::string CURRENCY_UNIT = "NEBL";

// Total coin that will be released (~infinite);
static const CAmount MAX_MONEY = std::numeric_limits<int64_t>::max();

inline bool MoneyRange(int64_t nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif //  BITCOIN_AMOUNT_H

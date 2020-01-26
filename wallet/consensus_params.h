// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "NetworkForks.h"
#include <bignum.h>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <uint256.h>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV,    // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_SEGWIT, // Deployment of BIP141, BIP143, and BIP147.
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Parameters that influence chain consensus.
 */
struct Params
{
    uint256 hashGenesisBlock;

    /** peercoin stuff */
    CBigNum bnProofOfWorkLimit;
    CBigNum bnProofOfStakeLimit;
    int64_t nStakeTargetSpacingV2;
    int64_t nStakeTargetSpacingV1;
    int64_t nTargetTimespan;
    int64_t nStakeMinAgeV2;
    int64_t nStakeMinAgeV1;
    int64_t nStakeMaxAge;
    int64_t nModifierInterval;
    // Coinbase transaction outputs can only be spent after this number of new blocks (network rule)
    int nCoinbaseMaturityV3;
    int nCoinbaseMaturityV2;
    int nCoinbaseMaturityV1;

    /** neblio specific */
    int firstValidNTP1Height; // first block height with a valid NTP1 transaction

    std::unique_ptr<NetworkForks> forks;

    /** Max OP_RETURN Size */
    unsigned int nMaxOpReturnSizeV1;
    unsigned int nMaxOpReturnSizeV2;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H

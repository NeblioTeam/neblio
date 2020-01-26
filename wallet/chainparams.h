// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include "ThreadSafeMap.h"
#include "chainparamsbase.h"
#include "consensus_params.h"
#include "protocol.h"

#include <memory>
#include <unordered_map>
#include <vector>

class CBlock;

struct SeedSpec6
{
    uint8_t  addr[16];
    uint16_t port;
};

using MapCheckpoints              = ThreadSafeMap<int, uint256>;
using MapStakeModifierCheckpoints = std::map<int, unsigned int>;
using MapExcludedTxs              = std::unordered_map<uint256, int>;
using MapBlacklistedTokens        = std::unordered_map<std::string, int>;

struct ChainTxData
{
    int64_t nTime;
    int64_t nTxCount;
    double  dTxRate;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Bitcoin system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    enum Base58Type
    {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,

        MAX_BASE58_TYPES
    };

    const Consensus::Params&                 GetConsensus() const { return consensus; }
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    const std::vector<unsigned char>&        AlertKey() const { return vAlertPubKey; }
    int                                      GetDefaultPort() const { return nDefaultPort; }

    const CBlock&  GenesisBlock() const;
    const uint256& GenesisBlockHash() const;
    /** Make miner stop after a block is found. In RPC, don't return until nGenProcLimit blocks are
     * generated */
    bool MineBlocksOnDemand() const { return fMineBlocksOnDemand; }
    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const { return strNetworkID; }
    /** Return the list of hostnames to look up for DNS seeds */
    const std::vector<std::string>& DNSSeeds() const { return vDNSSeeds; }
    const std::vector<std::string>& AdditionalNodes() const { return vAdditionalNodes; }
    uint8_t                         Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    const MapCheckpoints&           Checkpoints() const { return checkpointData; }

    const NetworkForks& GetNetForks() const { return *consensus.forks; }

    int64_t     StakeMinAge() const;
    int64_t     StakeMaxAge() const;
    int64_t     StakeModifierInterval() const;
    NetworkType NetType() const;
    bool        PassedFirstValidNTP1Tx() const;
    int64_t     TargetTimeSpan() const;

    unsigned int OpReturnMaxSize() const;

    unsigned int TargetSpacing() const;

    int CoinbaseMaturity() const;

    const CBigNum& PoWLimit() const;
    const CBigNum& PoSLimit() const;

    const MapStakeModifierCheckpoints& StakeModifierCheckpoints() const;

    bool IsNTP1TokenBlacklisted(const std::string& tokenId, int& maxHeight) const
    {
        auto it = ntp1BlacklistedTokenIds.find(tokenId);
        if (it != ntp1BlacklistedTokenIds.cend()) {
            maxHeight = it->second;
            return true;
        }
        return false;
    }

    bool IsNTP1TokenBlacklisted(const std::string& tokenId) const
    {
        return ntp1BlacklistedTokenIds.find(tokenId) != ntp1BlacklistedTokenIds.cend();
    }

    bool IsNTP1TxExcluded(const uint256& txHash) const
    {
        return excludedTxs.find(txHash) != excludedTxs.cend();
    }

protected:
    CChainParams() = default;

    Consensus::Params                 consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    //! Raw pub key bytes for the broadcast alert signing key.
    std::vector<unsigned char>            vAlertPubKey;
    int                                   nDefaultPort;
    std::vector<std::string>              vDNSSeeds;
    std::vector<std::string>              vAdditionalNodes;
    std::array<uint8_t, MAX_BASE58_TYPES> base58Prefixes;
    std::string                           strNetworkID;
    std::unique_ptr<CBlock>               genesis;
    bool                                  fMineBlocksOnDemand;
    MapCheckpoints                        checkpointData;
    NetworkType                           networkType;

    std::unordered_map<std::string, int> ntp1BlacklistedTokenIds;
    std::unordered_map<uint256, int>     excludedTxs;

    //! Hard checkpoints of stake modifiers to ensure they are deterministic
    MapStakeModifierCheckpoints mapStakeModifierCheckpoints;
};

/**
 * Creates and returns a std::unique_ptr<CChainParams> of the chosen chain.
 * @returns a CChainParams* of the chosen chain.
 * @throws a std::runtime_error if the chain is not supported.
 */
std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain);

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams& Params();

/**
 * Sets the params returned by Params() to those for the given BIP70 chain name.
 * @throws std::runtime_error when the chain is not supported.
 */
void SelectParams(NetworkType networkType);

#endif // BITCOIN_CHAINPARAMS_H

// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparamsbase.h>

#include <util.h>

#include <assert.h>
#include <memory>

const std::string CBaseChainParams::MAIN    = "main";
const std::string CBaseChainParams::TESTNET = "test";
const std::string CBaseChainParams::REGTEST = "regtest";

void AppendParamsHelpMessages(std::string& strUsage, bool debugHelp)
{
    strUsage += std::string("Chain selection options:");
    strUsage += std::string("-testnet") + "Use the test chain";
    if (debugHelp) {
        strUsage += std::string("-regtest") +
                    "Enter regression test mode, which uses a special chain in which "
                    "blocks can be solved instantly. "
                    "This is intended for regression testing tools and app development.\n\n";
    }
}

/**
 * Main network
 */
class CBaseMainParams : public CBaseChainParams
{
public:
    CBaseMainParams()
    {
        nRPCPort   = 6326;
        strDataDir = "";
    }
};

/**
 * Testnet (v3)
 */
class CBaseTestNetParams : public CBaseChainParams
{
public:
    CBaseTestNetParams()
    {
        nRPCPort   = 16326;
        strDataDir = "testnet";
    }
};

/**
 * Regression test
 */
class CBaseRegTestParams : public CBaseChainParams
{
public:
    CBaseRegTestParams()
    {
        nRPCPort   = 26326;
        strDataDir = "regtest";
    }
};

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;

const CBaseChainParams& BaseParams()
{
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}

std::string GetChainName(NetworkType networkType)
{
    switch (networkType) {
    case NetworkType::Mainnet:
        return CBaseChainParams::MAIN;
    case NetworkType::Testnet:
        return CBaseChainParams::TESTNET;
    case NetworkType::Regtest:
        return CBaseChainParams::REGTEST;
    }
    throw std::runtime_error(
        fmt::format("{}: Unknown chain with id {}.", FUNCTIONSIG, static_cast<int32_t>(networkType)));
}

std::unique_ptr<CBaseChainParams> CreateBaseChainParams(NetworkType networkType)
{
    switch (networkType) {
    case NetworkType::Mainnet:
        return std::unique_ptr<CBaseChainParams>(new CBaseMainParams());
    case NetworkType::Testnet:
        return std::unique_ptr<CBaseChainParams>(new CBaseTestNetParams());
    case NetworkType::Regtest:
        return std::unique_ptr<CBaseChainParams>(new CBaseRegTestParams());
    }
    throw std::runtime_error(
        fmt::format("{}: Unknown chain {}.", FUNCTIONSIG, GetChainName(networkType)));
}

void SelectBaseParams(NetworkType networkType)
{
    globalChainBaseParams = CreateBaseChainParams(networkType);
}

NetworkType ChainTypeFromCommandLine()
{
    bool fRegTest = GetBoolArg("-regtest", false);
    bool fTestNet = GetBoolArg("-testnet", false);

    if (fTestNet && fRegTest)
        throw std::runtime_error("Invalid combination of -regtest and -testnet.");
    if (fRegTest)
        return NetworkType::Regtest;
    if (fTestNet)
        return NetworkType::Testnet;
    return NetworkType::Mainnet;
}

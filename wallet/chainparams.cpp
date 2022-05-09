// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include "block.h"
#include "txdb.h"
#include "util.h"
#include <amount.h>
#include <assert.h>
#include <boost/optional.hpp>
#include <cinttypes>
#include <memory>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript,
                                 uint32_t nTimeTx, uint32_t nTimeBlock, uint32_t nNonce, uint32_t nBits,
                                 int32_t nVersion, const CAmount& genesisReward)
{

    CTransaction txNew;
    txNew.nTime = 1500674579;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 0 << CBigNum(42)
                                       << std::vector<unsigned char>((const unsigned char*)pszTimestamp,
                                                                     (const unsigned char*)pszTimestamp +
                                                                         strlen(pszTimestamp));
    txNew.vout[0].nValue       = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;
    txNew.nTime                = nTimeTx;

    CBlock genesis;
    genesis.nTime    = nTimeBlock;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock  = 0;
    genesis.hashMerkleRoot = genesis.GetMerkleRoot();

    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 */
static CBlock CreateGenesisBlock(uint32_t nTimeTx, uint32_t nTimeBlock, uint32_t nNonce, uint32_t nBits,
                                 int32_t nVersion, const CAmount& genesisReward)
{
    const char*   pszTimestamp        = "21jul2017 - Neblio First Net Launches";
    const CScript genesisOutputScript = CScript();
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTimeTx, nTimeBlock, nNonce, nBits,
                              nVersion, genesisReward);
}

/**
 * Main network
 */
class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkType  = NetworkType::Mainnet;
        strNetworkID = "main";

        // Set PoW difficulty to easiest
        consensus.bnProofOfWorkLimit = CBigNum(~uint256(0) >> 1);
        // Set PoS difficulty to standard
        consensus.bnProofOfStakeLimit = CBigNum(~uint256(0) >> 20);

        consensus.nTargetTimespan       = 2 * 60 * 60;              // two hours
        consensus.nStakeTargetSpacingV1 = 120;                      // 120 seconds block spacing
        consensus.nStakeTargetSpacingV2 = 30;                       // 30 seconds block spacing
        consensus.nStakeMinAgeV1        = 24 * 60 * 60;             // minimum age for coin age
        consensus.nStakeMinAgeV2        = consensus.nStakeMinAgeV1; // minimum age for coin age
        consensus.nStakeMaxAge          = 7 * 24 * 60 * 60;         // Maximum stake age 7 days
        consensus.nModifierInterval     = 10 * 60; // time to elapse before new modifier is computed

        consensus.firstValidNTP1Height = 157528;

        consensus.nMaxOpReturnSizeV1 = 80;
        consensus.nMaxOpReturnSizeV2 = 4096;

        consensus.forks.emplace(NetworkForks(boost::container::flat_map<NetworkFork, int>{
            {NetworkFork::NETFORK__1_FIRST_ONE, 0},
            // number of stake confirmations changed to 10
            {NetworkFork::NETFORK__2_CONFS_CHANGE, 248000},
            // Tachyon upgrade. Approx Jan 12th 2019
            {NetworkFork::NETFORK__3_TACHYON, 387028},
            // RetargetV3 upgrade. Approx June 15 2019
            {NetworkFork::NETFORK__4_RETARGET_CORRECTION, 1003125},
            // Enable cold-staking - unset placeholder
            {NetworkFork::NETFORK__5_COLD_STAKING, 2730450}}));

        consensus.nCoinbaseMaturityV1 = 30;
        consensus.nCoinbaseMaturityV2 = 10;
        consensus.nCoinbaseMaturityV3 = 120;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x32;
        pchMessageStart[1] = 0x5e;
        pchMessageStart[2] = 0x6f;
        pchMessageStart[3] = 0x86;

        vAlertPubKey = ParseHex("046df586b596db22eda44a90b08fbaab100dff97612d3eb32cee236ea385cb09e5e05fc"
                                "0b0d2ec278ac0ac97daba8201508be27b4de780be06d447217037c6d082");
        nDefaultPort = 6325;

        genesis = std::unique_ptr<CBlock>(
            new CBlock(CreateGenesisBlock(1500674579, 1500674579, 8485, PoWLimit().GetCompact(), 1, 0)));
        consensus.hashGenesisBlock = genesis->GetHash();

        assert(consensus.hashGenesisBlock ==
               uint256("0x7286972be4dbc1463d256049b7471c252e6557e222cab9be73181d359cd28bcc"));
        assert(genesis->hashMerkleRoot ==
               uint256("0x203fd13214321a12b01c0d8b32c780977cf52e56ae35b7383cd389c73291aee7"));

        vAdditionalNodes.clear();
        vAdditionalNodes.emplace_back("nebliodseed1.nebl.io");
        vAdditionalNodes.emplace_back("nebliodseed2.nebl.io");

        vDNSSeeds.clear();
        vDNSSeeds.emplace_back("seed.nebl.io");
        vDNSSeeds.emplace_back("seed2.nebl.io");

        base58Prefixes[PUBKEY_ADDRESS] = 53;  // Neblio: addresses begin with 'N'
        base58Prefixes[SCRIPT_ADDRESS] = 112; // Neblio: addresses begin with 'n'

        fMiningRequiresPeers = true;
        fMineBlocksOnDemand  = false;

        // staking parameters
        nStakeSplitAge          = 1 * 24 * 60 * 60;
        nStakeCombineThreshold  = 1000 * COIN;
        nMaxInputsInStake       = 100;
        nMaxStakeSearchInterval = 60;
        nMinColdStakingAmount   = 10 * COIN;

        nLastPoWBlock = 1000;

        /**
        // What makes a good checkpoint block?
        // + Is surrounded by blocks with reasonable timestamps
        //   (no blocks before with a timestamp after, none after with
        //    timestamp before)
        // + Contains no strange transactions
        */

        // clang-format off
        checkpointData = MapCheckpoints({
            {0,       genesis->GetHash()},
            {500,     uint256{"0x00000342c5dc5f7fd4a8ef041d4df4e569bd40756405a8c336c5f42c77e097a2"}},
            {1000,    uint256{"0x00000c60e3a8d27dedb15fc33d91caec5cf714fae60f24ea22a649cded8e0cca"}},
            {5000,    uint256{"0x074873095a26296d4f0033f697f46bddb7c1359ffcb3461f620e346bc516a1d2"}},
            {25000,   uint256{"0x9c28e51c9c21092909fe0a6ad98ae335f253fa9c8076bb3cca154b6ba5ee03ab"}},
            {100000,  uint256{"0xbb13aedc5846fe5d384601ef4648492262718fc7dfe35b886ef297ea74cab8cc"}},
            {150000,  uint256{"0x9a755758cc9a8d40fc36e6cc312077c8dd5b32b2c771241286099fd54fd22db0"}},
            {200000,  uint256{"0xacea764bbb689e940040b229a89213e17b50b98db0514e1428acedede9c1a4c0"}},
            {250000,  uint256{"0x297eda3c18c160bdb2b1465164b11ba2ee7908b209a26d3b76eac3876aa55072"}},
            {260000,  uint256{"0x4d407875afd318897266c14153d856774868949c65176de9214778d5626707a0"}},
            {270000,  uint256{"0x7f8ead004a853b411de63a3f30ee5a0e4c144a11dbbc00c96942eb58ff3b9a48"}},
            {280000,  uint256{"0x954544adaa689ad91627822b9da976ad6f272ced95a272b41b108aabff30a3e5"}},
            {285000,  uint256{"0x7c37fbdb5129db54860e57fd565f0a17b40fb8b9d070bda7368d196f63034ae5"}},
            {287500,  uint256{"0x3da2de78a53afaf9dafc8cec20a7ace84c52cff994307aef4072d3d0392fe041"}},
            {290000,  uint256{"0x5685d1cc15100fa0c7423b7427b9f0f22653ccd137854f3ecc6230b0d1af9ebc"}},
            {295000,  uint256{"0x581aef5415de9ce8b2817bf803cf29150bd589a242c4cb97a6fd931d6f165190"}},
            {300000,  uint256{"0xb2d6ef8b3ec931c48c2d42fa574a382a534014388b17eb8e0eca1a0db379e369"}},
            {305000,  uint256{"0x9332baa2c500cb938024d2ec35b265bfa2928b63ae5d2d9d81ffd8cbfd75ef1d"}},
            {310000,  uint256{"0x53c993efaf747fadd0ecae8b3a15292549e77223853a8dc90c18aa4664f85b6e"}},
            {315000,  uint256{"0xb46b2d2681294d04a366f34eb2b9183621961432c841a155fe723deabcbf9e38"}},
            {320000,  uint256{"0x82ecc41d44fefc6667119b0142ba956670bda4e15c035eefe66bfaa4362d2823"}},
            {350000,  uint256{"0x7787a1240f1bff02cd3e37cfc8f4635725e26c6db7ff44e8fbee7bf31dc6d929"}},
            {360000,  uint256{"0xb4b001753a4d7ec18012a5ff1cbf3f614130adbf6c3f2515d36dfc3300655c2a"}},
            {387026,  uint256{"0x37ec421ce623892935d939930d61c066499b8c7eb55606be67219a576d925b67"}},
            {387027,  uint256{"0x1a7a41f757451fa32acb0aa31e262398d660e90994b8e17f164dd201718c8f5d"}},
            {387028,  uint256{"0xac7d44244ff394255f4c1f99664b26cd015d3d10bddbb8a86727ff848faa6acf"}},
            {387029,  uint256{"0x7e4655517659f78cd2e870305e42353ea5bcf9ac1aaa79c1254f9222993c12d5"}},
            {387030,  uint256{"0xae375a05ca92fe78e2768352eebb358b12fc0c2c65263d7ac29e4fe723636f81"}},
            {390000,  uint256{"0xcd035c9899d22c414f79a345c1b96fd9342d1beb5f80f1dbad6a6244b5d3d5b8"}},
            {400000,  uint256{"0x7ae908b0c5351fae59fcff7ab4fe0e23f4e7630ed895822676f3ee551262d82d"}},
            {500000,  uint256{"0x92b5c16c99769dcad4c2d4548426037b35894ef57ff1bf2516575440e1f87d4f"}},
            {600000,  uint256{"0x69c4acf177368eeb40155e7b03d07b7a6579620320d5de2554db99d0f4908b97"}},
            {685000,  uint256{"0xa276d5697372e71f597dca34c40391747186ce3fda96ee1875376b4b0f625881"}},
            {700000,  uint256{"0x8b5806c169fb7d3345e9f02ee0a38538cc4ab5884177002c1e9528058c5eab40"}},
            {800000,  uint256{"0x71e29af1056d1e8e217382f433d017406db7f0e03eb1995429a9edb741120643"}},
            {900000,  uint256{"0x8757e0670d5db26a9b540c616ae1c208bda9f4c3b3270754a36c867aa238206b"}},
            {1000000, uint256{"0x0ef9d1ce85a1e8209f735f1574bbe0ed0aaca34f0c6052a65443aada25be94a8"}},
            {1003123, uint256{"0xf2ec975040b2a5b1a1bf0c722b685596755e6021680661589aa7f8585d283700"}},
            {1003124, uint256{"0xd9d451b69134e2d7682014fb5366bb662b3e753b23722cb34326c09aa1c22762"}},
            {1003125, uint256{"0x0faaf5119ab9eb3a22e0984d6cba6cebc8d7bae25342401c782ab4fa413c326e"}},
            {1003126, uint256{"0x8f21fc3e383c5ec61dec1f171a0b49eea25dccbb28755214a0d45e73dccb7c56"}},
            {1003127, uint256{"0x5aaf45ff165d066f84d55399fda3c4458234f94cf32b0cfdcc7f9bbcc814585d"}},
            {1100000, uint256{"0xb726814d624b9a1b77e4edfb43ec4c8c47d5cfe4a2c7644812074fb5ac01f252"}},
            {1120000, uint256{"0x8c33837e3657a73aa3a89fa9f31cc565b6d075ddcb246de1cf5d9db90574e344"}},
            {1130000, uint256{"0xd953fc97fedf8e580211f1156b82b50f6da37c59e26c7d57dcfed9fbfd489ef8"}},
            {1200000, uint256{"0x901c6205092ac4fff321de8241badaf54da4c1f3f7c421b06a442f2a887d88ce"}},
            {1300000, uint256{"0xc0d0115689b9687cb03d7520ed45e5500e792a83cd3842034b5f9e26fda6d3ce"}},
            {1400000, uint256{"0x4697721a360aa7909e7badf528b3223add193943f1444524284b9a31501cd88a"}},
            {1500000, uint256{"0xdc3445dfd8e1f57f42011e6b1d63352a69347c830dc1fab36c699dc6a211b48f"}},
            {1600000, uint256{"0xb3970d20ca506d31d191f6422150c5e65696ef55bbc51df844171681ed79693f"}},
            {1700000, uint256{"0x67490f7265f5fc8d29a36ebb066a7f4dee724bfa9b7691b8e420544385556c68"}},
            {1800000, uint256{"0x820f5b448a49b8273d60377f047eb45b1764cd0a00bf8c219f555b49b9751c66"}},
            {1900000, uint256{"0x70ff2582c9ef327a71f5215d58d3ad2b6473b3649b2c018cc1ff524b672d69a2"}},
            {2000000, uint256{"0xc2a644527223b80000f11b9a821e398ab99483d71c3cb1304e9c267b64c7b85a"}},
            {2100000, uint256{"0xd5e7791acc99afc500679205df06bfb62b298040645f247f41eaf2acb42868cb"}},
            {2200000, uint256{"0x8791a85a7ec96571070a589978a99cc2cc0e06c5345056698604e7e793759d08"}},
            {2300000, uint256{"0x575ca59268e10b92cfedca6059a388043882f95442b7290012bf8a333ce889c4"}},
            {2400000, uint256{"0xdd8ed2992b0df4422d1fc950350c82f84d9a0862f93582f9404d5c3bb4b3a625"}},
            {2500000, uint256{"0x07ad693d84ef66eaa81f96db7ad901e871ca02a76b1fabb72c1e300580dd2c71"}},
            {2600000, uint256{"0x8d1855390705044b515907cc2096cd2bb4979cb18d6bf1edd26983da60387502"}},
            {2687000, uint256{"0x6d2097fce84bd83b066f2a63512b8a44225314cd5f2561eac471071eae291d9a"}}});
        // clang-format on

        mapStakeModifierCheckpoints = MapStakeModifierCheckpoints{{0, 0xfd11f4e7},   // genesis
                                                                  {500, 0x3b54b16d}, // premine
                                                                  {1000, 0x7b238954}};

        excludedTxs = MapExcludedTxs{};

        // token id vs max block to take transactions from these tokens due to bugs
        ntp1BlacklistedTokenIds = MapBlacklistedTokens{
            {"La77KcJTUj991FnvxNKhrCD1ER8S81T3LgECS6", 300000}, // old QRT mainnet
            {"La4kVcoUAddLWkmQU9tBxrNdFjmSaHQruNJW2K", 300000}, // old TNIBB testnet
            {"La36YNY2G6qgBPj7VSiQDjGCy8aC2GUUsGqtbQ", 300000}, // old TNIBB testnet
            {"La9wLfpkfZTQvRqyiWjaEpgQStUbCSVMWZW2by", 300000}, // TEST3 on testnet
            {"La347xkKhi5VUCNDCqxXU4F1RUu8wPvC3pnQk6", 300000}, // BOT on testnet
            {"La531vUwiu9NnvtJcwPEjV84HrdKCupFCCb6D7", 300000}, // BAUTO on testnet
            {"La5JGnJcSsLCvYWxqqVSyj3VUqsrAcLBjZjbw5", 300000}, // XYZ from Sam on testnet
            {"La86PtvXGftbwdoZ9rVMKsLQU5nPHganJDsCRq", 300000}  // ON
            //    ,{"LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp", 300000}  // NIBBL
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams
{
public:
    CTestNetParams()
    {
        networkType  = NetworkType::Testnet;
        strNetworkID = "test";

        // Set PoW difficulty to easiest
        consensus.bnProofOfWorkLimit = CBigNum(~uint256(0) >> 1);
        // Set PoS difficulty to standard
        consensus.bnProofOfStakeLimit = CBigNum(~uint256(0) >> 20);

        consensus.nTargetTimespan       = 2 * 60 * 60;      // two hours
        consensus.nStakeTargetSpacingV1 = 120;              // 120 seconds block spacing
        consensus.nStakeTargetSpacingV2 = 30;               // 30 seconds block spacing
        consensus.nStakeMinAgeV1        = 60;               // minimum age for coin age
        consensus.nStakeMinAgeV2        = 24 * 60 * 60;     // minimum age for coin age
        consensus.nStakeMaxAge          = 7 * 24 * 60 * 60; // Maximum stake age 7 days
        consensus.nModifierInterval     = 10 * 60; // time to elapse before new modifier is computed

        consensus.nCoinbaseMaturityV1 = 10;
        consensus.nCoinbaseMaturityV2 = 10;
        consensus.nCoinbaseMaturityV3 = 120;

        consensus.firstValidNTP1Height = 10313;

        consensus.nMaxOpReturnSizeV1 = 80;
        consensus.nMaxOpReturnSizeV2 = 4096;

        consensus.forks.emplace(NetworkForks(boost::container::flat_map<NetworkFork, int>{
            {NetworkFork::NETFORK__1_FIRST_ONE, 0},
            {NetworkFork::NETFORK__2_CONFS_CHANGE, 0},
            // Roughly Aug 1 2018 Noon EDT
            {NetworkFork::NETFORK__3_TACHYON, 110100},
            {NetworkFork::NETFORK__4_RETARGET_CORRECTION, 1163000},
            // Enable cold-staking
            {NetworkFork::NETFORK__5_COLD_STAKING, 2386991}}));

        pchMessageStart[0] = 0x1b;
        pchMessageStart[1] = 0xba;
        pchMessageStart[2] = 0x63;
        pchMessageStart[3] = 0xc5;
        vAlertPubKey = ParseHex("04da59da7f2e1c9d0f575187065930361ad09751f7a8ccae25f0ab9ebbd479c0cda65a8"
                                "ae0415a4a64bac46f79a4cd67bdb0925871855db3227969005361beaf21");
        nDefaultPort = 16325;

        genesis = std::unique_ptr<CBlock>(
            new CBlock(CreateGenesisBlock(1500674579, 1500674579, 8485, PoWLimit().GetCompact(), 1, 0)));
        consensus.hashGenesisBlock = genesis->GetHash();

        assert(consensus.hashGenesisBlock ==
               uint256("0x7286972be4dbc1463d256049b7471c252e6557e222cab9be73181d359cd28bcc"));
        assert(genesis->hashMerkleRoot ==
               uint256("0x203fd13214321a12b01c0d8b32c780977cf52e56ae35b7383cd389c73291aee7"));

        vAdditionalNodes.clear();
        vAdditionalNodes.emplace_back("nebliodseed1.nebl.io");
        vAdditionalNodes.emplace_back("nebliodseed2.nebl.io");

        vDNSSeeds.clear();
        vDNSSeeds.emplace_back("testnet-seed.nebl.io");

        base58Prefixes[PUBKEY_ADDRESS] = 65;  // Neblio: addresses begin with 'T'
        base58Prefixes[SCRIPT_ADDRESS] = 127; // Neblio: addresses begin with 't'

        fMiningRequiresPeers = true;
        fMineBlocksOnDemand  = false;

        // staking parameters
        nStakeSplitAge          = 1 * 24 * 60 * 60;
        nStakeCombineThreshold  = 1000 * COIN;
        nMaxInputsInStake       = 100;
        nMaxStakeSearchInterval = 60;
        nMinColdStakingAmount   = 10 * COIN;

        nLastPoWBlock = 1000;

        /**
        // What makes a good checkpoint block?
        // + Is surrounded by blocks with reasonable timestamps
        //   (no blocks before with a timestamp after, none after with
        //    timestamp before)
        // + Contains no strange transactions
        */

        // clang-format off
        checkpointData = MapCheckpoints({
            {0,       genesis->GetHash()},
            {1,       uint256{"0x0e2eecad99db0eab96abbd7e2de769d92483a090eefcefc014b802d31131a0ce"}},
            {500,     uint256{"0x0000006939777fded9640797f3008d9fca5d6e177e440655ba10f8a900cabe61"}},
            {1000,    uint256{"0x000004715d8818cea9c2e5e9a727eb2f950964eb0d1060e1d5effd44c2ca45df"}},
            {100000,  uint256{"0x1fdbb9642e997fa13df3b0c11c95e959a2606ef9bc6c431e942cf3fc74ed344d"}},
            {200000,  uint256{"0xf4072b1e5b7ede5b33c82045b13f225b41ff3d8262e03ea5ed9521290e2d5e42"}},
            {300000,  uint256{"0x448d74d70dea376576217ef72518f18f289ab4680f6714cdac8a3903f7a2cacf"}},
            {400000,  uint256{"0x09c3bd420fa43ab4e591b0629ed8fe0e86fc264939483d6b7cb0a59f05020953"}},
            {500000,  uint256{"0xae87c4f158e07623b88aa089f2de3e3437352873293febcfa1585b07e823d955"}},
            {600000,  uint256{"0x3c7dbe265d43da7834c3f291e031dda89ef6c74f2950f0af15acf33768831f91"}},
            {700000,  uint256{"0xa5bcfb2d5d52e8c0bdce1ae11019a7819d4d626e6836f1980fe6b5ce13c10039"}},
            {800000,  uint256{"0x13a2c603fbdb4ced718d6f7bba60b335651ddb832fbe8e11962e454c6625e20f"}},
            {900000,  uint256{"0xe5c4d6f1fbd90b6a2af9a02f1e947422a4c5a8756c34d7f0e45f57b341e47156"}},
            {1000000, uint256{"0x806506a6eafe00e213c666a8c8fd14dac0c6d6a52e0f05a4d175633361e5e377"}},
            {1100000, uint256{"0x397b5e6e0e95d74d7c01064feae627d11a2a99d08ebf91200dbb9d94b1d4ee26"}},
            {1200000, uint256{"0x54e813b81516c1a6169ff81abaec2715e13b2ec0796db4fcc510be1e0805d21e"}},
            {1300000, uint256{"0x75da223a32b31b3bbb1f32ab33ad5079b70698902ebed5594bebc02ffecb74a8"}},
            {1400000, uint256{"0x064c16b9c408e40f020ca455255e58da98b019eb424554259407d7461c5258e2"}},
            {1500000, uint256{"0x1fc65c5e904c0dda39a26826df0feaa1d35f5d49657acee2d1674271f38b2100"}},
            {1600000, uint256{"0x8510acea950aa7e2da8d287bacc66cca6056bf89f5f0d70109fd92adaf1023d9"}},
            {1700000, uint256{"0x65738a87a454cfe97b8200149cd4be7199d1ceff30b18778bd79d222203962ce"}},
            {1801000, uint256{"0x406fc58723c11eae128c85174e81b5b6b333eaf683ff4f6ca34bbd8cee3b24f5"}},
            {2521000, uint256{"0xd3dc0dd25f4850fa8a607620620959e1970e7bcfe9b36ffd8df3bda1004e5cab"}},
            {2581300, uint256{"0xe90b2a55da410f834e047a1f2c1d1901f6beeba2a366a6ce05b01112e9973432"}}});
        // clang-format on

        // Hard checkpoints of stake modifiers to ensure they are deterministic (testNet)
        mapStakeModifierCheckpoints = MapStakeModifierCheckpoints{{0, 0xfd11f4e7}, // genesis
                                                                  {100, 0x7bb33af1}};

        // list of transactions to be excluded because they're invalid
        excludedTxs = MapExcludedTxs(std::unordered_map<uint256, int>{
            {uint256("826e7b74b24e458e39d779b1033567d325b8d93b507282f983e3c4b3f950fca1"), 0},
            {uint256("c378447562be04c6803fdb9f829c9ba0dda462b269e15bcfc7fac3b3561d2eef"), 0},
            {uint256("a57a3e4746a79dd0d0e32e6a831d4207648ff000c82a4c5e8d9f3b6b0959f8b8"), 0},
            {uint256("7e71508abef696d6c0427cc85073e0d56da9380f3d333354c7dd9370acd422bc"), 0},
            {uint256("adb421a497e25375a88848b17b5c632a8d60db3d02dcc61dbecd397e6c1fb1ca"), 0},
            {uint256("adedc16e0318668e55f08f2a1ea57be8c5a86cfce3c1900346b0337a8f75a390"), 0},
            {uint256("bb8f1a29237e64285b9bd1f2bf1500c0de6205e8eb5e004c3b1ab6671e9c4cb2"), 0},
            {uint256("cc8f8a763677b8015bf79a19c9bcf87837b734d1cb203b30726af27b75f41a48"), 0},
            {uint256("666d81ad74e470ef1c9e74022a8be886e4951a0bec0d27f9b078519a30af71b2"), 0},
            {uint256("27bea35b4e2ac8987441aa7c5ff3d305047664ef7244b822cad54e549b84f50b"), 0},
            {uint256("59cb6e2cc9649d9a9b806f820a91927dcb0e43d1e1e92b0b9d976e921bba1334"), 0},
            {uint256("054cead1a3b498ec845462a1920508698e4f0ab2a71e1f4f8d827d007a43a2f4"), 0},
            {uint256("7d211b98e4796e9375233d935eb8d1262d6fb9d79645b576f15ad1b85427facf"), 0},
            {uint256("ab336eecf51cdaecd3f7444d5da7eca2286462d44e7f3439458ecbe3d7514971"), 0},
            {uint256("2feb60b06d175603b159d4d2c3e436d514f775968a83fc610993e6abc2e96dcc"), 0},
            {uint256("58640bf32069a8e54b6a2d11a4f3c33084c849484ab799c82680b94989b1dda8"), 0},
            {uint256("b4c4b0532f89d582292ce2c199f75f9629d0751f08a72f9ae30cf3f410f6cbf0"), 0},
            {uint256("43cf479c59b2a14876e390c63ea17846251b43cee1ad9a0cd3a9f5c824ac1aba"), 0},
            {uint256("0295e6d8e9e57acc8e4b80e095e2f10bebcc89e4bc1720c27e30c27a37a7464f"), 0},
            {uint256("d68305e8a8efc4535185c3e6923006fcd81c007a96022f08d556ab7832fd6f83"), 0},
            {uint256("1cfb6af9299854257b0697264ce2f574ae37b543dc8c2b2c60182bcf570ba60c"), 0},
            {uint256("834c4809152fb9a964f3b2ba76d281868044114661285913cead8cabb69f91e9"), 0},
            {uint256("95c6f2b978160ab0d51545a13a7ee7b931713a52bd1c9f12807f4cd77ff7536b"), 0}});

        // token id vs max block to take transactions from these tokens due to bugs
        ntp1BlacklistedTokenIds = MapBlacklistedTokens{
            {"La77KcJTUj991FnvxNKhrCD1ER8S81T3LgECS6", 300000}, // old QRT mainnet
            {"La4kVcoUAddLWkmQU9tBxrNdFjmSaHQruNJW2K", 300000}, // old TNIBB testnet
            {"La36YNY2G6qgBPj7VSiQDjGCy8aC2GUUsGqtbQ", 300000}, // old TNIBB testnet
            {"La9wLfpkfZTQvRqyiWjaEpgQStUbCSVMWZW2by", 300000}, // TEST3 on testnet
            {"La347xkKhi5VUCNDCqxXU4F1RUu8wPvC3pnQk6", 300000}, // BOT on testnet
            {"La531vUwiu9NnvtJcwPEjV84HrdKCupFCCb6D7", 300000}, // BAUTO on testnet
            {"La5JGnJcSsLCvYWxqqVSyj3VUqsrAcLBjZjbw5", 300000}, // XYZ from Sam on testnet
            {"La86PtvXGftbwdoZ9rVMKsLQU5nPHganJDsCRq", 300000}  // ON
            //    ,{"LaA5grPQMDhwvciWFqxwG1ySDqNHAgms1yLrPp", 300000}  // NIBBL
        };
    }
};

/**
 * Regression test
 */

class CRegTestParams : public CChainParams
{
public:
    CRegTestParams()
    {
        networkType  = NetworkType::Regtest;
        strNetworkID = "regtest";

        // Set PoW difficulty to easiest
        consensus.bnProofOfWorkLimit = CBigNum(~uint256(0) >> 1);
        // Set PoS difficulty to standard
        consensus.bnProofOfStakeLimit = CBigNum(~uint256(0) >> 20);

        consensus.nStakeTargetSpacingV1 = 120; // 120 seconds block spacing
        consensus.nStakeTargetSpacingV2 = 30;  // 30 seconds block spacing

        /** be aware that this should not be less than the "GetStakeModifierSelectionInterval()".
         * Otherwise, GetKernelStakeModifier() may fail because the starting block isn't far enough in
         * the past
         */
        consensus.nStakeMinAgeV1 = 10 * 60;                  // minimum age for coin age
        consensus.nStakeMinAgeV2 = consensus.nStakeMinAgeV1; // minimum age for coin age
        consensus.nStakeMaxAge   = 7 * 24 * 60 * 60;         // Maximum stake age 7 days
        // time to elapse before new modifier is computed. This should be a little higher than block time
        consensus.nModifierInterval   = 60;
        consensus.nCoinbaseMaturityV1 = 10;
        consensus.nCoinbaseMaturityV2 = 10;
        consensus.nCoinbaseMaturityV3 = 10;
        consensus.nTargetTimespan     = 2 * 60 * 60; // two hours

        consensus.firstValidNTP1Height = 0;

        consensus.nMaxOpReturnSizeV1 = 4096;
        consensus.nMaxOpReturnSizeV2 = 4096;

        // setup forks
        {
            const boost::optional<std::vector<std::string>> forksHeights =
                mapMultiArgs.get("-forksheight");

            // parse the argument
            const boost::container::flat_map<NetworkFork, int> customForks =
                ParseForkHeightsArgs(forksHeights.get_value_or({}));

            boost::container::flat_map<NetworkFork, int> defaultRegtestForkHeights{
                {NetworkFork::NETFORK__1_FIRST_ONE, 1000},
                {NetworkFork::NETFORK__2_CONFS_CHANGE, 2000},
                {NetworkFork::NETFORK__3_TACHYON, 3000},
                {NetworkFork::NETFORK__4_RETARGET_CORRECTION, 4000},
                {NetworkFork::NETFORK__5_COLD_STAKING, -1}};

            // replace the default fork heights
            for (const auto& f : customForks) {
                defaultRegtestForkHeights[f.first] = f.second;
            }

            consensus.forks.emplace(NetworkForks(defaultRegtestForkHeights));
        }

        pchMessageStart[0] = 0xcd;
        pchMessageStart[1] = 0xf3;
        pchMessageStart[2] = 0xe0;
        pchMessageStart[3] = 0xee;
        vAlertPubKey = ParseHex("04da59da7f2e1c9d0f575187065930361ad09751f7a8ccae25f0ab9ebbd479c0cda65a8"
                                "ae0415a4a64bac46f79a4cd67bdb0925871855db3227969005361beaf21");
        nDefaultPort = 26325;

        genesis = std::unique_ptr<CBlock>(
            new CBlock(CreateGenesisBlock(1500674580, 1500674580, 102, PoWLimit().GetCompact(), 1, 0)));
        consensus.hashGenesisBlock = genesis->GetHash();

        assert(consensus.hashGenesisBlock ==
               uint256("0x30a1a5c355bbdee5ad873ea6b1b74dc77052688cf0421f0ce36fdf968c94e9fa"));
        assert(genesis->hashMerkleRoot ==
               uint256("0x7f1bebe1b7fd896ebacb63834ee0b4e55880975aba163047fe061c86911b5749"));

        vAdditionalNodes.clear();

        vDNSSeeds.clear(); //!< Regtest mode doesn't have any DNS seeds.

        checkpointData = {{
            {0, genesis->GetHash()},
        }};

        base58Prefixes[PUBKEY_ADDRESS] = 65;  // Neblio: addresses begin with 'T'
        base58Prefixes[SCRIPT_ADDRESS] = 127; // Neblio: addresses begin with 't'

        fMiningRequiresPeers = false;
        fMineBlocksOnDemand  = true;

        // staking parameters
        nStakeSplitAge          = 60 * 60;
        nStakeCombineThreshold  = 1000 * COIN;
        nMaxInputsInStake       = 10;
        nMaxStakeSearchInterval = 60;
        nMinColdStakingAmount   = 10 * COIN;

        nLastPoWBlock = 1000;

        // Hard checkpoints of stake modifiers to ensure they are deterministic (regtest)
        mapStakeModifierCheckpoints = MapStakeModifierCheckpoints{};

        excludedTxs = MapExcludedTxs{};

        // token id vs max block to take transactions from these tokens due to bugs
        ntp1BlacklistedTokenIds = MapBlacklistedTokens{};
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams& Params()
{
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(NetworkType networkType)
{
    switch (networkType) {
    case NetworkType::Mainnet:
        return std::unique_ptr<CChainParams>(new CMainParams());
    case NetworkType::Testnet:
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    case NetworkType::Regtest:
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    }
    throw std::runtime_error(
        fmt::format("{}: Unknown chain {}.", FUNCTIONSIG, GetChainName(networkType)));
}

void SelectParams(NetworkType networkType)
{
    SelectBaseParams(networkType);
    globalChainParams = CreateChainParams(networkType);
    NLog.write(b_sev::info, "Selected network/chain type: {}", GetChainName(networkType));
}

const CBlock& CChainParams::GenesisBlock() const
{
    assert(genesis);
    return *genesis;
}

const uint256& CChainParams::GenesisBlockHash() const { return consensus.hashGenesisBlock; }

int64_t CChainParams::StakeMinAge(const int blockHeight) const
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, blockHeight)) {
        return consensus.nStakeMinAgeV2;
    } else {
        return consensus.nStakeMinAgeV1;
    }
}

int64_t CChainParams::StakeMaxAge() const { return consensus.nStakeMaxAge; }

int64_t CChainParams::StakeModifierInterval() const { return consensus.nModifierInterval; }

NetworkType CChainParams::NetType() const { return networkType; }

bool CChainParams::PassedFirstValidNTP1Tx(const int blockHeight) const
{
    return blockHeight >= consensus.firstValidNTP1Height;
}

int64_t CChainParams::TargetTimeSpan() const { return consensus.nTargetTimespan; }

unsigned int CChainParams::OpReturnMaxSize(const int blockHeight) const
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, blockHeight)) {
        return consensus.nMaxOpReturnSizeV2;
    } else {
        return consensus.nMaxOpReturnSizeV1;
    }
}

unsigned int CChainParams::TargetSpacing(int blockHeight) const
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, blockHeight)) {
        return consensus.nStakeTargetSpacingV2;
    } else {
        return consensus.nStakeTargetSpacingV1;
    }
}

int CChainParams::CoinbaseMaturity(int blockHeight) const
{
    if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, blockHeight)) {
        return consensus.nCoinbaseMaturityV3;
    } else if (GetNetForks().isForkActivated(NetworkFork::NETFORK__2_CONFS_CHANGE, blockHeight)) {
        return consensus.nCoinbaseMaturityV2;
    } else {
        return consensus.nCoinbaseMaturityV1;
    }
}

const CBigNum& CChainParams::PoWLimit() const { return consensus.bnProofOfWorkLimit; }

const CBigNum& CChainParams::PoSLimit() const { return consensus.bnProofOfStakeLimit; }

const MapStakeModifierCheckpoints& CChainParams::StakeModifierCheckpoints() const
{
    return mapStakeModifierCheckpoints;
}

unsigned CChainParams::StakeSplitAge() const { return nStakeSplitAge; }

int64_t CChainParams::StakeCombineThreshold() const { return nStakeCombineThreshold; }

unsigned int CChainParams::MaxInputsInStake() const { return nMaxInputsInStake; }

bool CChainParams::IsColdStakingEnabled(const int blockHeight) const
{
    return consensus.forks->isForkActivated(NetworkFork::NETFORK__5_COLD_STAKING, blockHeight);
}

CAmount CChainParams::MinColdStakingAmount() const { return nMinColdStakingAmount; }

int CChainParams::MaxStakeSearchInterval() const { return nMaxStakeSearchInterval; }

int CChainParams::LastPoWBlock() const { return nLastPoWBlock; }

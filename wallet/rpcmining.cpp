// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinrpc.h"
#include "db.h"
#include "init.h"
#include "main.h"
#include "miner.h"
#include "script.h"
#include "txdb.h"
#include "txmempool.h"
#include <random>

using namespace json_spirit;
using namespace std;

Value getsubsidy(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("getsubsidy [nTarget]\n"
                            "Returns proof-of-work subsidy value for the specified value of target.");

    return (uint64_t)GetProofOfWorkReward(0);
}

Value getmininginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getmininginfo\n"
                            "Returns an object containing mining-related information.");

    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

    Object obj, diff, weight;
    obj.push_back(Pair("blocks", (int)nBestHeight));
    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx", (uint64_t)nLastBlockTx));

    diff.push_back(Pair("proof-of-work", GetDifficulty()));
    diff.push_back(Pair("proof-of-stake",
                        GetDifficulty(GetLastBlockIndex(boost::atomic_load(&pindexBest).get(), true))));
    diff.push_back(Pair("search-interval", (int)nLastCoinStakeSearchInterval));
    obj.push_back(Pair("difficulty", diff));

    obj.push_back(Pair("blockvalue", (uint64_t)GetProofOfWorkReward(0)));
    obj.push_back(Pair("netmhashps", GetPoWMHashPS()));
    obj.push_back(Pair("netstakeweight", GetPoSKernelPS()));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));
    obj.push_back(Pair("pooledtx", (uint64_t)mempool.size()));

    weight.push_back(Pair("minimum", (uint64_t)nMinWeight));
    weight.push_back(Pair("maximum", (uint64_t)nMaxWeight));
    weight.push_back(Pair("combined", (uint64_t)nWeight));
    obj.push_back(Pair("stakeweight", weight));

    obj.push_back(Pair("stakeinterest", (uint64_t)COIN_YEAR_REWARD));
    obj.push_back(Pair("testnet", Params().NetType() != Mainnet));
    return obj;
}

Value getstakinginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getstakinginfo\n"
                            "Returns an object containing staking-related information.");

    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

    uint64_t     nNetworkWeight = GetPoSKernelPS();
    bool         staking        = nLastCoinStakeSearchInterval && nWeight;
    unsigned int nTS            = Params().TargetSpacing();
    int          nExpectedTime  = staking ? (nTS * nNetworkWeight / nWeight) : -1;

    Object stakingCriteria;

    bool matureCoins      = nWeight;
    bool activeConnection = true;
    if (vNodes.empty()) {
        activeConnection = false;
    }
    bool unlocked = true;
    if (pwalletMain && pwalletMain->IsLocked()) {
        unlocked = false;
    }
    bool synced = true;
    if (IsInitialBlockDownload()) {
        synced = false;
    }

    stakingCriteria.push_back(Pair("mature-coins", matureCoins));
    stakingCriteria.push_back(Pair("wallet-unlocked", unlocked));
    stakingCriteria.push_back(Pair("online", activeConnection));
    stakingCriteria.push_back(Pair("synced", synced));

    Object obj;

    obj.push_back(Pair("enabled", GetBoolArg("-staking", true)));
    obj.push_back(Pair("staking", staking));
    obj.push_back(Pair("staking-criteria", stakingCriteria));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));

    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx", (uint64_t)nLastBlockTx));
    obj.push_back(Pair("pooledtx", (uint64_t)mempool.size()));

    obj.push_back(Pair("difficulty",
                       GetDifficulty(GetLastBlockIndex(boost::atomic_load(&pindexBest).get(), true))));
    obj.push_back(Pair("search-interval", (int)nLastCoinStakeSearchInterval));

    obj.push_back(Pair("weight", (uint64_t)nWeight));
    obj.push_back(Pair("netstakeweight", (uint64_t)nNetworkWeight));

    obj.push_back(Pair("expectedtime", nExpectedTime));

    return obj;
}

Value getworkex(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("getworkex [data, coinbase]\n"
                            "If [data, coinbase] is not specified, returns extended work data.\n");

    if (vNodes.empty())
        throw JSONRPCError(-9, "neblio is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(-10, "neblio is downloading blocks...");

    if (boost::atomic_load(&pindexBest)->nHeight >= LAST_POW_BLOCK)
        throw JSONRPCError(RPC_MISC_ERROR, "No more PoW blocks");

    typedef map<uint256, pair<CBlock*, CScript>> mapNewBlock_t;
    static mapNewBlock_t                         mapNewBlock;
    static vector<CBlock*>                       vNewBlock;
    static CReserveKey                           reservekey(std::atomic_load(&pwalletMain).get());

    if (params.size() == 0) {
        // Update block
        static unsigned int        nTransactionsUpdatedLast;
        static CBlockIndexSmartPtr pindexPrev;
        static int64_t             nStart;
        static CBlock*             pblock;
        if (pindexPrev != pindexBest ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)) {
            if (pindexPrev != pindexBest) {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                BOOST_FOREACH (CBlock* pblock, vNewBlock)
                    delete pblock;
                vNewBlock.clear();
            }
            nTransactionsUpdatedLast = nTransactionsUpdated;
            pindexPrev               = pindexBest;
            nStart                   = GetTime();

            // Create new block
            pblock = CreateNewBlock(pwalletMain.get()).release();
            if (!pblock)
                throw JSONRPCError(-7, "Out of memory");
            vNewBlock.push_back(pblock);
        }

        // Update nTime
        pblock->nTime  = max(pindexPrev->GetPastTimeLimit() + 1, GetAdjustedTime());
        pblock->nNonce = 0;

        // Update nExtraNonce
        static unsigned int nExtraNonce = 0;
        IncrementExtraNonce(pblock, pindexPrev.get(), nExtraNonce);

        // Save
        mapNewBlock[pblock->hashMerkleRoot] = make_pair(pblock, pblock->vtx[0].vin[0].scriptSig);

        // Prebuild hash buffers
        char pmidstate[32];
        char pdata[128];
        char phash1[64];
        FormatHashBuffers(pblock, pmidstate, pdata, phash1);

        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        CTransaction         coinbaseTx = pblock->vtx[0];
        std::vector<uint256> merkle     = pblock->GetMerkleBranch(0);

        Object result;
        result.push_back(Pair("data", HexStr(BEGIN(pdata), END(pdata))));
        result.push_back(Pair("target", HexStr(BEGIN(hashTarget), END(hashTarget))));

        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << coinbaseTx;
        result.push_back(Pair("coinbase", HexStr(ssTx.begin(), ssTx.end())));

        Array merkle_arr;

        BOOST_FOREACH (uint256 merkleh, merkle) {
            merkle_arr.push_back(HexStr(BEGIN(merkleh), END(merkleh)));
        }

        result.push_back(Pair("merkle", merkle_arr));

        return result;
    } else {
        // Parse parameters
        vector<unsigned char> vchData = ParseHex(params[0].get_str());
        vector<unsigned char> coinbase;

        if (params.size() == 2)
            coinbase = ParseHex(params[1].get_str());

        if (vchData.size() != 128)
            throw JSONRPCError(-8, "Invalid parameter");

        CBlock* pdata = (CBlock*)&vchData[0];

        // Byte reverse
        for (int i = 0; i < 128 / 4; i++)
            ((unsigned int*)pdata)[i] = ByteReverse(((unsigned int*)pdata)[i]);

        // Get saved block
        if (!mapNewBlock.count(pdata->hashMerkleRoot))
            return false;
        CBlock* pblock = mapNewBlock[pdata->hashMerkleRoot].first;

        pblock->nTime  = pdata->nTime;
        pblock->nNonce = pdata->nNonce;

        if (coinbase.size() == 0)
            pblock->vtx[0].vin[0].scriptSig = mapNewBlock[pdata->hashMerkleRoot].second;
        else
            CDataStream(coinbase, SER_NETWORK, PROTOCOL_VERSION) >> pblock->vtx[0]; // FIXME - HACK!

        pblock->hashMerkleRoot = pblock->GetMerkleRoot();

        return CheckWork(pblock, *pwalletMain, reservekey);
    }
}

Value getwork(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getwork [data]\n"
            "If [data] is not specified, returns formatted hash data to work on:\n"
            "  \"midstate\" : precomputed hash state after hashing the first half of the data "
            "(DEPRECATED)\n" // deprecated
            "  \"data\" : block data\n"
            "  \"hash1\" : formatted hash buffer for second hash (DEPRECATED)\n" // deprecated
            "  \"target\" : little endian hash target\n"
            "If [data] is specified, tries to solve the block and returns true if it was successful.");

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "neblio is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "neblio is downloading blocks...");

    if (boost::atomic_load(&pindexBest)->nHeight >= LAST_POW_BLOCK)
        throw JSONRPCError(RPC_MISC_ERROR, "No more PoW blocks");

    typedef map<uint256, pair<CBlock*, CScript>> mapNewBlock_t;
    static mapNewBlock_t                         mapNewBlock; // FIXME: thread safety
    static vector<CBlock*>                       vNewBlock;
    static CReserveKey                           reservekey(pwalletMain.get());

    if (params.size() == 0) {
        // Update block
        static unsigned int        nTransactionsUpdatedLast;
        static CBlockIndexSmartPtr pindexPrev;
        static int64_t             nStart;
        static CBlock*             pblock;
        if (pindexPrev != boost::atomic_load(&pindexBest) ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)) {
            if (pindexPrev != pindexBest) {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                for (CBlock* pblock : vNewBlock)
                    delete pblock;
                vNewBlock.clear();
            }

            // Clear pindexPrev so future getworks make a new block, despite any failures from here on
            pindexPrev = NULL;

            // Store the pindexBest used before CreateNewBlock, to avoid races
            nTransactionsUpdatedLast          = nTransactionsUpdated;
            CBlockIndexSmartPtr pindexPrevNew = pindexBest;
            nStart                            = GetTime();

            // Create new block
            pblock = CreateNewBlock(pwalletMain.get()).release();
            if (!pblock)
                throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");
            vNewBlock.push_back(pblock);

            // Need to update only after we know CreateNewBlock succeeded
            pindexPrev = pindexPrevNew;
        }

        // Update nTime
        pblock->UpdateTime(pindexPrev.get());
        pblock->nNonce = 0;

        // Update nExtraNonce
        static unsigned int nExtraNonce = 0;
        IncrementExtraNonce(pblock, pindexPrev.get(), nExtraNonce);

        // Save
        mapNewBlock[pblock->hashMerkleRoot] = make_pair(pblock, pblock->vtx[0].vin[0].scriptSig);

        // Pre-build hash buffers
        char pmidstate[32];
        char pdata[128];
        char phash1[64];
        FormatHashBuffers(pblock, pmidstate, pdata, phash1);

        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        Object result;
        result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate)))); // deprecated
        result.push_back(Pair("data", HexStr(BEGIN(pdata), END(pdata))));
        result.push_back(Pair("hash1", HexStr(BEGIN(phash1), END(phash1)))); // deprecated
        result.push_back(Pair("target", HexStr(BEGIN(hashTarget), END(hashTarget))));
        return result;
    } else {
        // Parse parameters
        vector<unsigned char> vchData = ParseHex(params[0].get_str());
        if (vchData.size() != 128)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
        CBlock* pdata = (CBlock*)&vchData[0];

        // Byte reverse
        for (int i = 0; i < 128 / 4; i++)
            ((unsigned int*)pdata)[i] = ByteReverse(((unsigned int*)pdata)[i]);

        // Get saved block
        if (!mapNewBlock.count(pdata->hashMerkleRoot))
            return false;
        CBlock* pblock = mapNewBlock[pdata->hashMerkleRoot].first;

        pblock->nTime                   = pdata->nTime;
        pblock->nNonce                  = pdata->nNonce;
        pblock->vtx[0].vin[0].scriptSig = mapNewBlock[pdata->hashMerkleRoot].second;
        pblock->hashMerkleRoot          = pblock->GetMerkleRoot();

        return CheckWork(pblock, *pwalletMain, reservekey);
    }
}

Value getblocktemplate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("getblocktemplate [params]\n"
                            "Returns data needed to construct a block to work on:\n"
                            "  \"version\" : block version\n"
                            "  \"previousblockhash\" : hash of current highest block\n"
                            "  \"transactions\" : contents of non-coinbase transactions that should be "
                            "included in the next block\n"
                            "  \"coinbaseaux\" : data that should be included in coinbase\n"
                            "  \"coinbasevalue\" : maximum allowable input to coinbase transaction, "
                            "including the generation award and transaction fees\n"
                            "  \"target\" : hash target\n"
                            "  \"mintime\" : minimum timestamp appropriate for next block\n"
                            "  \"curtime\" : current timestamp\n"
                            "  \"mutable\" : list of ways the block template may be changed\n"
                            "  \"noncerange\" : range of valid nonces\n"
                            "  \"sigoplimit\" : limit of sigops in blocks\n"
                            "  \"sizelimit\" : limit of block size\n"
                            "  \"bits\" : compressed target of next block\n"
                            "  \"height\" : height of the next block\n"
                            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.");

    std::string strMode = "template";
    if (params.size() > 0) {
        const Object& oparam  = params[0].get_obj();
        const Value&  modeval = find_value(oparam, "mode");
        if (modeval.type() == str_type)
            strMode = modeval.get_str();
        else if (modeval.type() == null_type) {
            /* Do nothing */
        } else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
    }

    if (strMode != "template")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "neblio is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "neblio is downloading blocks...");

    if (boost::atomic_load(&pindexBest)->nHeight >= LAST_POW_BLOCK)
        throw JSONRPCError(RPC_MISC_ERROR, "No more PoW blocks");

    static CReserveKey reservekey(pwalletMain.get());

    // Update block
    static unsigned int        nTransactionsUpdatedLast;
    static CBlockIndexSmartPtr pindexPrev;
    static int64_t             nStart;
    static CBlock*             pblock;
    if (pindexPrev != pindexBest ||
        (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 5)) {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = NULL;

        // Store the pindexBest used before CreateNewBlock, to avoid races
        nTransactionsUpdatedLast          = nTransactionsUpdated;
        CBlockIndexSmartPtr pindexPrevNew = boost::atomic_load(&pindexBest);
        nStart                            = GetTime();

        // Create new block
        if (pblock) {
            delete pblock;
            pblock = NULL;
        }
        pblock = CreateNewBlock(pwalletMain.get()).release();
        if (!pblock)
            throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");

        // Need to update only after we know CreateNewBlock succeeded
        pindexPrev = pindexPrevNew;
    }

    // Update nTime
    pblock->UpdateTime(pindexPrev.get());
    pblock->nNonce = 0;

    Array                 transactions;
    map<uint256, int64_t> setTxIndex;
    int                   i = 0;
    CTxDB                 txdb("r");
    BOOST_FOREACH (CTransaction& tx, pblock->vtx) {
        uint256 txHash     = tx.GetHash();
        setTxIndex[txHash] = i++;

        if (tx.IsCoinBase() || tx.IsCoinStake())
            continue;

        Object entry;

        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << tx;
        entry.push_back(Pair("data", HexStr(ssTx.begin(), ssTx.end())));

        entry.push_back(Pair("hash", txHash.GetHex()));

        MapPrevTx              mapInputs;
        map<uint256, CTxIndex> mapUnused;
        bool                   fInvalid = false;
        if (tx.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid)) {
            entry.push_back(Pair("fee", (int64_t)(tx.GetValueIn(mapInputs) - tx.GetValueOut())));

            Array deps;
            BOOST_FOREACH (MapPrevTx::value_type& inp, mapInputs) {
                if (setTxIndex.count(inp.first))
                    deps.push_back(setTxIndex[inp.first]);
            }
            entry.push_back(Pair("depends", deps));

            int64_t nSigOps = tx.GetLegacySigOpCount();
            nSigOps += tx.GetP2SHSigOpCount(mapInputs);
            entry.push_back(Pair("sigops", nSigOps));
        }

        transactions.push_back(entry);
    }

    Object aux;
    aux.push_back(Pair("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end())));

    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

    static Array aMutable;
    if (aMutable.empty()) {
        aMutable.push_back("time");
        aMutable.push_back("transactions");
        aMutable.push_back("prevblock");
    }

    Object result;
    result.push_back(Pair("version", pblock->nVersion));
    result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));
    result.push_back(Pair("transactions", transactions));
    result.push_back(Pair("coinbaseaux", aux));
    result.push_back(Pair("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nValue));
    result.push_back(Pair("target", hashTarget.GetHex()));
    result.push_back(Pair("mintime", (int64_t)pindexPrev->GetPastTimeLimit() + 1));
    result.push_back(Pair("mutable", aMutable));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    result.push_back(Pair("sigoplimit", (int64_t)MAX_BLOCK_SIGOPS));
    result.push_back(Pair("sizelimit", (int64_t)MaxBlockSize()));
    result.push_back(Pair("curtime", (int64_t)pblock->nTime));
    result.push_back(Pair("bits", strprintf("%08x", pblock->nBits)));
    result.push_back(Pair("height", (int64_t)(pindexPrev->nHeight + 1)));

    return result;
}

Value submitblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("submitblock <hex data> [optional-params-obj]\n"
                            "[optional-params-obj] parameter is currently ignored.\n"
                            "Attempts to submit new block to network.\n"
                            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.");

    vector<unsigned char> blockData(ParseHex(params[0].get_str()));
    CDataStream           ssBlock(blockData, SER_NETWORK, PROTOCOL_VERSION);
    CBlock                block;
    try {
        ssBlock >> block;
    } catch (std::exception& e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");
    }

    bool fAccepted = ProcessBlock(NULL, &block);
    if (!fAccepted)
        return "rejected";

    return Value::null;
}

boost::optional<unsigned> MineBlock(CBlock block, uint64_t nMaxTries)
{
    using NonceType = unsigned;
    static_assert(std::is_same<NonceType, decltype(block.nNonce)>::value,
                  "Nonce type is expected to be unsigned");

    auto seed = std::random_device{}();

    std::mt19937                             gen(seed);
    std::uniform_int_distribution<NonceType> dis;
    while (nMaxTries > 0) {
        --nMaxTries;
        block.nNonce = dis(gen);
        if (!CheckProofOfWork(block.GetHash(), block.nBits, true)) {
            continue;
        } else {
            return block.nNonce;
        }
    }
    return boost::none;
}

Value generateBlocks(int nGenerate, uint64_t nMaxTries, CWallet* const pwallet,
                     const boost::optional<CBitcoinAddress>& destinationAddress = boost::none)
{
    static const int nInnerLoopCount = 0x10000;
    int              nHeightEnd      = 0;
    int              nHeight         = nBestHeight.load();

    nHeightEnd = nBestHeight.load() + nGenerate;

    unsigned int       nExtraNonce = 0;
    json_spirit::Array blockHashes;

    // generate a new address
    const CBitcoinAddress destination = [&destinationAddress]() {
        if (destinationAddress) {
            return *destinationAddress;
        }

        CPubKey newKey;
        if (!pwalletMain->GetKeyFromPool(newKey, false))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT,
                               "Error: Keypool ran out, please call keypoolrefill first");
        CKeyID keyID = newKey.GetID();

        pwalletMain->SetAddressBookName(keyID, "");

        return CBitcoinAddress(keyID);
    }();

    while (nHeight < nHeightEnd) {
        std::unique_ptr<CBlock> pblock = CreateNewBlock(pwallet, false, 0, destination);
        if (!pblock)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock.get(), pindexBest.get(), nExtraNonce);
        }

        boost::optional<unsigned int> nonce = MineBlock(*pblock, nMaxTries);
        if (nonce) {
            pblock->nNonce = *nonce;
        } else {
            // failed to mine
            break;
        }

        if (pblock->nNonce == nInnerLoopCount) {
            continue;
        }

        // peercoin: sign block
        // rfc6: we sign proof of work blocks only before 0.8 fork
        //        if (!pblock->SignBlock(*pwallet, 0))
        //            throw JSONRPCError(-100, "Unable to sign block, wallet locked?");

        if (!ProcessBlock(nullptr, pblock.get()))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());

        // mark script as important because it was used at least for one coinbase output if the script
        // came from the wallet
        //        if (keepScript) {
        //            coinbaseScript->KeepScript();
        //        }
    }
    return Value(blockHashes);
}

Value generatePOSBlocks(int nGenerate, CWallet* const pwallet)
{
    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_INVALID_REQUEST, "This method can only be used on regtest");

    int nHeightEnd = 0;
    int nHeight    = nBestHeight.load();

    nHeightEnd = nBestHeight.load() + nGenerate;

    json_spirit::Array blockHashes;

    while (nHeight < nHeightEnd) {
        std::unique_ptr<CBlock> pblock;
        {
            pblock = CreateNewBlock(pwallet, true, 0);
            if (!pblock)
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");

            if (pblock->SignBlock(*pwallet, 0)) {
                if (!CheckStake(pblock.get(), *pwallet))
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "CheckStake, CheckStake failed");

            } else {
                pblock.reset();
            }
        }

        if (!pblock) {
            // staking failed
            break;
        }

        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());
    }
    return Value(blockHashes);
}

Value generate(const Array& params, bool fHelp)
{
    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_INVALID_REQUEST, "This method can only be used on regtest");

    EnsureWalletIsUnlocked();

    CWallet* const pwallet = pwalletMain.get();

    if (fHelp || params.size() < 1 || params.size() > 2) {
        throw std::runtime_error(
            "generate nblocks ( maxtries )\n"
            "\nMine up to nblocks blocks immediately (before the RPC call returns) to an address in the "
            "wallet.\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. maxtries     (numeric, optional) How many iterations to try (default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            "generate 11");
    }

    int      num_generate = params[0].get_int();
    uint64_t max_tries    = 1000000;
    if (params.size() > 1 && params[1].type() != null_type) {
        max_tries = params[1].get_int();
    }
    //    std::shared_ptr<CReserveScript> coinbase_script;
    //    pwallet->GetScriptForMining(coinbase_script);

    //    // If the keypool is exhausted, no script is returned at all.  Catch this.
    //    if (!coinbase_script) {
    //        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT,
    //                           "Error: Keypool ran out, please call keypoolrefill first");
    //    }

    //    // throw an error if no script was provided
    //    if (coinbase_script->reserveScript.empty()) {
    //        throw JSONRPCError(RPC_INTERNAL_ERROR, "No coinbase script available");
    //    }

    return generateBlocks(num_generate, max_tries, pwallet);
}

Value generatepos(const Array& params, bool fHelp)
{
    EnsureWalletIsUnlocked();

    CWallet* const pwallet = pwalletMain.get();

    if (fHelp || params.size() < 1 || params.size() > 1) {
        throw std::runtime_error(
            "generate nblocks\n"
            "\nMine one block with proof of stake immediately (before the RPC call returns)\n"
            "\nBe aware that staking is a complex principle that requires accurate timing. It's "
            "recommended that you manage timing manually and generate only 1 block at a time\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. count        (numeric, required) Number of blocks to generate.\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            "generate 11");
    }

    int num_generate = params[0].get_int();

    return generatePOSBlocks(num_generate, pwallet);
}

Value generatetoaddress(const Array& params, bool fHelp)
{
    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_INVALID_REQUEST, "This method can only be used on regtest");

    EnsureWalletIsUnlocked();

    CWallet* const pwallet = pwalletMain.get();

    if (fHelp || params.size() < 2 || params.size() > 3)
        throw std::runtime_error(
            "generatetoaddress nblocks address (maxtries)\n"
            "\nMine blocks immediately to a specified address (before the RPC call returns)\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. address      (string, required) The address to send the newly generated nebls to.\n"
            "3. maxtries     (numeric, optional) How many iterations to try (default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks to myaddress\n");

    int      num_generate = params[0].get_int();
    uint64_t max_tries    = 1000000;
    if (params.size() > 2 && params[2].type() != Value_type::null_type) {
        max_tries = params[2].get_int();
    }

    CBitcoinAddress destination(params[1].get_str());
    if (!destination.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Error: Invalid address");
    }

    return generateBlocks(num_generate, max_tries, pwallet, destination);
}

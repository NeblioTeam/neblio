// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinrpc.h"
#include "consensus.h"
#include "db.h"
#include "init.h"
#include "main.h"
#include "messaging.h"
#include "miner.h"
#include "script.h"
#include "txdb.h"
#include "txmempool.h"
#include "wallet_interface.h"
#include "work.h"
#include <random>

using namespace json_spirit;
using namespace std;

Value getsubsidy(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("getsubsidy [nTarget]\n"
                            "Returns proof-of-work subsidy value for the specified value of target.");

    return (uint64_t)GetProofOfWorkReward(CTxDB(), 0);
}

Value getmininginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getmininginfo\n"
                            "Returns an object containing mining-related information.");

    const CTxDB txdb;

    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    pwalletMain->GetStakeWeight(txdb, nMinWeight, nMaxWeight, nWeight);

    boost::optional<CBlockIndex> bestBlockIndex = CTxDB().GetBestBlockIndex();

    Object obj, diff, weight;
    obj.push_back(Pair("blocks", (int)bestBlockIndex->nHeight));
    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx", (uint64_t)nLastBlockTx));

    diff.push_back(Pair("proof-of-work", GetDifficulty()));
    const CBlockIndex bi = GetLastBlockIndex(*bestBlockIndex, true, txdb);
    diff.push_back(Pair("proof-of-stake", GetDifficulty(&bi)));
    diff.push_back(Pair("search-interval", (int)stakeMaker.getLastCoinStakeSearchInterval()));
    obj.push_back(Pair("difficulty", diff));

    obj.push_back(Pair("blockvalue", (uint64_t)GetProofOfWorkReward(txdb, 0)));
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

    const uint64_t     nNetworkWeight = GetPoSKernelPS();
    const bool         staking        = stakeMaker.IsStakingActive();
    const uint64_t     nWeight        = stakeMaker.getLatestStakeWeight().value_or(0);
    const unsigned int nTS            = Params().TargetSpacing(CTxDB());
    const int          nExpectedTime  = staking && !!nWeight ? (nTS * nNetworkWeight / nWeight) : -1;

    Object stakingCriteria;

    const bool matureCoins = !!nWeight;

    bool activeConnection = []() {
        LOCK(cs_vNodes);
        return !vNodes.empty();
    }();

    const CTxDB txdb;

    const bool unlocked = !(pwalletMain && pwalletMain->IsLocked());
    const bool synced   = !IsInitialBlockDownload(txdb);

    const std::size_t stakableCoinsCount = [&]() {
        std::vector<COutput> vCoins;
        bool                 fIncludeColdStaking =
            Params().IsColdStakingEnabled(txdb) && GetBoolArg("-coldstaking", true);
        pwalletMain->AvailableCoinsForStaking(txdb, vCoins, GetAdjustedTime(), fIncludeColdStaking,
                                              false);
        return vCoins.size();
    }();

    stakingCriteria.push_back(Pair("mature-coins", matureCoins));
    stakingCriteria.push_back(Pair("wallet-unlocked", unlocked));
    stakingCriteria.push_back(Pair("online", activeConnection));
    stakingCriteria.push_back(Pair("synced", synced));

    Object obj;

    obj.push_back(Pair("enabled", GetBoolArg("-staking", true)));
    obj.push_back(Pair("staking", staking));
    obj.push_back(Pair("staking-criteria", stakingCriteria));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));

    obj.push_back(Pair("stakableoutputs", (int)stakableCoinsCount));

    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx", (uint64_t)nLastBlockTx));
    obj.push_back(Pair("pooledtx", (uint64_t)mempool.size()));

    const CBlockIndex bi = GetLastBlockIndex(*CTxDB().GetBestBlockIndex(), true, txdb);
    obj.push_back(Pair("difficulty", GetDifficulty(&bi)));
    obj.push_back(Pair("search-interval", (int)stakeMaker.getLastCoinStakeSearchInterval()));

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

    const CTxDB txdb;

    if (IsInitialBlockDownload(txdb))
        throw JSONRPCError(-10, "neblio is downloading blocks...");

    if (txdb.GetBestChainHeight().value_or(0) >= Params().LastPoWBlock())
        throw JSONRPCError(RPC_MISC_ERROR, "No more PoW blocks");

    typedef map<uint256, pair<CBlock*, CScript>> mapNewBlock_t;
    static mapNewBlock_t                         mapNewBlock;
    static vector<CBlock*>                       vNewBlock;
    static CReserveKey                           reservekey(std::atomic_load(&pwalletMain).get());

    if (params.size() == 0) {
        // Update block
        static unsigned int                 nTransactionsUpdatedLast;
        static boost::optional<CBlockIndex> pindexPrev;
        static int64_t                      nStart;
        static CBlock*                      pblock;
        auto                                bestBlockIndex = CTxDB().GetBestBlockIndex();
        if (pindexPrev->GetBlockHash() != bestBlockIndex->GetBlockHash() ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)) {
            if (pindexPrev->GetBlockHash() != bestBlockIndex->GetBlockHash()) {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                for (CBlock* pblockP : vNewBlock)
                    delete pblockP;
                vNewBlock.clear();
            }
            nTransactionsUpdatedLast = nTransactionsUpdated;
            pindexPrev               = bestBlockIndex;
            nStart                   = GetTime();

            // Create new block
            pblock = CreateNewBlock(pwalletMain.get()).release();
            if (!pblock)
                throw JSONRPCError(-7, "Out of memory");
            vNewBlock.push_back(pblock);
        }

        // Update nTime
        pblock->nTime  = max(pindexPrev->GetPastTimeLimit(txdb) + 1, GetAdjustedTime());
        pblock->nNonce = 0;

        // Update nExtraNonce
        static unsigned int nExtraNonce = 0;
        IncrementExtraNonce(pblock, &*pindexPrev, nExtraNonce);

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

    const CTxDB txdb;

    if (IsInitialBlockDownload(txdb))
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "neblio is downloading blocks...");

    auto bestBlockIndex = txdb.GetBestBlockIndex();
    if (bestBlockIndex->nHeight >= Params().LastPoWBlock())
        throw JSONRPCError(RPC_MISC_ERROR, "No more PoW blocks");

    typedef map<uint256, pair<CBlock*, CScript>> mapNewBlock_t;
    static mapNewBlock_t                         mapNewBlock; // FIXME: thread safety
    static vector<CBlock*>                       vNewBlock;
    static CReserveKey                           reservekey(pwalletMain.get());

    if (params.size() == 0) {
        // Update block
        static unsigned int                 nTransactionsUpdatedLast;
        static boost::optional<CBlockIndex> pindexPrev;
        static int64_t                      nStart;
        static CBlock*                      pblock;
        if (pindexPrev->GetBlockHash() != bestBlockIndex->GetBlockHash() ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)) {
            if (pindexPrev->GetBlockHash() != bestBlockIndex->GetBlockHash()) {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                for (CBlock* pblockP : vNewBlock)
                    delete pblockP;
                vNewBlock.clear();
            }

            // Clear pindexPrev so future getworks make a new block, despite any failures from here on
            pindexPrev = boost::none;

            // Store the pindexBest used before CreateNewBlock, to avoid races
            nTransactionsUpdatedLast                   = nTransactionsUpdated;
            boost::optional<CBlockIndex> pindexPrevNew = bestBlockIndex;
            nStart                                     = GetTime();

            // Create new block
            pblock = CreateNewBlock(pwalletMain.get()).release();
            if (!pblock)
                throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");
            vNewBlock.push_back(pblock);

            // Need to update only after we know CreateNewBlock succeeded
            pindexPrev = pindexPrevNew;
        }

        // Update nTime
        pblock->UpdateTime(&*pindexPrev);
        pblock->nNonce = 0;

        // Update nExtraNonce
        static unsigned int nExtraNonce = 0;
        IncrementExtraNonce(pblock, &*pindexPrev, nExtraNonce);

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

    const CTxDB txdb;

    if (IsInitialBlockDownload(txdb))
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "neblio is downloading blocks...");

    if (txdb.GetBestChainHeight().value_or(0) >= Params().LastPoWBlock())
        throw JSONRPCError(RPC_MISC_ERROR, "No more PoW blocks");

    static CReserveKey reservekey(pwalletMain.get());

    // Update block
    static unsigned int                 nTransactionsUpdatedLast;
    static boost::optional<CBlockIndex> pindexPrev;
    static int64_t                      nStart;
    static CBlock*                      pblock;
    auto                                bestBlockIndex = CTxDB().GetBestBlockIndex();
    if (pindexPrev->GetBlockHash() != bestBlockIndex->GetBlockHash() ||
        (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 5)) {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = boost::none;

        // Store the pindexBest used before CreateNewBlock, to avoid races
        nTransactionsUpdatedLast                   = nTransactionsUpdated;
        boost::optional<CBlockIndex> pindexPrevNew = bestBlockIndex;
        nStart                                     = GetTime();

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
    pblock->UpdateTime(&*pindexPrev);
    pblock->nNonce = 0;

    Array                 transactions;
    map<uint256, int64_t> setTxIndex;
    int                   i = 0;

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
    result.push_back(Pair("mintime", (int64_t)pindexPrev->GetPastTimeLimit(txdb) + 1));
    result.push_back(Pair("mutable", aMutable));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    result.push_back(Pair("sigoplimit", (int64_t)MAX_BLOCK_SIGOPS));
    result.push_back(Pair("sizelimit", (int64_t)MaxBlockSize(txdb)));
    result.push_back(Pair("curtime", (int64_t)pblock->nTime));
    result.push_back(Pair("bits", fmt::format("{:08x}", pblock->nBits)));
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
    int              nHeight         = CTxDB().GetBestChainHeight().value_or(0);

    nHeightEnd = CTxDB().GetBestChainHeight().value_or(0) + nGenerate;

    unsigned int       nExtraNonce = 0;
    json_spirit::Array blockHashes;

    // generate a new address
    const CBitcoinAddress destination = [&destinationAddress]() {
        if (destinationAddress) {
            return *destinationAddress;
        }

        CPubKey newKey;
        if (!pwalletMain->GetKeyFromPool(newKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT,
                               "Error: Keypool ran out, please call keypoolrefill first");
        CKeyID keyID = newKey.GetID();

        pwalletMain->SetAddressBookEntry(keyID, "");

        return CBitcoinAddress(keyID);
    }();

    while (nHeight < nHeightEnd) {
        std::unique_ptr<CBlock> pblock = CreateNewBlock(pwallet, false, 0, destination);
        if (!pblock)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        {
            LOCK(cs_main);
            const boost::optional<CBlockIndex> best = CTxDB().GetBestBlockIndex();
            IncrementExtraNonce(pblock.get(), &*best, nExtraNonce);
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

Value generatePOSBlocks(
    int nGenerate, CWallet* const pwallet, bool submitBlock = true,
    const boost::optional<std::set<std::pair<uint256, unsigned>>>& customInputs        = boost::none,
    CAmount                                                        extraPayoutForTests = 0)
{
    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_INVALID_REQUEST, "This method can only be used on regtest");

    const CTxDB txdb;

    int nHeightEnd = 0;
    int nHeight    = txdb.GetBestChainHeight().value_or(0);

    nHeightEnd = txdb.GetBestChainHeight().value_or(0) + nGenerate;

    json_spirit::Array blockHashesOrSerializedData;

    const auto BlockMaker = [&]() {
        std::unique_ptr<CBlock> block = CreateNewBlock(pwallet, true, 0);
        if (!block)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");

        if (block->SignBlock(txdb, *pwallet, 0, customInputs, extraPayoutForTests)) {
            if (submitBlock) {
                if (!CheckStake(txdb, block.get(), *pwallet))
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "CheckStake, CheckStake failed");
            }
        } else {
            block.reset();
        }
        return block;
    };

    while (nHeight < nHeightEnd) {
        const std::unique_ptr<CBlock> pblock = BlockMaker();

        if (!pblock) {
            // staking failed
            break;
        }

        ++nHeight;
        if (submitBlock) {
            blockHashesOrSerializedData.push_back(pblock->GetHash().GetHex());
        } else {
            // if block is not submitted, return the serialized format
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << *pblock;
            std::string blockRaw = ss.str();
            blockHashesOrSerializedData.push_back(HexStr(std::make_move_iterator(blockRaw.begin()),
                                                         std::make_move_iterator(blockRaw.end())));
        }
    }
    return Value(blockHashesOrSerializedData);
}

Value generate(const Array& params, bool fHelp)
{
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

    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_INVALID_REQUEST, "This method can only be used on regtest");

    EnsureWalletIsUnlocked();

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

std::set<std::pair<uint256, unsigned>> ParseCustomInputs(const Value& inputsArray)
{
    std::set<std::pair<uint256, unsigned>> result;
    if (inputsArray.type() == Value_type::array_type) {
        Array inputs = inputsArray.get_array();
        for (const Value& el : inputs) {
            if (el.type() != Value_type::array_type) {
                throw JSONRPCError(RPC_TYPE_ERROR,
                                   "Error: Second parameter's elements must be arrays, each are a "
                                   "pair of string (hash) and output index (int) (error A)");
            }
            Array p = el.get_array();
            if (p.size() != 2) {
                throw JSONRPCError(RPC_TYPE_ERROR,
                                   "Error: Second parameter's elements must be arrays, each are a "
                                   "pair of string (hash) and output index (int) (error B)");
            }
            if (p[0].type() != Value_type::str_type) {
                throw JSONRPCError(RPC_TYPE_ERROR,
                                   "Error: Second parameter's elements must be arrays, each are a "
                                   "pair of string (hash) and output index (int) (first element of "
                                   "a pair is not a string)");
            }
            if (p[1].type() != Value_type::int_type) {
                throw JSONRPCError(RPC_TYPE_ERROR,
                                   "Error: Second parameter's elements must be arrays, each are a "
                                   "pair of string (hash) and output index (int) (second element of "
                                   "a pair is not an int)");
            }
            int outIndexS = p[1].get_int();
            if (outIndexS < 0) {
                throw JSONRPCError(RPC_TYPE_ERROR,
                                   "Error: Second parameter's elements must be arrays, each are a "
                                   "pair of string (hash) and output index (int) (second element of "
                                   "a pair is < 0)");
            }
            unsigned outIndex = static_cast<unsigned>(outIndexS);
            result.insert(std::make_pair(uint256(p[0].get_str()), outIndex));
        }
        return result;
    } else {
        throw JSONRPCError(RPC_TYPE_ERROR, "Error: Second parameter must be an array of pairs");
    }
}

Value generatepos(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4) {
        throw std::runtime_error(
            "generate nblocks\n"
            "\nMine one block with proof of stake immediately (before the RPC call returns)\n"
            "\nBe aware that staking is a complex principle that requires accurate timing. It's "
            "recommended that you manage timing manually and generate only 1 block at a time\n"
            "\nArguments:\n"
            "1. count         (numeric, required) Number of blocks to generate.\n"
            "2. submit block  (bool, optional, default=true) whether to submit the block after creating "
            "it or return its serialized hex form"
            "3. inputs to use (optional list of pairs (list of two elements), every one is a hash and "
            "an output index); the staker will filter the available outputs with these"
            "4. extra payout for tests  (numeric, optional): Extra payout for the stake for testing; "
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            "generate 11");
    }

    EnsureWalletIsUnlocked();

    CWallet* const pwallet = pwalletMain.get();

    int num_generate = params[0].get_int();

    bool fSubmitBlock = true;
    if (params.size() > 1 && params[1].type() == Value_type::bool_type) {
        fSubmitBlock = params[1].get_bool();
    }

    boost::optional<std::set<std::pair<uint256, unsigned>>> customInputs;
    if (params.size() > 2 && params[2].type() != Value_type::null_type) {
        customInputs = ParseCustomInputs(params[2]);
    }

    CAmount extraPayoutForTests = 0;
    if (params.size() > 3 && params[3].type() != Value_type::null_type) {
        extraPayoutForTests = params[3].get_int();
    }

    return generatePOSBlocks(num_generate, pwallet, fSubmitBlock, customInputs, extraPayoutForTests);
}

Value generatetoaddress(const Array& params, bool fHelp)
{
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

    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_INVALID_REQUEST, "This method can only be used on regtest");

    EnsureWalletIsUnlocked();

    CWallet* const pwallet = pwalletMain.get();

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

static CKey WIFSecretToKey(const std::string& wifKey)
{
    // decode the wif string of the private key
    CBitcoinSecret bitcoinSecret;
    bitcoinSecret.SetString(wifKey);
    bool fCompressed = false;

    // set the ECC key of the secret
    CKey          key;
    const CSecret secret = bitcoinSecret.GetSecret(fCompressed);
    key.SetSecret(secret, fCompressed);

    return key;
}

static CTransaction TxFromHex(const std::string& tx_hex)
{
    vector<unsigned char> blockData(ParseHex(tx_hex));
    CDataStream           ssTx(blockData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction          tx;
    try {
        ssTx >> tx;
    } catch (std::exception& e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Failed to deserialize transaction");
    }
    return tx;
}

Value generateblockwithkey(const Array& params, bool fHelp)
{
    // clang-format off
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw std::runtime_error(
            "generateblockwithkey output-tx-hash output-index private-key txs (maxtries)\n"
            "\nMine blocks immediately to a specified address (before the RPC call returns)\n"
            "\nArguments:\n"
            "1. output-tx-hash (string, required) The output transaction that should be used as stake kernel.\n"
            "2. output-index   (int, required) The index of the output to be used in the transaction above.\n"
            "3. private-key    (string, required) The private key of the address the UTXO is from.\n"
            "4. txs            (list<string>, optional) raw txs to include in the block\n"
            "5. maxtries       (numeric, optional) How many times to try to create the block (default = 10000).\n"
            "\nResult:\n"
            "raw block;  raw block as hex string, to be submitted using `submitblock` rpc function\n"
            "\nExamples:\n\n"
            "generateblockwithkey 0xabcdefg 1 Vxyzabc [0xabc, 0xdef]\n");
    // clang-format on

    uint256    outputHash(params[0].get_str());
    uint32_t   outputIndex = static_cast<uint32_t>(params[1].get_int());
    const CKey key         = WIFSecretToKey(params[2].get_str());
    uint32_t   maxRetries  = 10000;

    const CTxDB txdb;

    std::vector<CTransaction> txs;
    if (params.size() > 3) {
        if (params[3].type() != Value_type::array_type) {
            throw JSONRPCError(RPC_INVALID_PARAMS,
                               "Parameter txs must be an array of strings (top type isn't an array)");
        }
        const Array& txs_json = params[3].get_array();
        for (const Value& val : txs_json) {
            if (val.type() != Value_type::str_type) {
                throw JSONRPCError(RPC_INVALID_PARAMS,
                                   "Parameter txs must be an array of strings (element is not str)");
            }
            txs.push_back(TxFromHex(val.get_str()));
        }
    }
    if (params.size() > 4) {
        maxRetries = static_cast<uint32_t>(params[4].get_int());
    }

    const auto BlockMaker = [&]() {
        std::unique_ptr<CBlock> block = CreateNewBlock(nullptr, true, 0);
        if (!block)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't assemble new block");
        block->vtx.insert(block->vtx.end(), txs.begin(), txs.end());

        if (!block->SignBlockWithSpecificKey(txdb, COutPoint(outputHash, outputIndex), key, 0)) {
            block.reset();
        }
        return block;
    };

    for (uint32_t i = 0; i < maxRetries; i++) {
        const std::unique_ptr<CBlock> block = BlockMaker();
        if (block) {
            CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
            ssBlock << *block;
            std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
            return strHex;
        }
    }

    throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block; max retries exceeded");
}

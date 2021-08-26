// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "bitcoinrpc.h"
#include "blockmetadata.h"
#include "bootstraptools.h"
#include "consensus.h"
#include "main.h"
#include "merkletx.h"
#include "txdb.h"
#include "txmempool.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

using namespace json_spirit;
using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry,
                     bool ignoreNTP1 = false);
extern enum Checkpoints::CPMode CheckpointsMode;

double GetDifficulty(const CBlockIndex* pblockindex)
{
    const CTxDB txdb;

    CBlockIndex blockIndex;
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (pblockindex == nullptr) {
        auto bestBlockIndex = txdb.GetBestBlockIndex();
        if (!bestBlockIndex)
            return 1.0;
        else {
            blockIndex = GetLastBlockIndex(*bestBlockIndex, false, txdb);
        }
    } else {
        blockIndex = *pblockindex;
    }

    int nShift = (blockIndex.nBits >> 24) & 0xff;

    double dDiff = (double)0x0000ffff / (double)(blockIndex.nBits & 0x00ffffff);

    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GetPoWMHashPS()
{
    if (CTxDB().GetBestBlockIndex()->nHeight >= Params().LastPoWBlock())
        return 0;

    int     nPoWInterval          = 72;
    int64_t nTargetSpacingWorkMin = 30, nTargetSpacingWork = 30;

    boost::optional<CBlockIndex> pindex         = *pindexGenesisBlock;
    boost::optional<CBlockIndex> pindexPrevWork = *pindexGenesisBlock;

    const CTxDB txdb;

    while (pindex) {
        if (pindex->IsProofOfWork()) {
            int64_t nActualSpacingWork = pindex->GetBlockTime() - pindexPrevWork->GetBlockTime();
            nTargetSpacingWork =
                ((nPoWInterval - 1) * nTargetSpacingWork + nActualSpacingWork + nActualSpacingWork) /
                (nPoWInterval + 1);
            nTargetSpacingWork = max(nTargetSpacingWork, nTargetSpacingWorkMin);
            pindexPrevWork     = pindex;
        }

        pindex = pindex->getNext(txdb);
    }

    return GetDifficulty() * 4294.967296 / nTargetSpacingWork;
}

double GetPoSKernelPS()
{
    int    nPoSInterval          = 72;
    double dStakeKernelsTriedAvg = 0;
    int    nStakesHandled = 0, nStakesTime = 0;

    const CTxDB txdb;

    boost::optional<CBlockIndex> pindex = txdb.GetBestBlockIndex();
    // we use a unique_ptr just because optional... triggers an uninitialized false positive warning
    std::unique_ptr<CBlockIndex> pindexPrevStake;

    while (pindex && nStakesHandled < nPoSInterval) {
        if (pindex->IsProofOfStake()) {
            dStakeKernelsTriedAvg += GetDifficulty(&*pindex) * 4294967296.0;
            if (pindexPrevStake) {
                nStakesTime += pindexPrevStake->nTime - pindex->nTime;
            }
            pindexPrevStake = MakeUnique<CBlockIndex>(*pindex);
            nStakesHandled++;
        }

        pindex = pindex->getPrev(txdb);
    }

    return nStakesTime ? dStakeKernelsTriedAvg / nStakesTime : 0;
}

Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail,
                   bool ignoreNTP1 = false)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    const CTxDB txdb;
    int         confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (blockindex->IsInMainChain(txdb))
        confirmations = txdb.GetBestChainHeight().value_or(0) - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    const boost::optional<BlockMetadata> blockMetadata = txdb.ReadBlockMetadata(block.GetHash());
    result.push_back(
        Pair("mint", blockMetadata ? ValueFromAmount(blockMetadata->getMint()) : "<ERROR>"));
    result.push_back(Pair("time", (int64_t)block.GetBlockTime()));
    result.push_back(Pair("nonce", (uint64_t)block.nNonce));
    if (blockindex->nHeight > Params().LastPoWBlock() && block.nNonce > 0) {
        const VoteValueAndID voteValueAndID = VoteValueAndID::CreateVoteFromUint32(block.nNonce);
        result.push_back(Pair("votevalue", voteValueAndID.toJson()));
    } else {
        result.push_back(Pair("votevalue", json_spirit::Value()));
    }
    result.push_back(Pair("bits", fmt::format("{:08x}", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("blocktrust", leftTrim(blockindex->GetBlockTrust().GetHex(), '0')));
    result.push_back(Pair("chaintrust", leftTrim(blockindex->nChainTrust.GetHex(), '0')));
    if (const auto o = blockindex->getPrev(txdb))
        result.push_back(Pair("previousblockhash", o->GetBlockHash().GetHex()));
    if (const auto o = blockindex->getNext(txdb))
        result.push_back(Pair("nextblockhash", o->GetBlockHash().GetHex()));

    result.push_back(Pair(
        "flags", fmt::format("{}{}", blockindex->IsProofOfStake() ? "proof-of-stake" : "proof-of-work",
                             blockindex->GeneratedStakeModifier() ? " stake-modifier" : "")));
    result.push_back(Pair("proofhash", blockindex->hashProof.GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", fmt::format("{:016x}", blockindex->nStakeModifier)));
    result.push_back(
        Pair("modifierchecksum", fmt::format("{:08x}", blockindex->nStakeModifierChecksum)));

    Array txinfo;
    for (const CTransaction& tx : block.vtx) {
        if (fPrintTransactionDetail) {
            Object entry;

            TxToJSON(tx, 0, entry, ignoreNTP1);

            txinfo.push_back(entry);
        } else
            txinfo.push_back(tx.GetHash().GetHex());
    }

    result.push_back(Pair("tx", txinfo));

    if (block.IsProofOfStake())
        result.push_back(Pair("signature", HexStr(block.vchBlockSig.begin(), block.vchBlockSig.end())));

    return result;
}

Value getbestblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getbestblockhash\n"
                            "Returns the hash of the best block in the longest block chain.");

    return CTxDB().GetBestBlockHash().GetHex();
}

Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getblockcount\n"
                            "Returns the number of blocks in the longest block chain.");

    return CTxDB().GetBestChainHeight().value_or(0);
}

Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getdifficulty\n"
                            "Returns the difficulty as a multiple of the minimum difficulty.");

    Object obj;
    obj.push_back(Pair("proof-of-work", GetDifficulty()));
    const CBlockIndex bi = GetLastBlockIndex(*CTxDB().GetBestBlockIndex(), true, CTxDB());
    obj.push_back(Pair("proof-of-stake", GetDifficulty(&bi)));
    obj.push_back(Pair("search-interval", (int)stakeMaker.getLastCoinStakeSearchInterval()));
    return obj;
}

Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1 || AmountFromValue(params[0]) < MIN_TX_FEE)
        throw runtime_error("settxfee <amount>\n"
                            "<amount> is a real and is rounded to the nearest 0.01");

    nTransactionFee = AmountFromValue(params[0]);
    nTransactionFee = (nTransactionFee / MIN_TX_FEE) * MIN_TX_FEE; // round to nearest 0.0001

    return true;
}

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getrawmempool\n"
                            "Returns all transaction ids in memory pool.");

    vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    Array a;
    BOOST_FOREACH (const uint256& hash, vtxid)
        a.push_back(hash.ToString());

    return a;
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getblockhash <index>\n"
                            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > CTxDB().GetBestChainHeight().value_or(0))
        throw runtime_error("Block number out of range.");

    boost::optional<CBlockIndex> pblockindex = CBlock::FindBlockByHeight(nHeight);
    if (!pblockindex) {
        throw std::runtime_error(fmt::format("Failed to find block at height {}", nHeight));
    }
    return pblockindex->blockHash.GetHex();
}

Value calculateblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getblockhash <block_serialized_hex>\n"
                            "Returns hash of block that is sent here (used for test purposes).");

    std::string blockHexData = params[0].get_str();

    CDataStream ssBlock(ParseHex(blockHexData), SER_NETWORK, PROTOCOL_VERSION);
    CBlock      block;
    ssBlock >> block;
    block.print();

    return block.GetHash().GetHex();
}

Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "getblock <hash> [verbose=true] [showtxns=false] [ignoreNTP1=false]\n"
            "If verbose is false, returns a string that is serialized, hex-encoded data for block "
            "<hash>.\n"
            "If verbose is true, returns an Object with information about block <hash> .\n"
            "If verbose is true and showtxns is true, also returns Object about each transaction. Not "
            "ignoring NTP1 will try to retireve NTP1 data from the database. This won't work if the "
            "transaction is not in the blockchain.");

    std::string strHash = params[0].get_str();
    uint256     hash(strHash);

    const CTxDB txdb;

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    bool fShowTxns = false;
    if (params.size() > 2)
        fShowTxns = params[2].get_bool();

    const auto bi = txdb.ReadBlockIndex(hash);
    if (!bi)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    block.ReadFromDisk(&*bi, txdb, true);

    if (!fVerbose) {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    bool fIgnoreNTP1 = false;
    if (params.size() > 3)
        fIgnoreNTP1 = params[3].get_bool();

    return blockToJSON(block, &*bi, fShowTxns, fIgnoreNTP1);
}

Value getblockbynumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error("getblockbynumber <number> [txinfo] [ignoreNTP1=false]\n"
                            "txinfo optional to print more detailed tx info\n"
                            "Returns details of a block with given block-number. Not ignoring NTP1 will "
                            "try to retireve NTP1 data from the database. This won't work if the "
                            "transaction is not in the blockchain.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > CTxDB().GetBestChainHeight().value_or(0))
        throw runtime_error("Block number out of range.");

    const CTxDB txdb;

    CBlock block;

    boost::optional<CBlockIndex> pblockindex = CBlock::FindBlockByHeight(nHeight);

    if (!pblockindex) {
        throw runtime_error(fmt::format("Failed to get block at height: {}", nHeight));
    }
    block.ReadFromDisk(&*pblockindex, txdb, true);

    bool fIgnoreNTP1 = false;
    if (params.size() > 2)
        fIgnoreNTP1 = params[2].get_bool();

    return blockToJSON(block, &*pblockindex, params.size() > 1 ? params[1].get_bool() : false,
                       fIgnoreNTP1);
}

Value exportblockchain(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2) {
        throw runtime_error(
            "exportblockchain <path-dir> [breadth or depth]\n"
            "Exports the blockchain bootstrap.dat file to <path-dir>.\n"
            "<path-dir> must be a directory that exists. Ignoring the last parameter "
            "will export a linear version of the blockchain. If you need orphan chains, "
            "you can choose whether traversal is going to be breadth-firsth or depth-first.");
    }
    if (params.size() == 2 && params[1].get_str() != "breadth" && params[1].get_str() != "depth") {
        throw runtime_error("The second parameter can only be depth or breadth");
    }

    boost::optional<GraphTraverseType> graphTraverseType;
    if (params.size() == 2) {
        if (params[1].get_str() == "breadth") {
            graphTraverseType = GraphTraverseType::BreadthFirst;
        } else if (params[1].get_str() == "depth") {
            graphTraverseType = GraphTraverseType::DepthFirst;
        } else {
            graphTraverseType.reset();
        }
    }

    boost::filesystem::path bdir(params[0].get_str());
    if (!boost::filesystem::exists(bdir))
        throw runtime_error("Directory " + bdir.string() + " does not exist.");

    boost::filesystem::path filename = bdir / "bootstrap.dat";

    boost::promise<void>       finished;
    boost::unique_future<void> finished_future = finished.get_future();
    std::atomic<bool>          stopped{false};
    std::atomic<double>        progress{false};

    if (graphTraverseType) {
        // with orphans
        boost::thread exporterThread([&]() {
            ExportBootstrapBlockchainWithOrphans(filename, stopped, progress, finished,
                                                 graphTraverseType.value());
        });
        exporterThread.detach();
    } else {
        // without orphans
        boost::thread exporterThread(
            [&]() { ExportBootstrapBlockchain(filename, stopped, progress, finished); });
        exporterThread.detach();
    }

    NLog.write(b_sev::info, "Export blockchain to path started in another thread. Writing to path: {}",
               filename.string());

    int progVal            = 0;
    int lastPrintedProgVal = -1;
    while (!finished_future.is_ready()) {
        progVal = static_cast<int>(progress.load() * 100);
        if (progVal > lastPrintedProgVal) {
            NLog.write(b_sev::info, "Export blockchain progress: {}", progVal);
            lastPrintedProgVal = progVal;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (fShutdown) {
            stopped.store(true);
        }
    }

    finished_future.get(); // throws, but that's compatible with exception handle in this function

    NLog.write(b_sev::info, "Export blockchain to path {} is done.\n", filename.string());

    return Value();
}

Value waitforblockheight(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error("waitforblockheight <height> (timeout)\n"
                                 "\nWaits for (at least) block height and returns the height and hash\n"
                                 "of the current tip.\n"
                                 "\nReturns the current block on timeout or exit.\n"
                                 "\nArguments:\n"
                                 "1. height  (required, int) Block height to wait for (int)\n"
                                 "2. timeout (int, optional, default=0) Time in milliseconds to wait "
                                 "for a response. 0 indicates no timeout.\n"
                                 "\nResult:\n"
                                 "{                           (json object)\n"
                                 "  \"hash\" : {       (string) The blockhash\n"
                                 "  \"height\" : {     (int) Block height\n"
                                 "}\n"
                                 "\nExamples:\n"
                                 "waitforblockheight \"100\" 1000");
    int timeout = 0;

    int height = params[0].get_int();

    if (params.size() > 1 && params[1].type() != null_type) {
        timeout = params[1].get_int();
        NLog.write(b_sev::info, "Timeout set to: {}", timeout);
    }

    const auto bestHeight = []() -> int { return CTxDB().GetBestChainHeight().value_or(0); };

    int totalMilliSeconds = 0;
    while (totalMilliSeconds <= timeout && bestHeight() < height && IsRPCRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        totalMilliSeconds += 1000;
    }

    Object ret;
    ret.push_back(json_spirit::Pair("hash", CTxDB().GetBestBlockHash().GetHex()));
    ret.push_back(json_spirit::Pair("height", bestHeight()));

    return ret;
}

Value syncwithvalidationinterfacequeue(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0) {
        throw std::runtime_error("syncwithvalidationinterfacequeue (unimplemented)\n"
                                 "\nWaits for the validation interface queue to catch up on everything "
                                 "that was there when we entered this function.\n"
                                 "\nExamples:\n"
                                 "syncwithvalidationinterfacequeue");
    }
    //    SyncWithValidationInterfaceQueue();
    return Value();
}

Value getblockchaininfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getblockchaininfo\n"
            "Returns an object containing various state info regarding blockchain processing.\n"
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",              (string) current network name as defined in BIP70 "
            "(main, test, regtest)\n"
            "  \"blocks\": xxxxxx,             (numeric) the current number of blocks processed in the "
            "server\n"
            "  \"headers\": xxxxxx,            (numeric) the current number of headers we have "
            "validated\n"
            "  \"bestblockhash\": \"...\",       (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,         (numeric) the current difficulty\n"
            "  \"mediantime\": xxxxxx,         (numeric) median time for the current best block\n"
            "  \"initialblockdownload\": xxxx, (bool) (debug information) estimate of whether this node "
            "is in Initial Block Download mode.\n"
            "  \"chainwork\": \"xxxx\"           (string) total amount of work in active chain, in "
            "hexadecimal\n"
            "  \"size_on_disk\": xxxxxx,       (numeric) the estimated size of the block and undo files "
            "on disk\n"
            "  \"softforks\": [                (array) status of softforks in progress\n"
            "     {\n"
            "        \"id\": \"xxxx\",           (string) name of softfork\n"
            "        \"version\": xx,          (numeric) block version\n"
            "        \"reject\": {             (object) progress toward rejecting pre-softfork blocks\n"
            "           \"status\": xx,        (boolean) true if threshold reached\n"
            "        },\n"
            "     }, ...\n"
            "  ],\n"
            "  \"warnings\" : \"...\",           (string) any network and blockchain warnings.\n"
            "}\n"
            "\nExamples:\n"
            "getblockchaininfo");

    LOCK(cs_main);

    const CTxDB txdb;

    auto bestBlockIndex = txdb.GetBestBlockIndex();

    Object obj;
    obj.push_back(Pair("chain", Params().NetworkIDString()));
    obj.push_back(Pair("blocks", CTxDB().GetBestChainHeight().value_or(0)));
    obj.push_back(Pair("headers", bestBlockIndex ? bestBlockIndex->nHeight : -1));
    obj.push_back(Pair("bestblockhash", bestBlockIndex->blockHash.GetHex()));
    obj.push_back(Pair("difficulty", (double)GetDifficulty()));
    obj.push_back(Pair("mediantime", (int64_t)bestBlockIndex->GetMedianTimePast(txdb)));
    //    obj.push_back(
    //        Pair("verificationprogress", GuessVerificationProgress(Params().TxData(),
    //        chainActive.Tip())));
    obj.push_back(Pair("initialblockdownload", IsInitialBlockDownload(txdb)));
    obj.push_back(Pair("chainwork", bestBlockIndex->nChainTrust.GetHex()));
    obj.push_back(Pair("size_on_disk", (int64_t)CTxDB::GetCurrentDiskUsage()));
    obj.push_back(Pair("warnings", GetWarnings("statusbar")));
    return obj;
}

Value blockheaderToJSON(const CBlockIndex* blockindex)
{
    AssertLockHeld(cs_main);

    const CTxDB txdb;

    Object result;
    result.push_back(Pair("hash", blockindex->GetBlockHash().GetHex()));
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (blockindex->IsInMainChain(txdb))
        confirmations = txdb.GetBestChainHeight().value_or(0) - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", blockindex->nVersion));
    result.push_back(Pair("versionHex", fmt::format("{:08x}", blockindex->nVersion)));
    result.push_back(Pair("merkleroot", blockindex->hashMerkleRoot.GetHex()));
    result.push_back(Pair("time", (int64_t)blockindex->nTime));
    result.push_back(Pair("mediantime", (int64_t)blockindex->GetMedianTimePast(txdb)));
    result.push_back(Pair("nonce", (uint64_t)blockindex->nNonce));
    result.push_back(Pair("bits", fmt::format("{:08x}", blockindex->nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainTrust.GetHex()));
    //    result.push_back(Pair("nTx", (uint64_t)blockindex->nTx));

    if (auto o = blockindex->getPrev(txdb))
        result.push_back(Pair("previousblockhash", o->GetBlockHash().GetHex()));
    if (auto o = blockindex->getNext(txdb))
        result.push_back(Pair("nextblockhash", o->GetBlockHash().GetHex()));
    return result;
}

Value getblockheader(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            "getblockheader \"hash\" ( verbose )\n"
            "\nIf verbose is false, returns a string that is serialized, hex-encoded data for "
            "blockheader 'hash'.\n"
            "If verbose is true, returns an Object with information about blockheader <hash>.\n"
            "\nArguments:\n"
            "1. \"hash\"          (string, required) The block hash\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for "
            "the hex encoded data\n"
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is "
            "not on the main chain\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"versionHex\" : \"00000000\", (string) The block version formatted in hexadecimal\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 "
            "GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in seconds since epoch (Jan 1 "
            "1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainwork\" : \"0000...1f3\"     (string) Expected number of hashes required to "
            "produce the current chain (in hex)\n"
            "  \"nTx\" : n,             (numeric) The number of transactions in the block.\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\",      (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block "
            "'hash'.\n"
            "\nExamples:\n"
            "getblockheader 00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"");

    LOCK(cs_main);

    std::string strHash = params[0].get_str();
    uint256     hash(strHash);

    const CTxDB txdb;

    bool fVerbose = true;
    if (params.size() > 1 && params[1].type() != null_type) {
        fVerbose = params[1].get_bool();
    }

    const auto bi = txdb.ReadBlockIndex(hash);

    if (!bi)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    if (!fVerbose) {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << bi->GetBlockHeader();
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockheaderToJSON(&*bi);
}

Value gettxout(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw std::runtime_error(
            "gettxout \"txid\" n ( include_mempool )\n"
            "\nReturns details about an unspent transaction output.\n"
            "\nArguments:\n"
            "1. \"txid\"             (string, required) The transaction id\n"
            "2. \"n\"                (numeric, required) vout number\n"
            "3. \"include_mempool\"  (boolean, optional) Whether to include the mempool. Default: true."
            "     Note that an unspent output that is spent in the mempool won't appear.\n"
            "\nResult:\n"
            "{\n"
            "  \"bestblock\":  \"hash\",    (string) The hash of the block at the tip of the chain\n"
            "  \"confirmations\" : n,       (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,           (numeric) The transaction value in " +
            CURRENCY_UNIT +
            "\n"
            "  \"scriptPubKey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string) \n"
            "     \"hex\" : \"hex\",        (string) \n"
            "     \"reqSigs\" : n,          (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of neblio addresses\n"
            "        \"address\"     (string) neblio address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"coinbase\" : true|false   (boolean) Coinbase or not\n"
            "}\n"
            "\nExamples:\n"
            "\nGet unspent transactions\n"
            "listunspent\n"
            "View the details\n"
            "gettxout \"txid\" 1\n"
            "\nAs a json rpc call\n"
            "gettxout \"txid\" 1");

    LOCK(cs_main);

    json_spirit::Object ret;

    std::string strHash = params[0].get_str();
    uint256     hash(strHash);
    unsigned    n = static_cast<unsigned>(params[1].get_int());
    COutPoint   out(hash, n);
    bool        fMempool = true;
    if (params.size() > 2 && params[2].type() != Value_type::null_type)
        fMempool = params[2].get_bool();

    boost::optional<CTransaction> tx;
    uint32_t                      nHeight = 0;
    CTxIndex                      txindex;
    if (fMempool) {
        LOCKN(mempool.cs, mempoolLock);
        if (mempool.isSpent(out)) {
            return Value();
        }
        const CTransaction* txPtr = mempool.lookup_unsafe(out.hash);
        if (txPtr) {
            nHeight = MEMPOOL_HEIGHT;
            tx      = *txPtr;
        }
    }

    // if tx was not found in the mempool
    const CTxDB txdb;
    if (!tx) {
        if (!txdb.ReadTxIndex(out.hash, txindex)) {
            return Value();
        }

        if (n >= txindex.vSpent.size()) {
            throw std::runtime_error("Transaction " + out.hash.ToString() + " has only " +
                                     std::to_string(txindex.vSpent.size()) + " outputs. Output index " +
                                     std::to_string(n) + " is invalid");
        }

        if (!txindex.vSpent.at(n).IsNull()) {
            // it's already spent
            return Value();
        }
        auto bi = txdb.ReadBlockIndex(txindex.pos.nBlockPos);
        if (bi) {
            nHeight = bi->nHeight;
            tx      = CTransaction();
            if (!txdb.ReadTx(txindex.pos, *tx)) {
                return Value();
            }
        } else {
            return Value();
        }
    }

    if (!tx) {
        throw std::runtime_error("Failed to find tx " + out.hash.ToString());
    }

    if (n >= tx->vout.size()) {
        throw std::runtime_error("Transaction " + out.hash.ToString() + " has only " +
                                 std::to_string(tx->vout.size()) + " outputs. Output index " +
                                 std::to_string(n) + " is invalid");
    }

    const boost::optional<CBlockIndex> pindex = txdb.GetBestBlockIndex();
    ret.push_back(Pair("bestblock", pindex->GetBlockHash().GetHex()));
    if (nHeight == MEMPOOL_HEIGHT) {
        ret.push_back(Pair("confirmations", 0));
    } else {
        ret.push_back(Pair("confirmations", (int64_t)(pindex->nHeight - nHeight)));
    }
    ret.push_back(Pair("value", ValueFromAmount(tx->vout.at(n).nValue)));
    Object o;
    ScriptPubKeyToJSON(tx->vout.at(n).scriptPubKey, o, true);
    ret.push_back(Pair("scriptPubKey", o));
    ret.push_back(Pair("coinbase", tx->IsCoinBase()));
    ret.push_back(Pair("coinstake", tx->IsCoinStake()));

    return ret;
}

Value setviupushprobability(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
            "Only for regtest"
            "setviupushprobability <probability_numerator> <probability_denumerator>\n"
            "The probability is set as a fraction"
            "\nExamples:\n"
            "\nSet the probability to 1%\n"
            "setviupushprobability 1 100\n"
            "\nSet the probability to 5/1000 or 0.5%\n"
            "setviupushprobability 5 1000\n");

    if (Params().NetType() != NetworkType::Regtest) {
        throw std::runtime_error("This function is exclusively for regtest");
    }

    if (params[0].type() != Value_type::int_type || params[1].type() != Value_type::int_type) {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Parameters should be integers");
    }

    if (params[0].get_int() < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Numerator must be zero or positive");
    }

    if (params[1].get_int() <= 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Denominator must be positive");
    }

    VIUCachePushProbabilityNumerator   = params[0].get_int();
    VIUCachePushProbabilityDenominator = params[1].get_int();
  
    return Value();
}

Value listvotes(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error("listvotes \n"
                                 "\nReturns a list of the votes stored in this node.\n"
                                 "\nExamples:\n"
                                 "listvotes");

    return blockVotes.getAllVotesAsJson();
}

Value castvote(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw std::runtime_error(
            "castvote <start-block-height> <last-block-height> <proposal-ID> <vote-value>\n"
            "\nCasts a vote for a given proposal ID at a certain block range.\n"
            "\nExamples:\n"
            "\nCast a vote for proposal with ID 450, of value 66, starting from "
            "block 1000 to block 1100\n"
            "castvote 1000 1100 450 66\n");

    const int      startHeight = params[0].get_int();
    const int      lastHeight  = params[1].get_int();
    const uint32_t proposalID  = static_cast<uint32_t>(params[2].get_int());
    const uint32_t voteValue   = static_cast<uint32_t>(params[3].get_int());

    const Result<ProposalVote, ProposalVoteCreationError> voteResult =
        ProposalVote::CreateVote(startHeight, lastHeight, proposalID, voteValue);

    if (voteResult.isErr()) {
        throw std::runtime_error(
            ProposalVote::ProposalVoteCreationErrorAsString(voteResult.UNWRAP_ERR()));
    }
    const Result<void, AddVoteError> addVoteResult = blockVotes.addVote(voteResult.UNWRAP());
    if (addVoteResult.isErr()) {
        throw std::runtime_error(AllStoredVotes::AddVoteErrorAsString(addVoteResult.UNWRAP_ERR()));
    }
    blockVotes.writeAllVotesAsJsonToDataDir();
    return Value();
}

Value cancelallvotesofproposal(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error("cancelallvotesofproposal <proposal-ID>\n"
                                 "\nRemoves all the votes of a certain proposal ID (affects only blocks "
                                 "that are not staked yet).\n"
                                 "\nExamples:\n"
                                 "\nRemove all your votes that have the vote ID 123\n"
                                 "cancelallvotesofproposal 123\n");

    const uint32_t proposalID = static_cast<uint32_t>(params[0].get_int());

    blockVotes.removeAllVotesOfProposal(proposalID);
    blockVotes.writeAllVotesAsJsonToDataDir();
    return Value();
}

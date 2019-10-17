// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinrpc.h"
#include "main.h"
#include "merkletx.h"
#include "txmempool.h"
#include <atomic>

#include <algorithm>

using namespace json_spirit;
using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry,
                     bool ignoreNTP1 = false);
extern enum Checkpoints::CPMode CheckpointsMode;

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL) {
        if (pindexBest == NULL)
            return 1.0;
        else
            blockindex = GetLastBlockIndex(pindexBest.get(), false);
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff = (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

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
    if (boost::atomic_load(&pindexBest)->nHeight >= LAST_POW_BLOCK)
        return 0;

    int     nPoWInterval          = 72;
    int64_t nTargetSpacingWorkMin = 30, nTargetSpacingWork = 30;

    CBlockIndexSmartPtr pindex         = boost::atomic_load(&pindexGenesisBlock);
    CBlockIndexSmartPtr pindexPrevWork = boost::atomic_load(&pindexGenesisBlock);

    while (pindex) {
        if (pindex->IsProofOfWork()) {
            int64_t nActualSpacingWork = pindex->GetBlockTime() - pindexPrevWork->GetBlockTime();
            nTargetSpacingWork =
                ((nPoWInterval - 1) * nTargetSpacingWork + nActualSpacingWork + nActualSpacingWork) /
                (nPoWInterval + 1);
            nTargetSpacingWork = max(nTargetSpacingWork, nTargetSpacingWorkMin);
            pindexPrevWork     = pindex;
        }

        pindex = atomic_load(&pindex->pnext);
    }

    return GetDifficulty() * 4294.967296 / nTargetSpacingWork;
}

double GetPoSKernelPS()
{
    int    nPoSInterval          = 72;
    double dStakeKernelsTriedAvg = 0;
    int    nStakesHandled = 0, nStakesTime = 0;

    CBlockIndexSmartPtr pindex = pindexBest;

    CBlockIndexSmartPtr pindexPrevStake = nullptr;

    while (pindex && nStakesHandled < nPoSInterval) {
        if (pindex->IsProofOfStake()) {
            dStakeKernelsTriedAvg += GetDifficulty(pindex.get()) * 4294967296.0;
            nStakesTime += pindexPrevStake ? (pindexPrevStake->nTime - pindex->nTime) : 0;
            pindexPrevStake = pindex;
            nStakesHandled++;
        }

        pindex = pindex->pprev;
    }

    return nStakesTime ? dStakeKernelsTriedAvg / nStakesTime : 0;
}

Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail,
                   bool ignoreNTP1 = false)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);
    result.push_back(Pair("confirmations", (int)txGen.GetDepthInMainChain()));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    result.push_back(Pair("mint", ValueFromAmount(blockindex->nMint)));
    result.push_back(Pair("time", (int64_t)block.GetBlockTime()));
    result.push_back(Pair("nonce", (uint64_t)block.nNonce));
    result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("blocktrust", leftTrim(blockindex->GetBlockTrust().GetHex(), '0')));
    result.push_back(Pair("chaintrust", leftTrim(blockindex->nChainTrust.GetHex(), '0')));
    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    if (blockindex->pnext)
        result.push_back(Pair("nextblockhash", blockindex->pnext->GetBlockHash().GetHex()));

    result.push_back(Pair(
        "flags", strprintf("%s%s", blockindex->IsProofOfStake() ? "proof-of-stake" : "proof-of-work",
                           blockindex->GeneratedStakeModifier() ? " stake-modifier" : "")));
    result.push_back(Pair("proofhash", blockindex->hashProof.GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016" PRIx64, blockindex->nStakeModifier)));
    result.push_back(Pair("modifierchecksum", strprintf("%08x", blockindex->nStakeModifierChecksum)));

    Array txinfo;
    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
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

    return hashBestChain.GetHex();
}

Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getblockcount\n"
                            "Returns the number of blocks in the longest block chain.");

    return nBestHeight.load();
}

Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getdifficulty\n"
                            "Returns the difficulty as a multiple of the minimum difficulty.");

    Object obj;
    obj.push_back(Pair("proof-of-work", GetDifficulty()));
    obj.push_back(Pair("proof-of-stake", GetDifficulty(GetLastBlockIndex(pindexBest.get(), true))));
    obj.push_back(Pair("search-interval", (int)nLastCoinStakeSearchInterval));
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
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndexSmartPtr pblockindex = CBlock::FindBlockByHeight(nHeight);
    return pblockindex->phashBlock->GetHex();
}

// Experimentally deprecated in an effort to support the getblock() call electrum requires
// Value getblock(const Array& params, bool fHelp)
// {
//     if (fHelp || params.size() < 1 || params.size() > 2)
//         throw runtime_error(
//             "getblock <hash> [txinfo]\n"
//             "txinfo optional to print more detailed tx info\n"
//             "Returns details of a block with given block-hash.");

//     std::string strHash = params[0].get_str();
//     uint256 hash(strHash);

//     if (mapBlockIndex.count(hash) == 0)
//         throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

//     CBlock block;
//     CBlockIndex* pblockindex = mapBlockIndex[hash];
//     block.ReadFromDisk(pblockindex, true);

//     return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
// }

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

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    bool fShowTxns = false;
    if (params.size() > 2)
        fShowTxns = params[2].get_bool();

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock       block;
    CBlockIndex* pblockindex = boost::atomic_load(&mapBlockIndex[hash]).get();
    block.ReadFromDisk(pblockindex, true);

    if (!fVerbose) {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    bool fIgnoreNTP1 = false;
    if (params.size() > 3)
        fIgnoreNTP1 = params[3].get_bool();

    return blockToJSON(block, pblockindex, fShowTxns, fIgnoreNTP1);
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
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlock              block;
    CBlockIndexSmartPtr pblockindex = boost::atomic_load(&mapBlockIndex[hashBestChain]);
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;

    uint256 hash = *pblockindex->phashBlock;

    pblockindex = boost::atomic_load(&mapBlockIndex[hash]);
    block.ReadFromDisk(pblockindex.get(), true);

    bool fIgnoreNTP1 = false;
    if (params.size() > 2)
        fIgnoreNTP1 = params[2].get_bool();

    return blockToJSON(block, pblockindex.get(), params.size() > 1 ? params[1].get_bool() : false,
                       fIgnoreNTP1);
}

// ppcoin: get information of sync-checkpoint
Value getcheckpoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getcheckpoint\n"
                            "Show info of synchronized checkpoint.\n");

    Object       result;
    CBlockIndex* pindexCheckpoint;

    result.push_back(Pair("synccheckpoint", Checkpoints::hashSyncCheckpoint.ToString().c_str()));
    pindexCheckpoint = boost::atomic_load(&mapBlockIndex[Checkpoints::hashSyncCheckpoint]).get();
    result.push_back(Pair("height", pindexCheckpoint->nHeight));
    result.push_back(Pair("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str()));

    // Check that the block satisfies synchronized checkpoint
    if (CheckpointsMode == Checkpoints::STRICT)
        result.push_back(Pair("policy", "strict"));

    if (CheckpointsMode == Checkpoints::ADVISORY)
        result.push_back(Pair("policy", "advisory"));

    if (CheckpointsMode == Checkpoints::PERMISSIVE)
        result.push_back(Pair("policy", "permissive"));

    if (mapArgs.exists("-checkpointkey"))
        result.push_back(Pair("checkpointmaster", true));

    return result;
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

    if (graphTraverseType != boost::none) {
        // with orphans
        boost::thread exporterThread(
            boost::bind(&ExportBootstrapBlockchainWithOrphans, filename.string(), boost::ref(stopped),
                        boost::ref(progress), boost::ref(finished), graphTraverseType.value()));
        exporterThread.detach();
    } else {
        // without orphans
        boost::thread exporterThread(boost::bind(&ExportBootstrapBlockchain, filename.string(),
                                                 boost::ref(stopped), boost::ref(progress),
                                                 boost::ref(finished)));
        exporterThread.detach();
    }

    printf("Export blockchain to path started in another thread. Writing to path: %s\n",
           filename.string().c_str());

    int progVal            = 0;
    int lastPrintedProgVal = -1;
    while (!finished_future.is_ready()) {
        progVal = static_cast<int>(progress.load() * 100);
        if (progVal > lastPrintedProgVal) {
            printf("Export blockchain progress: %i%%\n", progVal);
            lastPrintedProgVal = progVal;
        }
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        if (fShutdown) {
            stopped.store(true);
        }
    }

    finished_future.get(); // throws, but that's compatible with exception handle in this function

    printf("Export blockchain to path %s is done.\n", filename.string().c_str());

    return Value();
}

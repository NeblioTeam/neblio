// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "NetworkForks.h"
#include "alert.h"
#include "block.h"
#include "blockindexlrucache.h"
#include "checkpoints.h"
#include "consensus.h"
#include "db.h"
#include "disktxpos.h"
#include "init.h"
#include "kernel.h"
#include "merkletx.h"
#include "messaging.h"
#include "net.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1/ntp1transaction.h"
#include "outpoint.h"
#include "txdb.h"
#include "txindex.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "wallet_interface.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>

using namespace std;
using namespace boost;

//
// Global state
//

CCriticalSection cs_main;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "Neblio Signed Message:\n";

using BlockTimeCacheType = BlockIndexLRUCache<int64_t>;

//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

static unsigned int GetNextTargetRequiredV1(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    const CTxDB txdb;

    CBigNum bnTargetLimit = fProofOfStake ? Params().PoSLimit() : Params().PoWLimit();

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const int prevBlockHeight = pindexLast->nHeight + 1;

    const CBlockIndex pindexPrev = GetLastBlockIndex(*pindexLast, fProofOfStake, txdb);
    if (pindexPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const boost::optional<CBlockIndex> indexPrevPrev = pindexPrev.getPrev(txdb);
    if (!indexPrevPrev) {
        NLog.write(b_sev::err,
                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex pindexPrevPrev = GetLastBlockIndex(*indexPrevPrev, fProofOfStake, txdb);
    if (pindexPrevPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev.GetBlockTime() - pindexPrevPrev.GetBlockTime();

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev.nBits);
    unsigned int nTS       = Params().TargetSpacing(prevBlockHeight);
    int64_t      nInterval = Params().TargetTimeSpan() / nTS;
    bnNew *= ((nInterval - 1) * nTS + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTS);

    if (bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

/**
 * Calculates the average spacing between blocks correctly, by sorting the times of the last X blocks,
 * and calculating the average from adjacent differences
 *
 * @brief CalculateActualBlockSpacingForV3
 * @param pindexLast
 * @return the average time spacing between blocks
 */
int64_t CalculateActualBlockSpacingForV3(const ITxDB& txdb, const CBlockIndex* pindexLast,
                                         BlockTimeCacheType& blockTimeCache)
{

    // get the latest blocks from the blocks. The amount of blocks is: TARGET_AVERAGE_BLOCK_COUNT
    int64_t forkBlock =
        Params().GetNetForks().getFirstBlockOfFork(NetworkFork::NETFORK__4_RETARGET_CORRECTION);
    // we start counting block times from the fork
    int64_t numOfBlocksToAverage = pindexLast->nHeight - (forkBlock + 1);
    // minimum number of blocks to calculate a difference is 2, and max is TARGET_AVERAGE_BLOCK_COUNT
    if (numOfBlocksToAverage <= 1) {
        numOfBlocksToAverage = 2;
    }
    if (numOfBlocksToAverage > TARGET_AVERAGE_BLOCK_COUNT) {
        numOfBlocksToAverage = TARGET_AVERAGE_BLOCK_COUNT;
    }

    // push block times to a vector
    std::vector<int64_t> blockTimes;
    std::vector<int64_t> blockTimeDifferences;
    blockTimes.reserve(numOfBlocksToAverage);
    blockTimeDifferences.reserve(numOfBlocksToAverage);
    uint256 currHash = pindexLast->GetBlockHash();
    blockTimeCache.manualAdd(*pindexLast);
    blockTimes.resize(numOfBlocksToAverage);
    for (int64_t i = 0; i < numOfBlocksToAverage; i++) {
        const boost::optional<BlockTimeCacheType::BICacheEntry> t = blockTimeCache.get(txdb, currHash);
        if (!t) {
            NLog.write(b_sev::critical, "CRITICAL ERROR: block not found while calculating target");
            break;
        }
        // fill the blocks in reverse order
        blockTimes.at(numOfBlocksToAverage - i - 1) = t->value;
        // move to the previous block
        if (t->prevHash != 0) {
            currHash = t->prevHash;
        } else {
            NLog.write(b_sev::critical,
                       "CRITICAL ERROR: prev block has zero hash even though it's not genesis. "
                       "THIS SHOULD NEVER HAPPEN. Database corrupt?");
        }
    }

    // sort block times to avoid negative values
    std::sort(blockTimes.begin(), blockTimes.end());
    // calculate adjacent differences
    std::adjacent_difference(blockTimes.cbegin(), blockTimes.cend(),
                             std::back_inserter(blockTimeDifferences));
    assert(blockTimeDifferences.size() >= 2);
    // calculate the average (n-1 because adjacent differences have size n-1)
    // begin()+1 because the first value is just a copy of the first value
    return std::accumulate(blockTimeDifferences.cbegin() + 1, blockTimeDifferences.cend(), 0) /
           (numOfBlocksToAverage - 1);
}

static unsigned int GetNextTargetRequiredV2(const ITxDB& txdb, const CBlockIndex* pindexLast,
                                            bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? Params().PoSLimit() : Params().PoWLimit();

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex pindexPrev = GetLastBlockIndex(*pindexLast, fProofOfStake, txdb);
    if (pindexPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const boost::optional<CBlockIndex> indexPrevPrev = pindexPrev.getPrev(txdb);
    if (!indexPrevPrev) {
        NLog.write(b_sev::err,
                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex pindexPrevPrev = GetLastBlockIndex(*indexPrevPrev, fProofOfStake, txdb);

    int64_t      nActualSpacing = pindexPrev.GetBlockTime() - pindexPrevPrev.GetBlockTime();
    unsigned int nTS            = Params().TargetSpacing(pindexLast->nHeight);
    if (nActualSpacing < 0)
        nActualSpacing = nTS;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev.nBits);
    int64_t nInterval = Params().TargetTimeSpan() / nTS;
    bnNew *= ((nInterval - 1) * nTS + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTS);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

static unsigned int GetNextTargetRequiredV3(const ITxDB& txdb, const CBlockIndex* pindexLast,
                                            bool fProofOfStake, BlockTimeCacheType& blockTimeCache)
{
    CBigNum bnTargetLimit = fProofOfStake ? Params().PoSLimit() : Params().PoWLimit();

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex pindexPrev = GetLastBlockIndex(*pindexLast, fProofOfStake, txdb);
    if (pindexPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const boost::optional<CBlockIndex> indexPrevPrev = pindexPrev.getPrev(txdb);
    if (!indexPrevPrev) {
        NLog.write(b_sev::err,
                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex pindexPrevPrev = GetLastBlockIndex(*indexPrevPrev, fProofOfStake, txdb);
    if (pindexPrevPrev.hashPrev == 0)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = CalculateActualBlockSpacingForV3(txdb, pindexLast, blockTimeCache);

    const unsigned int nTS = Params().TargetSpacing(pindexLast->nHeight);
    if (nActualSpacing < 0)
        nActualSpacing = nTS;

    /** if any of these assert fires, it means that you changed these parameteres.
     *  Be aware that the parameters k and l are fine tuned to produce a max shift in the difficulty in
     * the range [-3%,+5%]
     * This can be calculated with:
     * ((nInterval - l + k)*nTS + (m + l)*nActualSpacing)/((nInterval + k)*nTS + m*nActualSpacing),
     * with nActualSpacing being in the range [0,FutureDrift] = [0,600] If you change any of these
     * values, make sure you tune these variables again. A very high percentage on either side makes it
     * easier to change/manipulate the difficulty when mining
     */
    assert(FutureDrift(0) == 10 * 60);
    assert(Params().TargetSpacing(pindexLast->nHeight) == 30);
    assert(Params().TargetTimeSpan() == 2 * 60 * 60);

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum newTarget;
    newTarget.SetCompact(pindexPrev.nBits); // target from previous block
    int64_t nInterval = Params().TargetTimeSpan() / nTS;

    static constexpr const int k = 15;
    static constexpr const int l = 7;
    static constexpr const int m = 90;
    newTarget *= (nInterval - l + k) * nTS + (m + l) * nActualSpacing;
    newTarget /= (nInterval + k) * nTS + m * nActualSpacing;

    if (newTarget <= 0 || newTarget > bnTargetLimit)
        newTarget = bnTargetLimit;

    return newTarget.GetCompact();
}

unsigned int GetNextTargetRequired(const ITxDB& txdb, const CBlockIndex* pindexLast, bool fProofOfStake)
{
    if (!fProofOfStake && Params().MineBlocksOnDemand())
        return pindexLast->nBits;
    if (fProofOfStake && Params().MineBlocksOnDemand())
        return Params().PoWLimit().GetCompact();

    static thread_local typename BlockTimeCacheType::ExtractorFunc extractorFunc =
        [](const CBlockIndex& bi) { return bi.GetBlockTime(); };

    static thread_local BlockTimeCacheType blockIndexBlockTimeCache(500, extractorFunc);

    if (pindexLast->nHeight < 2000)
        return GetNextTargetRequiredV1(pindexLast, fProofOfStake);
    else if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__4_RETARGET_CORRECTION,
                                                    pindexLast->nHeight))
        return GetNextTargetRequiredV3(txdb, pindexLast, fProofOfStake, blockIndexBlockTimeCache);
    else
        return GetNextTargetRequiredV2(txdb, pindexLast, fProofOfStake);
}

// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    return std::max(cPeerBlockCounts.median(), Checkpoints::GetTotalBlocksEstimate());
}

bool CheckDiskSpace(std::uintmax_t nAdditionalBytes)
{
    std::uintmax_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes) {
        fShutdown         = true;
        string strMessage = _("Warning: Disk space is low!");
        strMiscWarning    = strMessage;
        NLog.write(b_sev::err, "*** {}", strMessage);
        uiInterface.ThreadSafeMessageBox(strMessage, "neblio",
                                         CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION |
                                             CClientUIInterface::MODAL);
        StartShutdown();
        return false;
    }
    return true;
}

bool LoadBlockIndex(bool fAllowNew)
{
    LOCK(cs_main);

    //
    // Load block index
    //
    {
        CTxDB txdb;
        txdb.resyncIfNecessary();
        if (!txdb.LoadBlockIndex())
            return false;

        //
        // Init with genesis block
        //
        if (!txdb.GetBestBlockIndex()) {
            if (!fAllowNew)
                return false;

            CBlock genesisBlock = Params().GenesisBlock();

            //// debug print
            genesisBlock.print();

            NLog.write(b_sev::info, "block.GetHash() == {}", genesisBlock.GetHash().ToString());
            NLog.write(b_sev::info, "block.hashMerkleRoot == {}",
                       genesisBlock.hashMerkleRoot.ToString());
            NLog.write(b_sev::info, "block.nTime = {}", genesisBlock.nTime);
            NLog.write(b_sev::info, "block.nNonce = {}", genesisBlock.nNonce);

            assert(genesisBlock.hashMerkleRoot == Params().GenesisBlock().hashMerkleRoot);
            assert(genesisBlock.GetHash() == Params().GenesisBlockHash());
            assert(genesisBlock.CheckBlock(0));

            // Start new block file
            if (!genesisBlock.WriteToDisk(boost::none, genesisBlock.GetHash(), genesisBlock.GetHash()))
                return NLog.error("LoadBlockIndex() : writing genesis block to disk failed");
        }
    }

    return true;
}

void PrintBlockTree()
{
    AssertLockHeld(cs_main);

    const CTxDB txdb;

    const auto blockIndexMap = txdb.ReadAllBlockIndexEntries();

    if (!blockIndexMap) {
        NLog.write(b_sev::err, "Failed to read block index from database");
        return;
    }

    // pre-compute tree structure
    map<uint256, vector<boost::optional<CBlockIndex>>> mapNext;
    for (BlockIndexMapType::MapType::const_iterator mi = blockIndexMap->cbegin();
         mi != blockIndexMap->cend(); ++mi) {
        const boost::optional<CBlockIndex> pindex = mi->second;
        mapNext[pindex->GetBlockHash()].push_back(pindex);
        // test
        // while (rand() % 3 == 0)
        //    mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, boost::optional<CBlockIndex>>> vStack;
    vStack.push_back(make_pair(0, *pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty()) {
        int                          nCol   = vStack.back().first;
        boost::optional<CBlockIndex> pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        std::stringstream b;
        if (nCol > nPrevCol) {
            for (int i = 0; i < nCol - 1; i++)
                b << "| ";
            b << "|";
        } else if (nCol < nPrevCol) {
            for (int i = 0; i < nCol; i++)
                b << "| ";
            b << "|";
        }
        NLog.write(b_sev::info, b.str());
        b.str("");

        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            b << "| ";
        NLog.write(b_sev::info, "{}", b.str());

        // print item
        CBlock block;
        block.ReadFromDisk(&*pindex, txdb);
        NLog.write(b_sev::info, "{} ({}) {}  {:08x}  {}  tx {}", pindex->nHeight,
                   pindex->GetBlockHash().ToString(), block.GetHash().ToString(), block.nBits,
                   DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()), block.vtx.size());

        PrintWallets(block);

        // put the main time-chain first
        vector<boost::optional<CBlockIndex>>& vNext = mapNext[pindex->GetBlockHash()];
        for (unsigned int i = 0; i < vNext.size(); i++) {
            if (vNext[i]->getNext(txdb)) {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol + i, vNext[i]));
    }
}

void ReadMoreBuffer(CDataStream& dataQueue, std::vector<char>& fileBuffer,
                    boost::filesystem::ifstream& fileIn)
{
    assert(!fileBuffer.empty());
    fileIn.read(&fileBuffer.front(), fileBuffer.size());
    const std::streamsize sizeRead = fileIn.gcount();
    dataQueue.insert(dataQueue.end(), fileBuffer.begin(), fileBuffer.begin() + sizeRead);
}

bool LoadExternalBlockFile(boost::filesystem::ifstream& fileIn)
{
    const int64_t nStart = GetTimeMillis() / 1000;

    int            nLoaded = 0;
    std::streampos nPos    = 0;

    static const std::size_t CHUNK_SIZE = 1 << 25; // 32 MB

    try {
        CDataStream       serializedBlockData(SER_DISK, CLIENT_VERSION);
        std::vector<char> fileBuffer(CHUNK_SIZE);
        while (fileIn.good() && !fRequestShutdown && !fShutdown) {
            if (serializedBlockData.size() < 2 * sizeof(uint32_t)) {
                ReadMoreBuffer(serializedBlockData, fileBuffer, fileIn);
            }

            uint32_t expectedBlockSize;
            uint32_t tmpMessageStart;

            static_assert(CMessageHeader::MESSAGE_START_SIZE == sizeof(tmpMessageStart), "Wrong sizes");

            serializedBlockData >> tmpMessageStart;
            serializedBlockData >> expectedBlockSize;

            CMessageHeader::MessageStartChars pchMessageStart;
            std::memcpy(pchMessageStart, &tmpMessageStart, sizeof(tmpMessageStart));
            if (std::memcmp(pchMessageStart, Params().MessageStart(),
                            CMessageHeader::MESSAGE_START_SIZE) != 0) {
                throw std::runtime_error("Invalid magic bytes; block import failed.");
            }

            if (serializedBlockData.size() < expectedBlockSize) {
                ReadMoreBuffer(serializedBlockData, fileBuffer, fileIn);
            }

            if (serializedBlockData.size() >= expectedBlockSize) {
                CBlock block;
                serializedBlockData >> block;
                NLog.write(b_sev::info, "Reading block at file pos: {}", nPos);

                LOCK(cs_main);

                if (ProcessBlock(nullptr, &block)) {
                    nLoaded++;
                    nPos += 2 * sizeof(uint32_t) + expectedBlockSize;
                }
            } else {
                throw std::runtime_error("Bootstrap file buffer ended unexpectedly");
            }
        }
    } catch (const std::exception& e) {
        NLog.write(b_sev::err, "Deserialize or I/O error caught during load: {}", e.what());
    }

    NLog.write(b_sev::info, "Loaded {} blocks from external file in {} seconds", nLoaded,
               GetTimeMillis() / 1000 - nStart);
    return nLoaded > 0;
}

struct CImportingNow
{
    CImportingNow()
    {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow()
    {
        assert(fImporting == true);
        fImporting = false;
    }
};

void ThreadImport(const std::vector<boost::filesystem::path> vFiles)
{
    RenameThread("bitcoin-loadblk");

    CImportingNow imp;
    vnThreadsRunning[THREAD_IMPORT]++;

    // -loadblock=
    // uiInterface.InitMessage(_("Starting block import..."));
    for (const boost::filesystem::path& path : vFiles) {
        boost::filesystem::ifstream filestream(path, std::ios::binary);
        if (filestream.good())
            LoadExternalBlockFile(filestream);
    }

    // hardcoded $DATADIR/bootstrap.dat
    filesystem::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (filesystem::exists(pathBootstrap)) {
        // uiInterface.InitMessage(_("Importing bootstrap blockchain data file."));

        boost::filesystem::ifstream filestream(pathBootstrap, std::ios::binary);
        if (filestream.good()) {
            filesystem::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LoadExternalBlockFile(filestream);
            RenameOver(pathBootstrap, pathBootstrapOld);
        }
    }

    vnThreadsRunning[THREAD_IMPORT]--;
}

string GetWarnings(string strFor)
{
    int    nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (GetBoolArg("-testsafemode"))
        strRPC = "test";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "") {
        nPriority    = 1000;
        strStatusBar = strMiscWarning;
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        for (PAIRTYPE(const uint256, CAlert) & item : mapAlerts) {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority) {
                nPriority    = alert.nPriority;
                strStatusBar = alert.strStatusBar;
                if (nPriority > 1000)
                    strRPC = strStatusBar;
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
    if (fDebug)
        NLog.write(b_sev::debug, "ProcessMessages({} messages)", pfrom->vRecvMsg.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        if (fDebug)
            NLog.write(b_sev::debug, "ProcessMessages(message {} msgsz, {} bytes, complete:{})",
                       msg.hdr.nMessageSize, msg.vRecv.size(), msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, Params().MessageStart(),
                   CMessageHeader::MESSAGE_START_SIZE) != 0) {
            NLog.write(b_sev::err, "PROCESSMESSAGE: INVALID MESSAGESTART");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid(Params().MessageStart())) {
            NLog.write(b_sev::err, "PROCESSMESSAGE: ERRORS IN HEADER {}", hdr.GetCommand());
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv     = msg.vRecv;
        uint256      hash      = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum) {
            NLog.write(
                b_sev::err,
                "ProcessMessages({}, {} bytes) : CHECKSUM ERROR nChecksum={:08x} hdr.nChecksum={:08x}",
                strCommand, nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try {
            {
                LOCK(cs_main);
                fRet = ProcessMessage(pfrom, strCommand, vRecv);
            }
            if (fShutdown)
                break;
        } catch (std::ios_base::failure& e) {
            pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED,
                               std::string("error parsing message"));
            if (strstr(e.what(), "end of data")) {
                // Allow exceptions from under-length message on vRecv
                NLog.write(b_sev::err,
                           "ProcessMessages({}, {} bytes) : Exception '{}' caught, normally caused by a "
                           "message being shorter than its stated length",
                           strCommand, nMessageSize, e.what());
            } else if (strstr(e.what(), "size too large")) {
                // Allow exceptions from over-long size
                NLog.write(b_sev::err, "ProcessMessages({}, {} bytes) : Exception '{}' caught",
                           strCommand, nMessageSize, e.what());
            } else {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        } catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(nullptr, "ProcessMessages()");
        }

        if (!fRet)
            NLog.write(b_sev::err, "ProcessMessage({}, {} bytes) FAILED", strCommand, nMessageSize);
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}

bool SendMessages(CNode* pto, bool fSendTrickle)
{
    TRY_LOCK(cs_main, lockMain);
    if (lockMain) {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        // Keep-alive ping. We send a nonce of zero because we don't use it anywhere
        // right now.
        if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSendMsg.empty()) {
            uint64_t nonce = 0;
            if (pto->nVersion > BIP0031_VERSION)
                pto->PushMessage("ping", nonce);
            else
                pto->PushMessage("ping");
        }

        // Resend wallet transactions that haven't gotten in a block yet
        ResendWalletTransactions();

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload(CTxDB()) && (GetTime() - nLastRebroadcast > 24 * 60 * 60)) {
            {
                LOCK(cs_vNodes);
                for (CNode* pnode : vNodes) {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    if (nLastRebroadcast)
                        pnode->setAddrKnown.clear();

                    // Rebroadcast our address
                    if (!fNoListen) {
                        CAddress addr = GetLocalAddress(&pnode->addr);
                        if (addr.IsRoutable())
                            pnode->PushAddress(addr);
                    }
                }
            }
            nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle) {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            for (const CAddress& addr : pto->vAddrToSend) {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second) {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000) {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }

        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            for (const CInv& inv : pto->vInventoryToSend) {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle) {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint256 hashRand  = inv.hash ^ hashSalt;
                    hashRand          = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    // always trickle our own transactions
                    if (!fTrickleWait) {
                        CWalletTx wtx;
                        if (GetTransaction(inv.hash, wtx))
                            if (wtx.fFromMe)
                                fTrickleWait = true;
                    }

                    if (fTrickleWait) {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second) {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000) {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);

        //
        // Message: getdata
        //
        vector<CInv> vGetData;
        int64_t      nNow = GetTime() * 1000000;
        const CTxDB  txdb;
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow) {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(txdb, inv)) {
                if (fDebugNet)
                    NLog.write(b_sev::debug, "sending getdata: {}", inv.ToString());
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000) {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
                mapAlreadyAskedFor.set(inv, nNow);
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);
    }
    return true;
}

// Amount compression:
// * If the amount is 0, output 0
// * first, divide the amount (in base units) by the largest power of 10 possible; call the exponent e (e
// is max 9)
// * if e<9, the last digit of the resulting number cannot be 0; store it as d, and drop it (divide by
// 10)
//   * call the result n
//   * output 1 + 10*(9*n + d - 1) + e
// * if e==9, we only know the resulting number is not zero, so output 1 + 10*(n - 1) + 9
// (this is decodable, as d is in [1-9] and e is in [0-9])

uint64_t CTxOutCompressor::CompressAmount(uint64_t n)
{
    if (n == 0)
        return 0;
    int e = 0;
    while (((n % 10) == 0) && e < 9) {
        n /= 10;
        e++;
    }
    if (e < 9) {
        int d = (n % 10);
        assert(d >= 1 && d <= 9);
        n /= 10;
        return 1 + (n * 9 + d - 1) * 10 + e;
    } else {
        return 1 + (n - 1) * 10 + 9;
    }
}

uint64_t CTxOutCompressor::DecompressAmount(uint64_t x)
{
    // x = 0  OR  x = 1+10*(9*n + d - 1) + e  OR  x = 1+10*(n - 1) + 9
    if (x == 0)
        return 0;
    x--;
    // x = 10*(9*n + d - 1) + e
    int e = x % 10;
    x /= 10;
    uint64_t n = 0;
    if (e < 9) {
        // x = 9*n + d - 1
        int d = (x % 9) + 1;
        x /= 9;
        // x = n
        n = x * 10 + d;
    } else {
        n = x + 1;
    }
    while (e) {
        n *= 10;
        e--;
    }
    return n;
}

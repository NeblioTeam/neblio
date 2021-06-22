// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/optional.hpp>
#include <boost/scope_exit.hpp>
#include <boost/thread/future.hpp>
#include <boost/version.hpp>
#include <future>
#include <random>

#include "blockmetadata.h"
#include "globals.h"
#include "kernel.h"
#include "main.h"
#include "stringmanip.h"
#include "txdb.h"
#include "util.h"

#include "SerializationTester.h"

using namespace std;
using namespace boost;

boost::filesystem::path CTxDB::DB_DIR                         = "txlmdb";
bool                    CTxDB::QuickSyncHigherControl_Enabled = true;

// this is just an arbitrary value for stake seen, don't change it without changing the DATABASE_VERSION
// variable
static const std::string STAKESEEN_VALUE = "f";

bool IsQuickSyncOSCompatible(const std::string& osValue)
{
    if (osValue == "any") {
        return true;
    } else {
        return false;
    }
}

void DownloadQuickSyncFile(const json_spirit::Value& fileVal, const filesystem::path& dbdir)
{
    // get json fields of this file
    const json_spirit::Array urlsObj = NTP1Tools::GetArrayField(fileVal.get_obj(), "url");
    const std::string        sum     = NTP1Tools::GetStrField(fileVal.get_obj(), "sha256sum");
    const std::string        sumBin  = boost::algorithm::unhex(sum);

    if (urlsObj.empty()) {
        std::string jsonData = json_spirit::write(fileVal);
        throw std::runtime_error("Empty list of urls retrieved: " + jsonData);
    }

    std::vector<std::string> urls;
    for (auto urlObj : urlsObj) {
        urls.push_back(urlObj.get_str());
    }

    // shuffle the urls to pick a random one of them first
    auto rng = std::default_random_engine{};
    std::shuffle(urls.begin(), urls.end(), rng);

    // Diskspace check disabled as it doesn't deliver reliable results for large files
    std::uintmax_t fileSize = static_cast<uint64_t>(NTP1Tools::GetInt64Field(fileVal.get_obj(), "size"));
    // check available diskspace
    std::uintmax_t availableSpace = GetFreeDiskSpace(dbdir);
    std::uintmax_t requiredSpace  = static_cast<std::size_t>(static_cast<double>(fileSize) * 1.2);
    if (requiredSpace > availableSpace) {
        throw std::runtime_error("Diskspace insufficient to download the blockchain; Available: " +
                                 std::to_string(availableSpace / ONE_MB) +
                                 " MB; required: " + std::to_string(requiredSpace / ONE_MB) + "MB");
    }

    std::string      leaf               = filesystem::path(urls.at(0)).filename().string();
    std::string      tempLeaf           = leaf + ".temp";
    filesystem::path downloadTarget     = dbdir / leaf;
    filesystem::path downloadTempTarget = dbdir / tempLeaf;

    // delete files that already exist before downloading them
    {
        boost::system::error_code remove_error;
        if (filesystem::exists(downloadTarget) && !filesystem::remove(downloadTarget, remove_error)) {
            throw std::runtime_error(
                "File " + leaf + " already exists and could not be deleted: " + remove_error.message());
        }
        if (filesystem::exists(downloadTempTarget) &&
            !filesystem::remove(downloadTempTarget, remove_error)) {
            throw std::runtime_error("File " + tempLeaf + " already exists and could not be deleted: " +
                                     remove_error.message());
        }
    }

    std::atomic<float> progress;
    progress.store(0);

    // ensure that all leaf file names are the same in the retrieved json data, this shows if the
    // json data has a problem
    for (const std::string& url : urls) {
        if (leaf != filesystem::path(url).filename().string()) {
            throw std::runtime_error(
                "The URLs in the following json snippet do not all have the same file names: " +
                json_spirit::write(fileVal));
        }
    }

    // download the file asynchronously in a new thread
    std::promise<void> downloadThreadPromise;
    std::future<void>  downloadThreadFuture = downloadThreadPromise.get_future();
    std::thread        downloadThread([&downloadThreadPromise, &urls, &downloadTempTarget, &progress]() {
        for (unsigned i = 0; i < urls.size(); i++) {
            try {
                NLog.write(b_sev::info, "Downloading file for QuickSync: {}...", urls[i]);
                static const long connectionTimeout = 300;
                cURLTools::GetLargeFileFromHTTPS(urls[i], connectionTimeout, downloadTempTarget,
                                                 progress, std::set<CURLcode>({CURLE_PARTIAL_FILE}));
                NLog.write(b_sev::debug, "Setting promise value for downloaded file: {}...", urls[i]);
                downloadThreadPromise.set_value();
                NLog.write(b_sev::info, "Done setting promise value for downloaded file: {}...",
                           urls[i]);
                break; // break if a file is downloaded successfully
            } catch (std::exception& ex) {
                // if this is the last file, set the exception and fail
                NLog.write(b_sev::err, "Failed to download a file {}. The last error is: {}", urls[i],
                           ex.what());
                if (i + 1 >= urls.size()) {
                    downloadThreadPromise.set_exception(std::make_exception_ptr(std::runtime_error(
                        "Failed to download any of the available files. The last error is: " +
                        std::string(ex.what()))));
                }
            }
        }
    });

    do {
        std::stringstream ss;
        ss.setf(std::ios::fixed);
        ss << "Downloading QuickSync file " << leaf << ": " << std::setprecision(2)
           << progress.load(std::memory_order_relaxed) << " MB...";
        uiInterface.InitMessage(ss.str());
    } while (downloadThreadFuture.wait_for(std::chrono::milliseconds(250)) != std::future_status::ready);
    downloadThread.join();
    downloadThreadFuture.get();

    uiInterface.InitMessage("Calculating hash to verify integrity...");
    NLog.write(b_sev::info, "Done downloading {}", leaf);
    std::string calculatedHash = CalculateHashOfFile<Sha256Calculator>(downloadTempTarget);
    if (calculatedHash != sumBin) {
        throw std::runtime_error("The calculated checksum for the downloaded file: " +
                                 downloadTempTarget.string() + "; does not match the expected one.");
    }

    {
        boost::system::error_code rename_ec;
        filesystem::rename(downloadTempTarget, downloadTarget, rename_ec);
        if (rename_ec) {
            throw std::runtime_error("Error when trying to rename the temporary file " + tempLeaf +
                                     " to " + leaf + ". Error: " + rename_ec.message());
        }
    }

    uiInterface.InitMessage("Download and verification of " + leaf + " is done.");
}

void DoQuickSync(const filesystem::path& dbdir)
{
    unsigned         failedAttempts      = 0;
    static const int MAX_FAILED_ATTEMPTS = 10;

    bool success = false;

    while (failedAttempts < MAX_FAILED_ATTEMPTS) {
        {
            std::string msg = "Attempting quicksync... (attempt " + std::to_string(failedAttempts + 1) +
                              " out of " + std::to_string(MAX_FAILED_ATTEMPTS) + ")";
            uiInterface.InitMessage(msg);
            NLog.write(b_sev::info, "{}", msg);
        }
        try {
            filesystem::remove_all(dbdir);
            filesystem::create_directories(dbdir);

            std::string        jsonStrData = cURLTools::GetFileFromHTTPS(QuickSyncDataLink, 30, false);
            json_spirit::Value parsedJsonData;
            json_spirit::read_or_throw(jsonStrData, parsedJsonData);
            json_spirit::Array rootArray = parsedJsonData.get_array();
            for (const json_spirit::Value& val : rootArray) {
                std::string        os        = NTP1Tools::GetStrField(val.get_obj(), "os");
                uint64_t           dbversion = NTP1Tools::GetUint64Field(val.get_obj(), "dbversion");
                json_spirit::Array files     = NTP1Tools::GetArrayField(val.get_obj(), "files");

                if (dbversion != DATABASE_VERSION) {
                    NLog.write(b_sev::debug, "Skipping database with version {}", dbversion);
                    continue;
                }

                if (!IsQuickSyncOSCompatible(os)) {
                    NLog.write(b_sev::debug, "Skipping database with OS {}", dbversion);
                    continue;
                }
                for (const json_spirit::Value& fileVal : files) {
                    DownloadQuickSyncFile(fileVal, dbdir);
                }
                success = true;
                break; // after downloading one set of files, stop
            }
            break; // download is done, exit the "failedAttempts" counter
        } catch (std::exception& ex) {
            static const int WAIT_TIME_SECONDS = 5;
            std::string      msg               = "Quick sync failed... ";
            failedAttempts++;
            if (failedAttempts < MAX_FAILED_ATTEMPTS) {
                msg += "retrying in " + std::to_string(WAIT_TIME_SECONDS) + " seconds...";
            }
            uiInterface.InitMessage(msg);
            NLog.write(b_sev::err, "Quick sync failed (attempt {} of {}). Error: {}", failedAttempts,
                       MAX_FAILED_ATTEMPTS, ex.what());
            std::this_thread::sleep_for(std::chrono::seconds(WAIT_TIME_SECONDS));
        }
    }
    uiInterface.InitMessage("QuickSync done");
    if (!success) {
        throw std::runtime_error("QuickSync error: None of the files matched the correct settings or "
                                 "another error occurred.");
    }
    NLog.write(b_sev::info, "QuickSync done");
}

bool ShouldQuickSyncBeDone(const filesystem::path& dbdir)
{
    if (CTxDB::QuickSyncHigherControl_Enabled == false) {
        return false;
    }

    if (GetBoolArg("-noquicksync") == true) {
        return false;
    }

    return (!filesystem::exists(dbdir) || !filesystem::exists(dbdir / "data.mdb") ||
            !filesystem::exists(dbdir / "lock.mdb")) &&
           Params().NetType() == NetworkType::Mainnet;
}

void CTxDB::resyncIfNecessary(bool forceClearDB)
{

    nVersion = ReadVersion().value_or(0);
    NLog.write(b_sev::info, "Transaction index version is {}", nVersion);

    if (nVersion != DATABASE_VERSION) {
        NLog.write(b_sev::warn, "Required index version is {}, removing old database", DATABASE_VERSION);

        forceClearDB = true;
    }

    NLog.write(b_sev::info, "Opened LMDB successfully");

    // check if the database has to be wiped
    if (forceClearDB ||
        SC_CheckOperationOnRestartScheduleThenDeleteIt(SC_SCHEDULE_ON_RESTART_OPNAME__RESYNC)) {

        db->clearDBData();

        // after a resync, always rescan the wallet
        SC_CreateScheduledOperationOnRestart(SC_SCHEDULE_ON_RESTART_OPNAME__RESCAN);
    }

    // run serialization tests to ensure that no binary interpretation platforms will arise
    try {
        RunCrossPlatformSerializationTests();
        NLog.write(b_sev::info, "Binary format tests have passed.");
    } catch (std::exception& ex) {
        NLog.write(b_sev::err, "Binary format tests have failed: {}", ex.what());
    }

    // at this point, there's no database, so we attempt quicksync
    if (const auto dbdir = db->getDataDir()) {
        // if the directory doesn't exist, use quicksync
        if (ShouldQuickSyncBeDone(*dbdir)) {
            // close the database before running quicksync
            this->Close();

            try {
                // binary layout compatibility is necessary for quicksync to work
                RunCrossPlatformSerializationTests();
                NLog.write(b_sev::info, "Binary format tests have passed.");
                DoQuickSync(*dbdir);

                // after quicksync, a rescan has to be done
                SC_CreateScheduledOperationOnRestart(SC_SCHEDULE_ON_RESTART_OPNAME__RESCAN);
            } catch (std::exception& ex) {
                NLog.write(b_sev::err,
                           "Quicksync exited with an exception (this is not expected to happen): {}",
                           ex.what());
                db->clearDBData();
            }
        }
    }

    db->openDB(false);

    WriteVersion(DATABASE_VERSION); // Save db schema version

    // ensure the correct version is now there
    nVersion = ReadVersion().value_or(0);
    if (nVersion != DATABASE_VERSION) {
        throw std::runtime_error(
            "Failed to persist database schema version number after clearing the database.");
    }
}

// CDB subclasses are created and destroyed VERY OFTEN. That's why
// we shouldn't treat this as a free operations.
CTxDB::CTxDB()
{
    static boost::filesystem::path DBDir = GetDataDir() / DB_DIR;

    db = MakeUnique<LMDB>(&DBDir);
}

void CTxDB::Close() { db->close(); }

bool CTxDB::TxnBegin(size_t required_size) { return db->beginDBTransaction(required_size); }

bool CTxDB::TxnCommit() { return db->commitDBTransaction(); }

bool CTxDB::TxnAbort() { return db->abortDBTransaction(); }

boost::optional<int> CTxDB::ReadVersion()
{
    return Read(std::string("version"), nVersion, IDB::Index::DB_MAIN_INDEX)
               ? boost::make_optional(nVersion)
               : boost::none;
}

bool CTxDB::WriteVersion(int nVersionIn)
{
    return Write(std::string("version"), nVersionIn, IDB::Index::DB_MAIN_INDEX);
}

bool CTxDB::ReadTxIndex(const uint256& hash, CTxIndex& txindex) const
{
    txindex.SetNull();
    return Read(hash, txindex, IDB::Index::DB_TX_INDEX);
}

bool CTxDB::UpdateTxIndex(const uint256& hash, const CTxIndex& txindex)
{
    return Write(hash, txindex, IDB::Index::DB_TX_INDEX);
}

bool CTxDB::ReadTx(const CDiskTxPos& txPos, CTransaction& tx) const
{
    tx.SetNull();
    return Read(txPos.nBlockPos, tx, IDB::Index::DB_BLOCKS_INDEX, 0, txPos.nTxPos);
}

bool CTxDB::ReadNTP1Tx(const uint256& hash, NTP1Transaction& ntp1tx) const
{
    ntp1tx.setNull();
    return Read(hash, ntp1tx, IDB::Index::DB_NTP1TX_INDEX);
}

bool CTxDB::ReadNTP1TxsWithTokenSymbol(std::string tokenName, std::vector<uint256>& txs) const
{
    std::transform(tokenName.begin(), tokenName.end(), tokenName.begin(), ::toupper);
    return ReadMultiple(tokenName, txs, IDB::Index::DB_NTP1TOKENNAMES_INDEX);
}

bool CTxDB::WriteNTP1TxWithTokenSymbol(std::string tokenSymbol, const NTP1Transaction& ntp1tx)
{
    if (ntp1tx.isNull()) {
        NLog.write(b_sev::err,
                   "Attempted to store token symbol information of token with given symbol {}",
                   tokenSymbol);
        return false;
    }
    std::string symbol;
    try {
        symbol = ntp1tx.getTokenSymbolIfIssuance();
    } catch (std::exception& ex) {
        NLog.write(
            b_sev::err,
            "Failed to get token symbol for transaction: {}; with claimed token symbol {}. Error: {}",
            ntp1tx.getTxHash().ToString(), tokenSymbol, ex.what());
        return false;
    } catch (...) {
        NLog.write(
            b_sev::err,
            "Failed to get token symbol for transaction: {}; with claimed token symbol {}. Unknown "
            "error.",
            ntp1tx.getTxHash().ToString(), tokenSymbol);
        return false;
    }

    std::transform(tokenSymbol.begin(), tokenSymbol.end(), tokenSymbol.begin(), ::toupper);
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

    if (symbol != tokenSymbol) {
        NLog.write(b_sev::err,
                   "While writing NTP1 tx for token names, the token name provided is not equal to the "
                   "token name calculated: {} != {}",
                   symbol, tokenSymbol);
        return false;
    }
    return Write(tokenSymbol, ntp1tx.getTxHash(), IDB::Index::DB_NTP1TOKENNAMES_INDEX);
}

bool CTxDB::ReadAddressPubKey(const CBitcoinAddress& address, std::vector<uint8_t>& pubkey) const
{
    return Read(address, pubkey, IDB::Index::DB_ADDRSVSPUBKEYS_INDEX);
}

bool CTxDB::WriteAddressPubKey(const CBitcoinAddress& address, const std::vector<uint8_t>& pubkey)
{
    return Write(address, pubkey, IDB::Index::DB_ADDRSVSPUBKEYS_INDEX);
}

bool CTxDB::WriteNTP1Tx(const uint256& hash, const NTP1Transaction& ntp1tx)
{
    return Write(hash, ntp1tx, IDB::Index::DB_NTP1TX_INDEX);
}

bool CTxDB::ReadAllIssuanceTxs(std::vector<uint256>& txs) const
{
    // the key is empty because we want to get all keys in the database
    std::map<std::string, std::vector<uint256>> resMap;
    bool success = ReadMultipleWithKeys(resMap, IDB::Index::DB_NTP1TOKENNAMES_INDEX);
    if (!success) {
        return false;
    }
    txs.clear();
    for (auto&& p : resMap) {
        txs.insert(txs.end(), std::make_move_iterator(p.second.begin()),
                   std::make_move_iterator(p.second.end()));
    }
    return true;
}

bool CTxDB::ReadBlock(const uint256& hash, CBlock& blk, bool fReadTransactions) const
{
    blk.SetNull();
    int modifiers = (fReadTransactions ? 0 : SER_BLOCKHEADERONLY);
    return Read(hash, blk, IDB::Index::DB_BLOCKS_INDEX, modifiers);
}

bool CTxDB::WriteBlock(const uint256& hash, const CBlock& blk)
{
    return Write(hash, blk, IDB::Index::DB_BLOCKS_INDEX);
}

bool CTxDB::EraseTxIndex(const uint256& hash) { return Erase(hash, IDB::Index::DB_TX_INDEX); }

bool CTxDB::ContainsTx(const uint256& hash) const { return Exists(hash, IDB::Index::DB_TX_INDEX); }

bool CTxDB::ContainsNTP1Tx(const uint256& hash) const
{
    return Exists(hash, IDB::Index::DB_NTP1TX_INDEX);
}

bool CTxDB::ReadDiskTx(const uint256& hash, CTransaction& tx, CTxIndex& txindex) const
{
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos, *this));
}

bool CTxDB::ReadDiskTx(const uint256& hash, CTransaction& tx) const
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(const COutPoint& outpoint, CTransaction& tx, CTxIndex& txindex) const
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(const COutPoint& outpoint, CTransaction& tx) const
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

boost::optional<CBlockIndex> CTxDB::ReadBlockIndex(const uint256& blockHash) const
{
    CBlockIndex result;
    if (!Read(blockHash, result, IDB::Index::DB_BLOCKINDEX_INDEX)) {
        return boost::none;
    } else {
        return boost::make_optional(std::move(result));
    }
}

bool CTxDB::WriteBlockIndex(const CBlockIndex& blockindex)
{
    return Write(blockindex.GetBlockHash(), blockindex, IDB::Index::DB_BLOCKINDEX_INDEX);
}

bool CTxDB::EraseBlockHashOfHeight(int32_t height)
{
    return Erase(height, IDB::Index::DB_BLOCKHEIGHTS_INDEX);
}

boost::optional<uint256> CTxDB::ReadBlockHashOfHeight(int32_t height) const
{
    uint256 result = 0;
    if (Read(height, result, IDB::Index::DB_BLOCKHEIGHTS_INDEX)) {
        return boost::make_optional(std::move(result));
    } else {
        return boost::none;
    }
}

bool CTxDB::WriteBlockHashOfHeight(int32_t height, const uint256& blockHash)
{
    return Write(height, blockHash, IDB::Index::DB_BLOCKHEIGHTS_INDEX);
}

boost::optional<BlockMetadata> CTxDB::ReadBlockMetadata(const uint256& blockHash) const
{
    BlockMetadata blockMetadata(0, 0, 0);
    if (Read(blockHash, blockMetadata, IDB::Index::DB_BLOCKMETADATA_INDEX)) {
        return boost::make_optional(std::move(blockMetadata));
    } else {
        return boost::none;
    }
}

bool CTxDB::WriteBlockMetadata(const BlockMetadata& blockMetadata)
{
    return Write(blockMetadata.getBlockHash(), blockMetadata, IDB::Index::DB_BLOCKMETADATA_INDEX);
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain) const
{
    return Read(string("hashBestChain"), hashBestChain, IDB::Index::DB_MAIN_INDEX);
}

bool CTxDB::WriteHashBestChain(const uint256& hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain, IDB::Index::DB_MAIN_INDEX);
}

bool CTxDB::ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust) const
{
    return Read(string("bnBestInvalidTrust"), bnBestInvalidTrust, IDB::Index::DB_MAIN_INDEX);
}

bool CTxDB::WriteBestInvalidTrust(const CBigNum& bnBestInvalidTrust)
{
    return Write(string("bnBestInvalidTrust"), bnBestInvalidTrust, IDB::Index::DB_MAIN_INDEX);
}

boost::optional<std::map<uint256, CBlockIndex>> CTxDB::ReadAllBlockIndexEntries() const
{
    auto&& rawAll = db->readAllUnique(IDB::Index::DB_BLOCKINDEX_INDEX);
    if (!rawAll) {
        return boost::none;
    }

    std::map<uint256, CBlockIndex> result;
    while (!rawAll->empty()) {
        const std::pair<const std::string, const std::string> p = *rawAll->begin();

        // Unpack keys and values.
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(p.first.data(), p.first.size());
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.write(p.second.data(), p.second.size());

        uint256 blockHash;
        ssKey >> blockHash;

        CBlockIndex diskindex;
        ssValue >> diskindex;

        result[blockHash] = diskindex;

        // delete the current loaded entry from the map
        rawAll->erase(p.first);
    }
    return result;
}

bool CTxDB::WriteStakeSeen(const std::pair<COutPoint, unsigned int>& stake)
{
    return Write(stake, STAKESEEN_VALUE, IDB::Index::DB_STAKES_INDEX);
}

boost::optional<bool> CTxDB::WasStakeSeen(const std::pair<COutPoint, unsigned int>& stake) const
{
    std::string val;
    bool        readResult = Read(stake, val, IDB::Index::DB_STAKES_INDEX);
    if (!readResult) {
        // failed to read
        return boost::none;
    } else if (val == STAKESEEN_VALUE) {
        // success
        return boost::make_optional(true);
    } else {
        // read success, but the value is invalid... this should practically never happen
        NLog.write(b_sev::err, "CRITICAL: A stake-seen value was invalid. This should never happen");
        return boost::make_optional(false);
    }
}

std::string LmdbValToString(const MDB_val& val)
{
    return std::string((const char*)val.mv_data, val.mv_size);
}

bool CTxDB::LoadBlockIndex()
{
    // Load hashBestChain pointer to end of best chain
    uint256 hashBestChainTemp = 0;
    if (!ReadHashBestChain(hashBestChainTemp)) {
        if (pindexGenesisBlock == nullptr)
            return true;
        NLog.write(b_sev::err, "CTxDB::LoadBlockIndex() : hashBestChain not loaded");
        return false;
    }

    const boost::optional<CBlockIndex> genesisIndex = ReadBlockIndex(Params().GenesisBlockHash());
    if (!genesisIndex) {
        NLog.write(b_sev::err,
                   "CTxDB::LoadBlockIndex() : genesis block index not found in the database");
        return false;
    }
    pindexGenesisBlock = boost::make_shared<CBlockIndex>(*genesisIndex);

    const boost::optional<CBlockIndex> bestBlockIndex = ReadBlockIndex(hashBestChainTemp);
    if (!bestBlockIndex) {
        NLog.write(b_sev::err, "CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");
        return false;
    }

    const int bestHeight = bestBlockIndex->nHeight;

    NLog.write(b_sev::err, "LoadBlockIndex(): hashBestChain={}  height={}  trust={}  date={}",
               hashBestChainTemp.ToString().substr(0, 20), bestHeight,
               CBigNum(GetBestChainTrust().value_or(0)).ToString(),
               DateTimeStrFormat("%x %H:%M:%S", bestBlockIndex->GetBlockTime()));

    // Load bnBestInvalidTrust, OK if it doesn't exist
    CBigNum bnBestInvalidTrust;
    ReadBestInvalidTrust(bnBestInvalidTrust);
    nBestInvalidTrust = bnBestInvalidTrust.getuint256();

    if (!Checkpoints::ValidateCheckpointsInDB(*this)) {
        NLog.write(b_sev::err, "Checkpoint validation failed.");
        return false;
    }

    // Verify blocks in the best chain
    int nCheckLevel = GetArg("-checklevel", 1);
    int nCheckDepth = GetArg("-checkblocks", 2500);
    if (nCheckDepth == 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > bestHeight)
        nCheckDepth = bestHeight;
    NLog.write(b_sev::info, "Verifying last {} blocks at level {}", nCheckDepth, nCheckLevel);
    boost::optional<CBlockIndex>               pindexFork = boost::none;
    map<uint256, boost::optional<CBlockIndex>> mapBlockPos;
    uint64_t                                   loadedCount = 0;
    for (boost::optional<CBlockIndex> pindex      = ReadBlockIndex(hashBestChainTemp);
         pindex && pindex->getPrev(*this); pindex = pindex->getPrev(*this)) {

        if (loadedCount % 100 == 0) {
            uiInterface.InitMessage("Verifying latest blocks (" + std::to_string(loadedCount) + "/" +
                                    std::to_string(nCheckDepth) + ")");
            NLog.write(b_sev::info, "Done Verifying latest blocks {}/{}", loadedCount, nCheckDepth);
        }
        loadedCount++;

        if (fRequestShutdown || pindex->nHeight < bestHeight - nCheckDepth)
            break;
        CBlock block;
        if (!block.ReadFromDisk(&*pindex, *this)) {
            NLog.write(b_sev::err, "LoadBlockIndex() : block.ReadFromDisk failed");
            return false;
        }
        // check level 1: verify block validity
        // check level 7: verify block signature too
        if (nCheckLevel > 0 &&
            !block.CheckBlock(*this, block.GetHash(), true, true, (nCheckLevel > 6))) {
            NLog.write(b_sev::warn, "LoadBlockIndex() : *** found bad block at {}, hash={}",
                       pindex->nHeight, pindex->GetBlockHash().ToString());
            pindexFork = pindex->getPrev(*this);
        }
        // check level 2: verify transaction index validity
        if (nCheckLevel > 1) {
            uint256 pos      = pindex->GetBlockHash();
            mapBlockPos[pos] = pindex;
            for (const CTransaction& tx : block.vtx) {
                uint256  hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex)) {
                    // check level 3: checker transaction hashes
                    if (nCheckLevel > 2 || pindex->GetBlockHash() != txindex.pos.nBlockPos) {
                        // either an error or a duplicate transaction
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos, *this)) {
                            NLog.write(b_sev::warn,
                                       "LoadBlockIndex() : *** cannot read mislocated transaction {}",
                                       hashTx.ToString());
                            pindexFork = pindex->getPrev(*this);
                        } else if (txFound.GetHash() != hashTx) // not a duplicate tx
                        {
                            NLog.write(b_sev::warn, "LoadBlockIndex(): *** invalid tx position for {}",
                                       hashTx.ToString());
                            pindexFork = pindex->getPrev(*this);
                        }
                    }
                    // check level 4: check whether spent txouts were spent within the main chain
                    unsigned int nOutput = 0;
                    if (nCheckLevel > 3) {
                        for (const CDiskTxPos& txpos : txindex.vSpent) {
                            if (!txpos.IsNull()) {
                                uint256 posFind = txpos.nBlockPos;
                                if (!mapBlockPos.count(posFind)) {
                                    NLog.write(
                                        b_sev::warn,
                                        "LoadBlockIndex(): *** found bad spend at {}, hashBlock={}, "
                                        "hashTx={}",
                                        pindex->nHeight, pindex->GetBlockHash().ToString(),
                                        hashTx.ToString());
                                    pindexFork = pindex->getPrev(*this);
                                }
                                // check level 6: check whether spent txouts were spent by a valid
                                // transaction that consume them
                                if (nCheckLevel > 5) {
                                    CTransaction txSpend;
                                    if (!txSpend.ReadFromDisk(txpos, *this)) {
                                        NLog.write(
                                            b_sev::warn,
                                            "LoadBlockIndex(): *** cannot read spending transaction "
                                            "of {}:{} from disk",
                                            hashTx.ToString(), nOutput);
                                        pindexFork = pindex->getPrev(*this);
                                    } else if (txSpend.CheckTransaction(*this).isErr()) {
                                        NLog.write(b_sev::warn,
                                                   "LoadBlockIndex(): *** spending transaction of {}:{} "
                                                   "is invalid",
                                                   hashTx.ToString(), nOutput);
                                        pindexFork = pindex->getPrev(*this);
                                    } else {
                                        bool fFound = false;
                                        for (const CTxIn& txin : txSpend.vin)
                                            if (txin.prevout.hash == hashTx && txin.prevout.n == nOutput)
                                                fFound = true;
                                        if (!fFound) {
                                            NLog.write(
                                                b_sev::warn,
                                                "LoadBlockIndex(): *** spending transaction of {}:{} "
                                                "does not spend it",
                                                hashTx.ToString(), nOutput);
                                            pindexFork = pindex->getPrev(*this);
                                        }
                                    }
                                }
                            }
                            nOutput++;
                        }
                    }
                }
                // check level 5: check whether all prevouts are marked spent
                if (nCheckLevel > 4) {
                    for (const CTxIn& txin : tx.vin) {
                        CTxIndex txindexP;
                        if (ReadTxIndex(txin.prevout.hash, txindexP))
                            if (txindexP.vSpent.size() - 1 < txin.prevout.n ||
                                txindexP.vSpent[txin.prevout.n].IsNull()) {
                                NLog.write(b_sev::debug,
                                           "LoadBlockIndex(): *** found unspent prevout {}:{} in {}",
                                           txin.prevout.hash.ToString(), txin.prevout.n,
                                           hashTx.ToString());
                                pindexFork = pindex->getPrev(*this);
                            }
                    }
                }
            }
        }
    }

    NLog.write(b_sev::info, "Verifying latest blocks done.");
    uiInterface.InitMessage("Verifying latest blocks done");

    if (pindexFork && !fRequestShutdown) {
        // Reorg back to the fork
        NLog.write(b_sev::debug, "LoadBlockIndex() : *** moving best chain pointer back to block {}",
                   pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(&*pindexFork, *this)) {
            NLog.write(b_sev::err, "LoadBlockIndex() : block.ReadFromDisk failed");
            return false;
        }
        block.SetBestChain(*this, pindexFork);
    }

    return true;
}

boost::optional<int> CTxDB::GetBestChainHeight() const
{
    if (auto v = GetBestBlockIndex()) {
        if (v.is_initialized()) {
            return v->nHeight;
        }
    }
    return boost::none;
}

boost::optional<uint256> CTxDB::GetBestChainTrust() const
{
    if (auto v = GetBestBlockIndex()) {
        if (v.is_initialized()) {
            return v->nChainTrust;
        }
    }
    return boost::none;
}

uint256 CTxDB::GetBestBlockHash() const
{
    uint256 result;
    if (ReadHashBestChain(result)) {
        return result;
    }
    return 0;
}

boost::optional<CBlockIndex> CTxDB::GetBestBlockIndex() const
{
    uint256 bestChainHash = 0;
    if (ReadHashBestChain(bestChainHash)) {
        return ReadBlockIndex(bestChainHash);
    }
    NLog.write(b_sev::critical, "CRITICAL ERROR: best block index was unloadable from DB");
    return boost::none;
}

uintmax_t CTxDB::GetCurrentDiskUsage()
{
    try {
        boost::filesystem::path path(GetDataDir() / DB_DIR / "data.mdb");
        return boost::filesystem::file_size(path);
    } catch (...) {
        return 0;
    }
}

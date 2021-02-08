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

#include "db/defaultlogger/defaultlogger.h"
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
std::unique_ptr<ILog>   CTxDB::TxDBLogger                     = MakeUnique<DefaultLogger>();

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
                printf("Downloading file for QuickSync: %s...\n", urls[i].c_str());
                static const long connectionTimeout = 300;
                cURLTools::GetLargeFileFromHTTPS(urls[i], connectionTimeout, downloadTempTarget,
                                                 progress, std::set<CURLcode>({CURLE_PARTIAL_FILE}));
                printf("Setting promise value for downloaded file: %s...\n", urls[i].c_str());
                downloadThreadPromise.set_value();
                printf("Done setting promise value for downloaded file: %s...\n", urls[i].c_str());
                break; // break if a file is downloaded successfully
            } catch (std::exception& ex) {
                // if this is the last file, set the exception and fail
                printf("Failed to download a file %s. The last error is: %s\n", urls[i].c_str(),
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
    printf("Done downloading %s\n", leaf.c_str());
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
            printf("%s\n", msg.c_str());
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
                    printf("Skipping database with version %" PRIu64 "", dbversion);
                    continue;
                }

                if (!IsQuickSyncOSCompatible(os)) {
                    printf("Skipping database with OS %" PRIu64 "", dbversion);
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
            printf("Quick sync failed (attempt %i of %i). Error: %s\n", failedAttempts,
                   MAX_FAILED_ATTEMPTS, ex.what());
            std::this_thread::sleep_for(std::chrono::seconds(WAIT_TIME_SECONDS));
        }
    }
    uiInterface.InitMessage("QuickSync done");
    if (!success) {
        throw std::runtime_error("QuickSync error: None of the files matched the correct settings or "
                                 "another error occurred.");
    }
    printf("QuickSync done\n");
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
    if (forceClearDB ||
        SC_CheckOperationOnRestartScheduleThenDeleteIt(SC_SCHEDULE_ON_RESTART_OPNAME__RESYNC)) {

        db->clearDBData();

        // after a resync, always rescan the wallet
        SC_CreateScheduledOperationOnRestart(SC_SCHEDULE_ON_RESTART_OPNAME__RESCAN);
    }

    try {
        RunCrossPlatformSerializationTests();
        printf("Binary format tests have passed.\n");
    } catch (std::exception& ex) {
        printf("Binary format tests have failed: %s\n", ex.what());
    }

    if (const auto dbdir = db->getDataDir()) {
        // if the directory doesn't exist, use quicksync
        if (ShouldQuickSyncBeDone(*dbdir)) {
            // close the database before running quicksync
            this->Close();

            try {
                // binary layout compatibility is necessary for quicksync to work
                RunCrossPlatformSerializationTests();
                printf("Binary format tests have passed.\n");
                DoQuickSync(*dbdir);

                // after quicksync, a rescan has to be done
                SC_CreateScheduledOperationOnRestart(SC_SCHEDULE_ON_RESTART_OPNAME__RESCAN);
            } catch (std::exception& ex) {
                printf("Quicksync exited with an exception (this is not expected to happen): %s\n",
                       ex.what());
                db->clearDBData();
            }
        }
    }

    db->openDB(false);
}

// CDB subclasses are created and destroyed VERY OFTEN. That's why
// we shouldn't treat this as a free operations.
CTxDB::CTxDB()
{
    static boost::filesystem::path DBDir = GetDataDir() / DB_DIR;

    db = MakeUnique<LMDB>(&DBDir, TxDBLogger.get());
}

void CTxDB::Close() { db->close(); }

void CTxDB::__deleteDb()
{
    try {
        boost::filesystem::remove_all(GetDataDir() / DB_DIR);
    } catch (...) {
    }
}

bool CTxDB::TxnBegin(size_t required_size) { return db->beginDBTransaction(required_size); }

bool CTxDB::TxnCommit() { return db->commitDBTransaction(); }

bool CTxDB::TxnAbort() { return db->abortDBTransaction(); }

bool CTxDB::test1_WriteStrKeyVal(const string& key, const string& val)
{
    return db->write(IDB::Index::DB_MAIN_INDEX, key, val);
}

bool CTxDB::test1_ReadStrKeyVal(const string& key, string& val)
{
    auto res = db->read(IDB::Index::DB_MAIN_INDEX, key, 0, boost::none);
    val      = res.value_or(val);
    return !!res;
}

bool CTxDB::test1_ExistsStrKeyVal(const string& key)
{
    return db->exists(IDB::Index::DB_MAIN_INDEX, key);
}
bool CTxDB::test1_EraseStrKeyVal(const string& key) { return db->erase(IDB::Index::DB_MAIN_INDEX, key); }

bool CTxDB::test2_ReadMultipleStr1KeyVal(const string& key, vector<string>& val)
{
    auto res = db->readMultiple(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key);
    val      = res.value_or(val);
    return !!res;
}

bool CTxDB::test2_ReadMultipleAllStr1KeyVal(std::map<string, vector<string>>& vals)
{
    auto res = db->readAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX);
    vals     = res.value_or(vals);
    return !!res;
}

bool CTxDB::test2_WriteStrKeyVal(const string& key, const string& val)
{
    return db->write(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key, val);
}

bool CTxDB::test2_ExistsStrKeyVal(const string& key)
{
    return db->exists(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key);
}
bool CTxDB::test2_EraseStrKeyVal(const string& key)
{
    return db->eraseAll(IDB::Index::DB_NTP1TOKENNAMES_INDEX, key);
}

boost::optional<int> CTxDB::ReadVersion()
{
    return Read(std::string("version"), nVersion, IDB::Index::DB_MAIN_INDEX)
               ? boost::make_optional(nVersion)
               : boost::none;
}

bool CTxDB::WriteVersion(int nVersion)
{
    return Write(std::string("version"), nVersion, IDB::Index::DB_MAIN_INDEX);
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
        printf("Attempted to store token symbol information of token with given symbol %s",
               tokenSymbol.c_str());
        return false;
    }
    std::string symbol;
    try {
        symbol = ntp1tx.getTokenSymbolIfIssuance();
    } catch (std::exception& ex) {
        printf("Failed to get token symbol for transaction: %s; with claimed token symbol %s. Error: %s",
               ntp1tx.getTxHash().ToString().c_str(), tokenSymbol.c_str(), ex.what());
        return false;
    } catch (...) {
        printf("Failed to get token symbol for transaction: %s; with claimed token symbol %s. Unknown "
               "error.",
               ntp1tx.getTxHash().ToString().c_str(), tokenSymbol.c_str());
        return false;
    }

    std::transform(tokenSymbol.begin(), tokenSymbol.end(), tokenSymbol.begin(), ::toupper);
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

    if (symbol != tokenSymbol) {
        printf("While writing NTP1 tx for token names, the token name provided is not equal to the "
               "token name calculated: %s != %s",
               symbol.c_str(), tokenSymbol.c_str());
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
    assert(blk.GetHash() != 0);
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

bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(blockindex.GetBlockHash(), blockindex, IDB::Index::DB_BLOCKINDEX_INDEX);
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

static CBlockIndexSmartPtr InsertBlockIndex(const uint256&              hash,
                                            BlockIndexMapType::MapType& blockIndexMap)
{
    if (hash == 0)
        return nullptr;

    // Return existing
    BlockIndexMapType::MapType::iterator mi = blockIndexMap.find(hash);
    if (mi != blockIndexMap.end())
        return mi->second;

    // Create new
    CBlockIndexSmartPtr pindexNew = boost::make_shared<CBlockIndex>();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi                    = blockIndexMap.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = mi->first;

    return pindexNew;
}

std::string LmdbValToString(const MDB_val& val)
{
    return std::string((const char*)val.mv_data, val.mv_size);
}

bool CTxDB::LoadBlockIndex()
{
    if (mapBlockIndex.size() > 0) {
        // Already loaded once in this session. It can happen during migration
        // from BDB.
        return true;
    }

    // The block index is an in-memory structure that maps hashes to on-disk
    // locations where the contents of the block can be found. Here, we scan it
    // out of the DB and into mapBlockIndex.

    boost::optional<std::map<std::string, std::vector<std::string>>> blockIndexStr =
        db->readAll(IDB::Index::DB_BLOCKINDEX_INDEX);

    uint64_t loadedCount = 0;

    BlockIndexMapType::MapType loadedBlockIndex;

    if (blockIndexStr) {
        // Now read each entry.
        while (!blockIndexStr->empty()) {
            // TODO: make db->readAllUnique to not read to a vector
            const std::pair<const std::string, const std::vector<std::string>> p =
                *blockIndexStr->begin();

            // Unpack keys and values.
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            ssKey.write(p.first.data(), p.first.size());
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            ssValue.write(p.second.front().data(), p.second.front().size());

            if (fRequestShutdown)
                break;

            uint256 blockHash;
            ssKey >> blockHash;

            CDiskBlockIndex diskindex;
            ssValue >> diskindex;

            // (Changed by Sam) previously, using diskindex.GetBlockHash retrieved the block hash AND set
            // it inside the diskindex object with a const_cast. Now this is fixed to be correct
            diskindex.SetBlockHash(blockHash);

            // Construct block index object
            CBlockIndexSmartPtr pindexNew = InsertBlockIndex(blockHash, loadedBlockIndex);
            pindexNew->pprev              = InsertBlockIndex(diskindex.hashPrev, loadedBlockIndex);
            pindexNew->pnext              = InsertBlockIndex(diskindex.hashNext, loadedBlockIndex);
            pindexNew->blockKeyInDB       = diskindex.blockKeyInDB;
            pindexNew->nHeight            = diskindex.nHeight;
            pindexNew->nMint              = diskindex.nMint;
            pindexNew->nMoneySupply       = diskindex.nMoneySupply;
            pindexNew->nFlags             = diskindex.nFlags;
            pindexNew->nStakeModifier     = diskindex.nStakeModifier;
            pindexNew->prevoutStake       = diskindex.prevoutStake;
            pindexNew->nStakeTime         = diskindex.nStakeTime;
            pindexNew->hashProof          = diskindex.hashProof;
            pindexNew->nVersion           = diskindex.nVersion;
            pindexNew->hashMerkleRoot     = diskindex.hashMerkleRoot;
            pindexNew->nTime              = diskindex.nTime;
            pindexNew->nBits              = diskindex.nBits;
            pindexNew->nNonce             = diskindex.nNonce;

            // Watch for genesis block
            if (pindexGenesisBlock == nullptr && blockHash == Params().GenesisBlockHash())
                pindexGenesisBlock = pindexNew;

            if (!pindexNew->CheckIndex()) {
                return error("LoadBlockIndex() : CheckIndex failed at %d", pindexNew->nHeight);
            }

            // NovaCoin: build setStakeSeen
            if (pindexNew->IsProofOfStake())
                setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));

            loadedCount++;
            if (loadedCount % 10000 == 0) {
                uiInterface.InitMessage(_("Loading block index...") +
                                        " (block: " + std::to_string(loadedCount) + ")");
            }

            // delete the current loaded entry from the map
            blockIndexStr->erase(p.first);
        }
    }
    printf("Done reading block index\n");
    uiInterface.InitMessage(_("Loading block index...") + " (done reading block index)");

    if (fRequestShutdown)
        return true;

    // Calculate nChainTrust
    vector<pair<int, CBlockIndex*>> vSortedByHeight;
    vSortedByHeight.reserve(loadedBlockIndex.size());
    uiInterface.InitMessage("Building chain trust... (allocating memory...)");
    for (const PAIRTYPE(const uint256, CBlockIndexSmartPtr) & item : loadedBlockIndex) {
        CBlockIndex* pindex = item.second.get();
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    // use heap-sort to guarantee O(n*log(n)) performance, since std::sort() can have O(n^2) complexity
    uiInterface.InitMessage("Building chain trust... (sorting...)");
    std::make_heap(vSortedByHeight.begin(), vSortedByHeight.end());
    std::sort_heap(vSortedByHeight.begin(), vSortedByHeight.end());
    loadedCount = 0;
    for (const PAIRTYPE(int, CBlockIndex*) & item : vSortedByHeight) {
        loadedCount++;
        if (loadedCount % 50000 == 0) {
            uiInterface.InitMessage(
                "Building chain trust... (chaining block: " + std::to_string(loadedCount) + "/" +
                std::to_string(vSortedByHeight.size()) + ")");
        }
        CBlockIndex* pindex = item.second;
        pindex->nChainTrust = (pindex->pprev ? pindex->pprev->nChainTrust : 0) + pindex->GetBlockTrust();
        // NovaCoin: calculate stake modifier checksum
        pindex->nStakeModifierChecksum = GetStakeModifierChecksum(pindex);
        if (!CheckStakeModifierCheckpoints(pindex->nHeight, pindex->nStakeModifierChecksum))
            return error("CTxDB::LoadBlockIndex() : Failed stake modifier checkpoint height=%d, "
                         "modifier=0x%016" PRIx64,
                         pindex->nHeight, pindex->nStakeModifier);
    }

    // Load hashBestChain pointer to end of best chain
    uint256 hashBestChainTemp = 0;
    if (!ReadHashBestChain(hashBestChainTemp)) {
        if (pindexGenesisBlock == nullptr)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }
    if (!loadedBlockIndex.count(hashBestChainTemp))
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");
    //    bestChain.setBestChain(loadedBlockIndex.at(hashBestChainTemp), false);

    const int bestHeight = loadedBlockIndex.at(hashBestChainTemp)->nHeight;

    printf("LoadBlockIndex(): hashBestChain=%s  height=%d  trust=%s  date=%s\n",
           hashBestChainTemp.ToString().substr(0, 20).c_str(), bestHeight,
           CBigNum(GetBestChainTrust().value_or(0)).ToString().c_str(),
           DateTimeStrFormat("%x %H:%M:%S", loadedBlockIndex.at(hashBestChainTemp)->GetBlockTime())
               .c_str());

    // Load bnBestInvalidTrust, OK if it doesn't exist
    CBigNum bnBestInvalidTrust;
    ReadBestInvalidTrust(bnBestInvalidTrust);
    nBestInvalidTrust = bnBestInvalidTrust.getuint256();

    CTxDB txdb;
    // Verify blocks in the best chain
    int nCheckLevel = GetArg("-checklevel", 1);
    int nCheckDepth = GetArg("-checkblocks", 2500);
    if (nCheckDepth == 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > bestHeight)
        nCheckDepth = bestHeight;
    printf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CBlockIndexSmartPtr              pindexFork = nullptr;
    map<uint256, const CBlockIndex*> mapBlockPos;
    loadedCount = 0;
    for (ConstCBlockIndexSmartPtr pindex = loadedBlockIndex.at(hashBestChainTemp);
         pindex && pindex->pprev; pindex = pindex->pprev) {

        if (loadedCount % 100 == 0) {
            uiInterface.InitMessage("Verifying latest blocks (" + std::to_string(loadedCount) + "/" +
                                    std::to_string(nCheckDepth) + ")");
        }
        loadedCount++;

        if (fRequestShutdown || pindex->nHeight < bestHeight - nCheckDepth)
            break;
        CBlock block;
        if (!block.ReadFromDisk(pindex.get()))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        // check level 1: verify block validity
        // check level 7: verify block signature too
        if (nCheckLevel > 0 && !block.CheckBlock(txdb, true, true, (nCheckLevel > 6))) {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n", pindex->nHeight,
                   pindex->GetBlockHash().ToString().c_str());
            pindexFork = pindex->pprev;
        }
        // check level 2: verify transaction index validity
        if (nCheckLevel > 1) {
            uint256 pos      = pindex->blockKeyInDB;
            mapBlockPos[pos] = pindex.get();
            for (const CTransaction& tx : block.vtx) {
                uint256  hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex)) {
                    // check level 3: checker transaction hashes
                    if (nCheckLevel > 2 || pindex->blockKeyInDB != txindex.pos.nBlockPos) {
                        // either an error or a duplicate transaction
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos, txdb)) {
                            printf("LoadBlockIndex() : *** cannot read mislocated transaction %s\n",
                                   hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        } else if (txFound.GetHash() != hashTx) // not a duplicate tx
                        {
                            printf("LoadBlockIndex(): *** invalid tx position for %s\n",
                                   hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        }
                    }
                    // check level 4: check whether spent txouts were spent within the main chain
                    unsigned int nOutput = 0;
                    if (nCheckLevel > 3) {
                        for (const CDiskTxPos& txpos : txindex.vSpent) {
                            if (!txpos.IsNull()) {
                                uint256 posFind = txpos.nBlockPos;
                                if (!mapBlockPos.count(posFind)) {
                                    printf("LoadBlockIndex(): *** found bad spend at %d, hashBlock=%s, "
                                           "hashTx=%s\n",
                                           pindex->nHeight, pindex->GetBlockHash().ToString().c_str(),
                                           hashTx.ToString().c_str());
                                    pindexFork = pindex->pprev;
                                }
                                // check level 6: check whether spent txouts were spent by a valid
                                // transaction that consume them
                                if (nCheckLevel > 5) {
                                    CTransaction txSpend;
                                    if (!txSpend.ReadFromDisk(txpos, txdb)) {
                                        printf("LoadBlockIndex(): *** cannot read spending transaction "
                                               "of %s:%i from disk\n",
                                               hashTx.ToString().c_str(), nOutput);
                                        pindexFork = pindex->pprev;
                                    } else if (txSpend.CheckTransaction(txdb).isErr()) {
                                        printf("LoadBlockIndex(): *** spending transaction of %s:%i is "
                                               "invalid\n",
                                               hashTx.ToString().c_str(), nOutput);
                                        pindexFork = pindex->pprev;
                                    } else {
                                        bool fFound = false;
                                        for (const CTxIn& txin : txSpend.vin)
                                            if (txin.prevout.hash == hashTx && txin.prevout.n == nOutput)
                                                fFound = true;
                                        if (!fFound) {
                                            printf("LoadBlockIndex(): *** spending transaction of %s:%i "
                                                   "does not spend it\n",
                                                   hashTx.ToString().c_str(), nOutput);
                                            pindexFork = pindex->pprev;
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
                        CTxIndex txindex;
                        if (ReadTxIndex(txin.prevout.hash, txindex))
                            if (txindex.vSpent.size() - 1 < txin.prevout.n ||
                                txindex.vSpent[txin.prevout.n].IsNull()) {
                                printf("LoadBlockIndex(): *** found unspent prevout %s:%i in %s\n",
                                       txin.prevout.hash.ToString().c_str(), txin.prevout.n,
                                       hashTx.ToString().c_str());
                                pindexFork = pindex->pprev;
                            }
                    }
                }
            }
        }
    }

    printf("Verifying latest blocks done.\n");
    uiInterface.InitMessage("Verifying latest blocks done");

    if (pindexFork && !fRequestShutdown) {
        // Reorg back to the fork
        printf("LoadBlockIndex() : *** moving best chain pointer back to block %d\n",
               pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(pindexFork.get()))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        CTxDB txdb;
        block.SetBestChain(txdb, pindexFork);
    }

    mapBlockIndex.setInternalMap(std::move(loadedBlockIndex));

    return true;
}

boost::optional<int> CTxDB::GetBestChainHeight() const
{
    uint256 bestChainHash = 0;
    if (ReadHashBestChain(bestChainHash)) {
        const auto v = mapBlockIndex.get(bestChainHash);
        if (v.is_initialized()) {
            return (*v)->nHeight;
        }
    }
    return boost::none;
}

boost::optional<uint256> CTxDB::GetBestChainTrust() const
{
    uint256 bestChainHash = 0;
    if (ReadHashBestChain(bestChainHash)) {
        const auto v = mapBlockIndex.get(bestChainHash);
        if (v.is_initialized()) {
            return (*v)->nChainTrust;
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

CBlockIndexSmartPtr CTxDB::GetBestBlockIndex() const
{
    uint256 bestChainHash = 0;
    if (ReadHashBestChain(bestChainHash)) {
        return mapBlockIndex.get(bestChainHash).value_or(nullptr);
    }
    return nullptr;
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

CTxDB::~CTxDB() {}

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletdb.h"
#include "blockindex.h"
#include "txdb.h"
#include "wallet.h"
#include <boost/filesystem.hpp>
#include <boost/version.hpp>

using namespace std;
using namespace boost;

static uint64_t nAccountingEntryNumber = 0;
extern bool     fWalletUnlockStakingOnly;

//
// CWalletDB
//

CWalletDB::CWalletDB(string strFilename, const char* pszMode, bool fFlushOnCloseIn)
    : CDB(strFilename.c_str(), pszMode, fFlushOnCloseIn)
{
}

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdated++;
    return Erase(make_pair(string("name"), strAddress));
}

bool CWalletDB::WriteTx(uint256 hash, const CWalletTx& wtx)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("tx"), hash), wtx);
}

bool CWalletDB::EraseTx(uint256 hash)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("tx"), hash));
}

bool CWalletDB::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey,
                         const CKeyMetadata& keyMeta)
{
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
        return false;

    return Write(std::make_pair(std::string("key"), vchPubKey.Raw()), vchPrivKey, false);
}

bool CWalletDB::WriteCryptedKey(const CPubKey&                    vchPubKey,
                                const std::vector<unsigned char>& vchCryptedSecret,
                                const CKeyMetadata&               keyMeta)
{
    nWalletDBUpdated++;
    bool fEraseUnencryptedKey = true;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("ckey"), vchPubKey.Raw()), vchCryptedSecret, false))
        return false;
    if (fEraseUnencryptedKey) {
        Erase(std::make_pair(std::string("key"), vchPubKey.Raw()));
        Erase(std::make_pair(std::string("wkey"), vchPubKey.Raw()));
    }
    return true;
}

bool CWalletDB::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool CWalletDB::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("cscript"), hash), redeemScript, false);
}

bool CWalletDB::WriteBestBlock(const CBlockLocator& locator)
{
    nWalletDBUpdated++;
    return Write(std::string("bestblock"), locator);
}

bool CWalletDB::WriteOrderPosNext(int64_t nOrderPosNext)
{
    nWalletDBUpdated++;
    return Write(std::string("orderposnext"), nOrderPosNext);
}

bool CWalletDB::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::ErasePool(int64_t nPool)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("pool"), nPool));
}

bool CWalletDB::WritePurpose(const std::string& strAddress, const std::string& strPurpose)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("purpose"), strAddress), strPurpose);
}

bool CWalletDB::ErasePurpose(const std::string& strPurpose)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("purpose"), strPurpose));
}

bool CWalletDB::ReadAccount(const string& strAccount, CAccount& account)
{
    account.SetNull();
    return Read(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccount(const string& strAccount, const CAccount& account)
{
    return Write(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return Write(boost::make_tuple(string("acentry"), acentry.strAccount, nAccEntryNum), acentry);
}

bool CWalletDB::WriteAccountingEntry(const CAccountingEntry& acentry)
{
    return WriteAccountingEntry(++nAccountingEntryNumber, acentry);
}

int64_t CWalletDB::GetAccountCreditDebit(const string& strAccount)
{
    list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    int64_t nCreditDebit = 0;
    BOOST_FOREACH (const CAccountingEntry& entry, entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const string& strAccount, list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw runtime_error("CWalletDB::ListAccountCreditDebit() : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    while (true) {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << boost::make_tuple(string("acentry"), (fAllAccounts ? string("") : strAccount),
                                       uint64_t(0));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int         ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags          = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0) {
            pcursor->close();
            throw runtime_error("CWalletDB::ListAccountCreditDebit() : error scanning DB");
        }

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        ssKey >> acentry.nEntryNo;
        entries.push_back(acentry);
    }

    pcursor->close();
}

DBErrors CWalletDB::ReorderTransactions(CWallet* pwallet)
{
    LOCK(pwallet->cs_wallet);
    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap.
    typedef pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef multimap<int64_t, TxPair>           TxItems;
    TxItems                                     txByTime;

    for (map<uint256, CWalletTx>::iterator it = pwallet->mapWallet.begin();
         it != pwallet->mapWallet.end(); ++it) {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    list<CAccountingEntry> acentries;
    ListAccountCreditDebit("", acentries);
    BOOST_FOREACH (CAccountingEntry& entry, acentries) {
        txByTime.insert(make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    int64_t& nOrderPosNext = pwallet->nOrderPosNext;
    nOrderPosNext          = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it) {
        CWalletTx* const        pwtx      = (*it).second.first;
        CAccountingEntry* const pacentry  = (*it).second.second;
        int64_t&                nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1) {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pacentry)
                // Have to write accounting regardless, since we don't keep it in memory
                if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        } else {
            int64_t nOrderPosOff = 0;
            BOOST_FOREACH (const int64_t& nOffsetStart, nOrderPosOffsets) {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (pwtx) {
                if (!WriteTx(pwtx->GetHash(), *pwtx))
                    return DB_LOAD_FAIL;
            } else if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                return DB_LOAD_FAIL;
        }
    }

    return DB_LOAD_OK;
}

class CWalletScanState
{
public:
    unsigned int    nKeys;
    unsigned int    nCKeys;
    unsigned int    nKeyMeta;
    bool            fIsEncrypted;
    bool            fAnyUnordered;
    int             nFileVersion;
    vector<uint256> vWalletUpgrade;

    CWalletScanState()
    {
        nKeys = nCKeys = nKeyMeta = 0;
        fIsEncrypted              = false;
        fAnyUnordered             = false;
        nFileVersion              = 0;
    }
};

boost::optional<int> GetBlockHeightOfMainChainTx(const ITxDB& txdb, const CWalletTx& wtx)
{
    CTxIndex txindex;
    if (!txdb.ReadTxIndex(wtx.GetHash(), txindex)) {
        return boost::none; // it's not in the mainchain
    }
    const boost::optional<CBlockIndex> bi = txdb.ReadBlockIndex(txindex.pos.nBlockPos);
    if (!bi) {
        NLog.critical(
            "Failed to read block index of block {} which includes a mainchain transaction {} in your "
            "wallet; there seems to be a database corruption",
            txindex.pos.nBlockPos.ToString(), wtx.GetHash().ToString());
    }
    return boost::make_optional(bi->nHeight);
}

bool ReadKeyValue(const ITxDB& txdb, CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
                  CWalletScanState& wss, string& strType, string& strErr)
{
    try {
        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        if (strType == "name") {
            string strAddress;
            ssKey >> strAddress;
            AddressBook::CAddressBookData d =
                pwallet->mapAddressBook.get(CBitcoinAddress(strAddress).Get())
                    .value_or(AddressBook::CAddressBookData());
            ssValue >> d.name;
            pwallet->mapAddressBook.set(CBitcoinAddress(strAddress).Get(), d);
        } else if (strType == "purpose") {
            std::string strAddress;
            ssKey >> strAddress;
            AddressBook::CAddressBookData d =
                pwallet->mapAddressBook.get(CBitcoinAddress(strAddress).Get())
                    .value_or(AddressBook::CAddressBookData());
            ssValue >> d.purpose;
            pwallet->mapAddressBook.set(CBitcoinAddress(strAddress).Get(), d);
        } else if (strType == "tx") {
            uint256 hash;
            ssKey >> hash;
            CWalletTx& wtx = pwallet->mapWallet[hash];
            ssValue >> wtx;
            const boost::optional<int> txHeight = GetBlockHeightOfMainChainTx(txdb, wtx);
            if (txHeight && wtx.CheckTransaction(*txHeight).isOk() && (wtx.GetHash() == hash))
                wtx.BindWallet(pwallet);
            else {
                pwallet->mapWallet.erase(hash);
                return false;
            }

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703) {
                if (!ssValue.empty()) {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = fmt::format("LoadWallet() upgrading tx ver={} {} '{}' {}",
                                         wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount,
                                         hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                } else {
                    strErr                    = fmt::format("LoadWallet() repairing tx ver={} {}",
                                         wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

            pwallet->AddToWallet(txdb, wtx, true, nullptr, false);

            //// debug print
            // NLog.write(b_sev::debug, "LoadWallet  {}", wtx.GetHash().ToString());
            // NLog.write(b_sev::debug, " {}  {}  {}  {}", wtx.vout[0].nValue,
            //           DateTimeStrFormat("%x %H:%M:%S", wtx.GetTxTime()),
            //           wtx.hashBlock.ToString().substr(0, 20), wtx.mapValue["message"]);
        } else if (strType == "acentry") {
            string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            if (nNumber > nAccountingEntryNumber)
                nAccountingEntryNumber = nNumber;

            if (!wss.fAnyUnordered) {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
        } else if (strType == "key" || strType == "wkey") {
            vector<unsigned char> vchPubKey;
            ssKey >> vchPubKey;
            CKey key;
            if (strType == "key") {
                wss.nKeys++;
                CPrivKey pkey;
                ssValue >> pkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(pkey)) {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                if (key.GetPubKey() != vchPubKey) {
                    strErr = "Error reading wallet database: CPrivKey pubkey inconsistency";
                    return false;
                }
                if (!key.IsValid()) {
                    strErr = "Error reading wallet database: invalid CPrivKey";
                    return false;
                }
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(wkey.vchPrivKey)) {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                if (key.GetPubKey() != vchPubKey) {
                    strErr = "Error reading wallet database: CWalletKey pubkey inconsistency";
                    return false;
                }
                if (!key.IsValid()) {
                    strErr = "Error reading wallet database: invalid CWalletKey";
                    return false;
                }
            }
            if (!pwallet->LoadKey(key)) {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        } else if (strType == "mkey") {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if (pwallet->mapMasterKeys.count(nID) != 0) {
                strErr = fmt::format("Error reading wallet database: duplicate CMasterKey id {}", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        } else if (strType == "ckey") {
            wss.nCKeys++;
            vector<unsigned char> vchPubKey;
            ssKey >> vchPubKey;
            vector<unsigned char> vchPrivKey;
            ssValue >> vchPrivKey;
            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey)) {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        } else if (strType == "keymeta") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);

            // find earliest key creation time, as wallet birthday
            if (!pwallet->nTimeFirstKey || (keyMeta.nCreateTime < pwallet->nTimeFirstKey))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
        } else if (strType == "defaultkey") {
            // We don't want or need the default key, but if there is one set,
            // we want to make sure that it is valid so that we can detect corruption
            CPubKey vchPubKey;
            ssValue >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: Default Key corrupt";
                return false;
            }
        } else if (strType == "pool") {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;
            pwallet->setKeyPool.insert(nIndex);

            // If no metadata exists yet, create a default with the pool key's
            // creation time. Note that this may be overwritten by actually
            // stored metadata for that key later, which is fine.
            CKeyID keyid = keypool.vchPubKey.GetID();
            if (pwallet->mapKeyMetadata.count(keyid) == 0)
                pwallet->mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);

        } else if (strType == "version") {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        } else if (strType == "cscript") {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> script;
            if (!pwallet->LoadCScript(script)) {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        } else if (strType == "orderposnext") {
            ssValue >> pwallet->nOrderPosNext;
        }
    } catch (...) {
        return false;
    }
    return true;
}

static bool IsKeyType(string strType)
{
    return (strType == "key" || strType == "wkey" || strType == "mkey" || strType == "ckey");
}

DBErrors CWalletDB::LoadWallet(CWallet* pwallet)
{
    CWalletScanState wss;
    bool             fNoncriticalErrors = false;
    DBErrors         result             = DB_LOAD_OK;

    const CTxDB txdb;

    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((string) "minversion", nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor) {
            NLog.write(b_sev::err, "Error getting wallet database cursor");
            return DB_CORRUPT;
        }

        while (true) {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int         ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0) {
                NLog.write(b_sev::err, "Error reading next record from wallet database");
                return DB_CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            string strType, strErr;
            if (!ReadKeyValue(txdb, pwallet, ssKey, ssValue, wss, strType, strErr)) {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType) || strType == "defaultkey")
                    result = DB_CORRUPT;
                else {
                    // Leave other errors alone, if we try to fix them we might make things worse.
                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    if (strType == "tx")
                        // Rescan if there is a bad transaction record:
                        SoftSetBoolArg("-rescan", true);
                }
            }
            if (!strErr.empty())
                NLog.write(b_sev::info, "{}", strErr);
        }
        pcursor->close();
    } catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DB_LOAD_OK)
        return result;

    NLog.write(b_sev::info, "nFileVersion = {}", wss.nFileVersion);

    NLog.write(b_sev::info, "Keys: {} plaintext, {} encrypted, {} w/ metadata, {} total", wss.nKeys,
               wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys);

    // nTimeFirstKey is only reliable if all keys have metadata
    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1; // 0 would be considered 'no value'

    BOOST_FOREACH (uint256 hash, wss.vWalletUpgrade)
        WriteTx(hash, pwallet->mapWallet[hash]);

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (wss.nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = ReorderTransactions(pwallet);

    return result;
}

// only call this when you're sure that cs_db lock is activated
void FlushWalletDB_unsafe(const std::string& strFile, unsigned int* nLastFlushedPtr = nullptr)
{
    // Don't do this if any databases are in use
    int                        nRefCount = 0;
    map<string, int>::iterator mi        = bitdb.mapFileUseCount.begin();
    while (mi != bitdb.mapFileUseCount.end()) {
        nRefCount += (*mi).second;
        mi++;
    }

    if (nRefCount == 0 && !fShutdown) {
        map<string, int>::iterator mit = bitdb.mapFileUseCount.find(strFile);
        if (mit != bitdb.mapFileUseCount.end()) {
            NLog.write(b_sev::info, "Flushing wallet.dat");
            if (nLastFlushedPtr) {
                *nLastFlushedPtr = nWalletDBUpdated;
            }
            const int64_t nStart = GetTimeMillis();

            // Flush wallet.dat so it's self contained
            bitdb.CloseDb(strFile);
            bitdb.CheckpointLSN(strFile);

            bitdb.mapFileUseCount.erase(mit++);
            NLog.write(b_sev::info, "Flushed wallet.dat | {} ms", GetTimeMillis() - nStart);
        }
    }
}

void FlushWalletDB(bool forceLockAndFlush, const std::string& strFile, unsigned int* nLastFlushedPtr)
{
    if (!forceLockAndFlush) {
        TRY_LOCK(bitdb.cs_db, lockDb);
        if (lockDb) {
            FlushWalletDB_unsafe(strFile, nLastFlushedPtr);
        }
    } else {
        LOCK(bitdb.cs_db);
        FlushWalletDB_unsafe(strFile, nLastFlushedPtr);
    }
}

void ThreadFlushWalletDB(const string strFile)
{
    // Make this thread recognisable as the wallet flushing thread
    RenameThread("neblio-wallet");

    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (!GetBoolArg("-flushwallet", true))
        return;

    unsigned int nLastSeen         = nWalletDBUpdated;
    unsigned int nLastFlushed      = nWalletDBUpdated;
    int64_t      nLastWalletUpdate = GetTime();
    while (!fShutdown) {
        MilliSleep(500);

        if (nLastSeen != nWalletDBUpdated) {
            nLastSeen         = nWalletDBUpdated;
            nLastWalletUpdate = GetTime();
        }

        if (nLastFlushed != nWalletDBUpdated && GetTime() - nLastWalletUpdate >= 2) {
            FlushWalletDB(false, strFile, &nLastFlushed);
        }
    }
}

bool BackupWallet(const CWallet& wallet, const string& strDest)
{
    if (!wallet.fFileBacked)
        return false;
    while (!fShutdown) {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(wallet.strWalletFile) ||
                bitdb.mapFileUseCount[wallet.strWalletFile] == 0) {
                // Flush log data to the dat file
                bitdb.CloseDb(wallet.strWalletFile);
                bitdb.CheckpointLSN(wallet.strWalletFile);
                bitdb.mapFileUseCount.erase(wallet.strWalletFile);

                // Copy wallet.dat
                filesystem::path pathSrc = GetDataDir() / wallet.strWalletFile;
                filesystem::path pathDest(strDest);
                if (filesystem::is_directory(pathDest))
                    pathDest /= wallet.strWalletFile;

                try {
#if BOOST_VERSION >= 104000
                    filesystem::copy_file(pathSrc, pathDest,
                                          filesystem::copy_option::overwrite_if_exists);
#else
                    filesystem::copy_file(pathSrc, pathDest);
#endif
                    NLog.write(b_sev::info, "copied wallet.dat to {}", pathDest.string());
                    return true;
                } catch (const filesystem::filesystem_error& e) {
                    NLog.write(b_sev::err, "error copying wallet.dat to {} - {}", pathDest.string(),
                               e.what());
                    return false;
                }
            }
        }
        MilliSleep(100);
    }
    return false;
}

//
// Try to (very carefully!) recover wallet.dat if there is a problem.
//
bool CWalletDB::Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys)
{
    // Recovery procedure:
    // move wallet.dat to wallet.timestamp.bak
    // Call Salvage with fAggressive=true to
    // get as much data as possible.
    // Rewrite salvaged data to wallet.dat
    // Set -rescan so any missing transactions will be
    // found.
    int64_t     now         = GetTime();
    std::string newFilename = fmt::format("wallet.{}.bak", now);

    int result = dbenv.dbenv.dbrename(NULL, filename.c_str(), NULL, newFilename.c_str(), DB_AUTO_COMMIT);
    if (result == 0)
        NLog.write(b_sev::info, "Renamed {} to {}", filename, newFilename);
    else {
        NLog.write(b_sev::err, "Failed to rename {} to {}", filename, newFilename);
        return false;
    }

    const CTxDB txdb;

    std::vector<CDBEnv::KeyValPair> salvagedData;
    bool                            allOK = dbenv.Salvage(newFilename, true, salvagedData);
    if (salvagedData.empty()) {
        NLog.write(b_sev::warn, "Salvage(aggressive) found no records in {}.", newFilename);
        return false;
    }
    NLog.write(b_sev::info, "Salvage(aggressive) found {} records", salvagedData.size());

    bool fSuccess = allOK;
    Db*  pdbCopy  = new Db(&dbenv.dbenv, 0);
    int  ret      = pdbCopy->open(NULL,             // Txn pointer
                            filename.c_str(), // Filename
                            "main",           // Logical db name
                            DB_BTREE,         // Database type
                            DB_CREATE,        // Flags
                            0);
    if (ret > 0) {
        NLog.write(b_sev::err, "Cannot create database file {}", filename);
        return false;
    }
    CWallet          dummyWallet;
    CWalletScanState wss;

    DbTxn* ptxn = dbenv.TxnBegin();
    for (CDBEnv::KeyValPair& row : salvagedData) {
        if (fOnlyKeys) {
            CDataStream ssKey(row.first, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(row.second, SER_DISK, CLIENT_VERSION);
            string      strType, strErr;
            bool        fReadOK = ReadKeyValue(txdb, &dummyWallet, ssKey, ssValue, wss, strType, strErr);
            if (!IsKeyType(strType))
                continue;
            if (!fReadOK) {
                NLog.write(b_sev::warn, "WARNING: CWalletDB::Recover skipping {}: {}", strType, strErr);
                continue;
            }
        }
        Dbt datKey(&row.first[0], row.first.size());
        Dbt datValue(&row.second[0], row.second.size());
        int ret2 = pdbCopy->put(ptxn, &datKey, &datValue, DB_NOOVERWRITE);
        if (ret2 > 0)
            fSuccess = false;
    }
    ptxn->commit(0);
    pdbCopy->close(0);
    delete pdbCopy;

    return fSuccess;
}

bool CWalletDB::Recover(CDBEnv& dbenv, std::string filename)
{
    return CWalletDB::Recover(dbenv, filename, false);
}

CKeyMetadata::CKeyMetadata() { SetNull(); }

CKeyMetadata::CKeyMetadata(int64_t nCreateTime_)
{
    nVersion    = CKeyMetadata::CURRENT_VERSION;
    nCreateTime = nCreateTime_;
}

void CKeyMetadata::SetNull()
{
    nVersion    = CKeyMetadata::CURRENT_VERSION;
    nCreateTime = 0;
}

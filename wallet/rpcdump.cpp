// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "bitcoinrpc.h"
#include "blockindex.h"
#include "init.h" // for pwalletMain
#include "txdb.h"
#include "ui_interface.h"
#include "wallet_interface.h"
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/variant/get.hpp>
#include <fstream>
#include <iostream>

#include <openssl/md5.h>

using namespace json_spirit;
using namespace std;

void EnsureWalletIsUnlocked();

namespace bt = boost::posix_time;

// Extended DecodeDumpTime implementation, see this page for details:
// http://stackoverflow.com/questions/3786201/parsing-of-date-time-from-string-boost
const std::locale formats[] = {
    std::locale(std::locale::classic(), new bt::time_input_facet("%Y-%m-%dT%H:%M:%SZ")),
    std::locale(std::locale::classic(), new bt::time_input_facet("%Y-%m-%d %H:%M:%S")),
    std::locale(std::locale::classic(), new bt::time_input_facet("%Y/%m/%d %H:%M:%S")),
    std::locale(std::locale::classic(), new bt::time_input_facet("%d.%m.%Y %H:%M:%S")),
    std::locale(std::locale::classic(), new bt::time_input_facet("%Y-%m-%d"))};

const size_t formats_n = sizeof(formats) / sizeof(formats[0]);

std::time_t pt_to_time_t(const bt::ptime& pt)
{
    bt::ptime         timet_start(boost::gregorian::date(1970, 1, 1));
    bt::time_duration diff = pt - timet_start;
    return diff.ticks() / bt::time_duration::rep_type::ticks_per_second;
}

int64_t DecodeDumpTime(const std::string& s)
{
    bt::ptime pt;

    for (size_t i = 0; i < formats_n; ++i) {
        std::istringstream is(s);
        is.imbue(formats[i]);
        is >> pt;
        if (pt != bt::ptime())
            break;
    }

    return pt_to_time_t(pt);
}

std::string static EncodeDumpTime(int64_t nTime)
{
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

std::string static EncodeDumpString(const std::string& str)
{
    std::stringstream ret;
    BOOST_FOREACH (unsigned char c, str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string& str)
{
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos + 2 < str.length()) {
            c = (((str[pos + 1] >> 6) * 9 + ((str[pos + 1] - '0') & 15)) << 4) |
                ((str[pos + 2] >> 6) * 9 + ((str[pos + 2] - '0') & 15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

class CTxDump
{
public:
    CBlockIndex* pindex;
    int64_t      nValue;
    bool         fSpent;
    CWalletTx*   ptx;
    int          nOut;
    CTxDump(CWalletTx* ptxP = NULL, int nOutP = -1)
    {
        pindex     = NULL;
        nValue     = 0;
        fSpent     = false;
        this->ptx  = ptxP;
        this->nOut = nOutP;
    }
};

Value importprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("importprivkey <neblioprivkey> [label]\n"
                            "Adds a private key (as returned by dumpprivkey) to your wallet.");

    string strSecret = params[0].get_str();
    string strLabel  = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();
    CBitcoinSecret vchSecret;
    bool           fGood = vchSecret.SetString(strSecret);

    if (!fGood)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    if (fWalletUnlockStakingOnly)
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");

    const CTxDB txdb;

    CKey    key;
    bool    fCompressed;
    CSecret secret = vchSecret.GetSecret(fCompressed);
    key.SetSecret(secret, fCompressed);
    CKeyID vchAddress = key.GetPubKey().GetID();
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBookEntry(vchAddress, strLabel);

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress))
            return Value::null;

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwalletMain->AddKey(key))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'

        pwalletMain->ScanForWalletTransactions(boost::atomic_load(&pindexGenesisBlock).get(), true);
        pwalletMain->ReacceptWalletTransactions(txdb);
    }

    return Value::null;
}

Value importwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("importwallet <filename>\n"
                            "Imports keys from a wallet dump file (see dumpwallet).");

    EnsureWalletIsUnlocked();

    ifstream file;
    file.open(params[0].get_str().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    const CTxDB txdb;

    int64_t nTimeBegin = txdb.GetBestBlockIndex()->nTime;

    bool fGood = true;

    while (file.good()) {
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        boost::split(vstr, line, boost::is_any_of(" "));
        if (vstr.size() < 2)
            continue;
        CBitcoinSecret vchSecret;
        if (!vchSecret.SetString(vstr[0]))
            continue;

        bool    fCompressed;
        CKey    key;
        CSecret secret = vchSecret.GetSecret(fCompressed);
        key.SetSecret(secret, fCompressed);
        CKeyID keyid = key.GetPubKey().GetID();

        if (pwalletMain->HaveKey(keyid)) {
            NLog.write(b_sev::info, "Skipping import of {} (key already present)\n",
                       CBitcoinAddress(keyid).ToString());
            continue;
        }
        int64_t     nTime = DecodeDumpTime(vstr[1]);
        std::string strLabel;
        bool        fLabel = true;
        for (unsigned int nStr = 2; nStr < vstr.size(); nStr++) {
            if (boost::algorithm::starts_with(vstr[nStr], "#"))
                break;
            if (vstr[nStr] == "change=1")
                fLabel = false;
            if (vstr[nStr] == "reserve=1")
                fLabel = false;
            if (boost::algorithm::starts_with(vstr[nStr], "label=")) {
                strLabel = DecodeDumpString(vstr[nStr].substr(6));
                fLabel   = true;
            }
        }
        NLog.write(b_sev::info, "Importing {}...", CBitcoinAddress(keyid).ToString());
        if (!pwalletMain->AddKey(key)) {
            fGood = false;
            continue;
        }
        pwalletMain->mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            pwalletMain->SetAddressBookEntry(keyid, strLabel);
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();

    auto bestBlockIndex = txdb.GetBestBlockIndex();

    boost::optional<CBlockIndex> pindex = *bestBlockIndex;
    while (pindex && pindex->getPrev(txdb) && pindex->nTime > nTimeBegin - 7200)
        pindex = pindex->getPrev(txdb);

    if (!pwalletMain->nTimeFirstKey || nTimeBegin < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = nTimeBegin;

    NLog.write(b_sev::info, "Rescanning last {} blocks", bestBlockIndex->nHeight - pindex->nHeight + 1);
    pwalletMain->ScanForWalletTransactions(&*pindex);
    pwalletMain->ReacceptWalletTransactions(txdb);
    pwalletMain->MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys to wallet");

    return Value::null;
}

Value dumpprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("dumpprivkey <neblioaddress>\n"
                            "Reveals the private key corresponding to <neblioaddress>.");

    EnsureWalletIsUnlocked();

    string          strAddress = params[0].get_str();
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");
    if (fWalletUnlockStakingOnly)
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");
    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    CSecret vchSecret;
    bool    fCompressed;
    if (!pwalletMain->GetSecret(keyID, vchSecret, fCompressed))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    return CBitcoinSecret(vchSecret, fCompressed).ToString();
}

Value dumppubkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("dumppubkey <neblioaddress>\n"
                            "Reveals the public key corresponding to <neblioaddress>.");

    EnsureWalletIsUnlocked();

    string          strAddress = params[0].get_str();
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid neblio address");
    if (fWalletUnlockStakingOnly)
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");
    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    CPubKey vchPubKey;
    if (!pwalletMain->GetPubKey(keyID, vchPubKey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Public key for address " + strAddress + " is not known");
    const auto pubKeyVec = vchPubKey.Raw();
    return HexStr(std::make_move_iterator(pubKeyVec.begin()), std::make_move_iterator(pubKeyVec.end()));
}

Value dumpwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("dumpwallet <filename>\n"
                            "Dumps all wallet keys in a human-readable format.");

    EnsureWalletIsUnlocked();

    boost::filesystem::path filepath = params[0].get_str();

    boost::filesystem::ofstream file;
    file.open(filepath.c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    const CTxDB txdb;

    std::map<CKeyID, int64_t> mapKeyBirth;

    std::set<CKeyID> setKeyPool;

    pwalletMain->GetKeyBirthTimes(txdb, mapKeyBirth);

    pwalletMain->GetAllReserveKeys(setKeyPool);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID>> vKeyBirth;
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end();
         it++) {
        vKeyBirth.push_back(std::make_pair(it->second, it->first));
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    boost::optional<CBlockIndex> bestBlockIndex = txdb.GetBestBlockIndex();

    assert(bestBlockIndex);

    // produce output
    file << fmt::format("# Wallet dump created by neblio {} ({})\n", CLIENT_BUILD, CLIENT_DATE);
    file << fmt::format("# * Created on {}\n", EncodeDumpTime(GetTime()));
    file << fmt::format("# * Best block at time of backup was {} ({}),\n", bestBlockIndex->nHeight,
                        bestBlockIndex->GetBlockHash().ToString());
    file << fmt::format("#   mined on {}\n", EncodeDumpTime(bestBlockIndex->nTime));
    file << "\n";
    for (std::vector<std::pair<int64_t, CKeyID>>::const_iterator it = vKeyBirth.begin();
         it != vKeyBirth.end(); it++) {
        const CKeyID& keyid   = it->second;
        std::string   strTime = EncodeDumpTime(it->first);
        std::string   strAddr = CBitcoinAddress(keyid).ToString();
        bool          IsCompressed;

        CKey key;
        if (pwalletMain->GetKey(keyid, key)) {
            if (auto entry = pwalletMain->mapAddressBook.get(keyid)) {
                CSecret secret = key.GetSecret(IsCompressed);
                file << fmt::format("{} {} label={} # addr={}\n",
                                    CBitcoinSecret(secret, IsCompressed).ToString(), strTime,
                                    EncodeDumpString(entry->name), strAddr);
            } else if (setKeyPool.count(keyid)) {
                CSecret secret = key.GetSecret(IsCompressed);
                file << fmt::format("{} {} reserve=1 # addr={}\n",
                                    CBitcoinSecret(secret, IsCompressed).ToString(), strTime, strAddr);
            } else {
                CSecret secret = key.GetSecret(IsCompressed);
                file << fmt::format("{} {} change=1 # addr={}\n",
                                    CBitcoinSecret(secret, IsCompressed).ToString(), strTime, strAddr);
            }
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();

    Object reply;
    reply.push_back(Pair("filename", filepath.string()));

    return reply;
}


Value importaddress(const Array& params, bool fHelp) {

    if (fHelp || params.size() > 1)
        throw std::runtime_error(
                "importaddress,"
                "\nAdds an address or script (in hex) that can be watched as if it were "
                "in your wallet but cannot be used to spend. Requires a new wallet backup.\n"
                "\nNote: This call can take over an hour to complete if rescan is true, during that time, other rpc calls\n"
                "may report that the imported address exists but related transactions are still missing, "
                "leading to temporarily incorrect/bogus balances and unspent outputs until rescan completes.\n");

    if (!pwalletMain)
        throw JSONRPCError(RPC_WALLET_ERROR, "There is no active wallet");

    CWallet* const pwallet = pwalletMain.get();

    std::string strLabel;
    if (!params[1].is_null())
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (!params[2].is_null())
        fRescan = params[2].get_bool();

    bool rescanInProgress = false;
    if (fRescan && rescanInProgress) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

    bool fP2SH = false;
    if (!params[3].is_null())
        fP2SH = params[3].get_bool();

    std::string strAddress = params[0].get_str();
    CBitcoinAddress addr(strAddress);
    CTxDestination dest = addr.Get();
    CTxDB db;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        if (dest.which() != 0) {
            if (fP2SH)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot use the p2sh flag with an address - use a script instead");

            pwallet->MarkDirty();
            pwallet->ImportScriptPubKeys(strLabel, {GetScriptForDestination(dest)}, /*have_solving_data=*/false, /*apply_label=*/true, /*timestamp=*/1);
        } else if (IsHex(strAddress)) {
            std::vector<unsigned char> data(ParseHex(strAddress));
            CScript redeemScript(data.begin(), data.end());

            std::set<CScript> scripts = {redeemScript};
            pwallet->ImportScripts(scripts, /*timestamp=*/0);

            if (fP2SH) {
                CTxDestination scriptDes;
                ExtractDestination(db, redeemScript, scriptDes);
                scripts.insert(GetScriptForDestination(scriptDes));
            }

            pwallet->ImportScriptPubKeys(strLabel, scripts, /*have_solving_data=*/false, /*apply_label=*/true, /*timestamp=*/1);
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Neblio address or script");
        }
    }

    if (fRescan)
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        pwallet->ReacceptWalletTransactions(db);
    }

    return Object{};
}


void _RescanBlockchain(int64_t earliestTime, const ITxDB& txdb)
{
    auto bestBlockIndex = txdb.GetBestBlockIndex();

    boost::optional<CBlockIndex> pindex = *bestBlockIndex;
    while (pindex && pindex->getPrev(txdb) && pindex->nTime > earliestTime - 7200)
        pindex = pindex->getPrev(txdb);

    if (!pwalletMain->nTimeFirstKey || earliestTime < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = earliestTime;

    NLog.write(b_sev::info, "Rescanning last {} blocks", bestBlockIndex->nHeight - pindex->nHeight + 1);
    pwalletMain->ScanForWalletTransactions(&*pindex);
    pwalletMain->ReacceptWalletTransactions(txdb);
    pwalletMain->MarkDirty();
}

bool _AddKeyToLocalWallet(const CKey& Key, const std::string& strLabel, int64_t KeyCreationTime,
                          int64_t& earliestTime, bool addInAddressBook)
{
    CKeyID keyid = Key.GetPubKey().GetID();
    NLog.write(b_sev::info, "Importing {}...", CBitcoinAddress(keyid).ToString());

    // if key exists already in the local wallet, don't add it
    if (pwalletMain->HaveKey(keyid)) {
        return false;
    }

    // attempt to add the key
    if (!pwalletMain->AddKey(Key)) {
        return false;
    }

    // set key creation time, in order to reset the blockchain to that time eventually
    pwalletMain->mapKeyMetadata[keyid].nCreateTime = KeyCreationTime;
    if (addInAddressBook) {
        pwalletMain->SetAddressBookEntry(keyid, strLabel);
    }
    earliestTime = std::min(earliestTime, KeyCreationTime);
    return true;
}

CBitcoinAddress GetAddressFromKey(const CKey& key)
{
    CBitcoinAddress a;
    a.Set(key.GetPubKey().GetID());
    return a;
}

/**
 * Tells whether the wallet in Src path is encrypted
 * @brief IsWalletEncrypted
 * @param Src
 * @return true if encrypted, falls otherwise
 */
bool IsWalletEncrypted(const std::string& Src)
{
    CWallet walletObj(Src);
    bool    firstRun = true;
    walletObj.LoadWallet(firstRun);
    return walletObj.IsCrypted();
}

/**
 * This function imports a wallet located in the path Src so the default wallet in the program
 * The passphrase is ignored in case the wallet is not encrypted.
 * @brief ImportBackupWallet
 * @param Src path to the wallet
 * @param PassPhrase
 * @param importReserveToAddressBook, if enabled, all reserve keys from the backup wallet will be
 * imported into the address book
 * @return A pair of two numbers, the second number is the total number of keys in the backup wallet, and
 * the first is the number of successfully added keys
 */
std::pair<long, long> ImportBackupWallet(const std::string& Src, std::string& PassPhrase,
                                         bool importReserveToAddressBook)
{
    const CTxDB txdb;

    if (pwalletMain->IsLocked()) {
        throw std::logic_error("Please unlock the wallet before importing.");
    }
    if (fWalletUnlockStakingOnly) {
        throw std::logic_error(
            "Please unlock the wallet before importing; unlocking should NOT be for staking only.");
    }

    std::pair<long, long> succeessfullyAddedOutOfTotal = std::make_pair<long, long>(0, 0);
    CWallet               backupWallet(Src);
    bool                  firstRun = true;
    backupWallet.LoadWallet(firstRun);
    bool         isEncrypted = backupWallet.IsCrypted();
    SecureString pass;
    // TODO: Move this 1024 to MAX_PASSPHRASE_SIZE
    pass.reserve(1024);
    if (isEncrypted) {
        pass.assign(PassPhrase.c_str());
        PassPhrase.clear();
        bool unlockSuccess = backupWallet.Unlock(pass);
        if (!unlockSuccess) {
            throw std::runtime_error("Unable to unlock backup wallet. Invalid passphrase.");
        }
    }

    // earliest time to rescan the blockchain
    int64_t earliestTime = CTxDB().GetBestBlockIndex()->nTime;

    std::set<CKeyID> allKeyIDsSet;
    backupWallet.GetKeys(allKeyIDsSet);
    // deque to simply elements access
    const std::deque<CKeyID> allKeyIDs(allKeyIDsSet.begin(), allKeyIDsSet.end());
    using AddressBookIt = std::map<CTxDestination, AddressBook::CAddressBookData>::const_iterator;
    const std::map<CTxDestination, AddressBook::CAddressBookData> addrBook =
        backupWallet.mapAddressBook.getInternalMap();

    // set total number of keys
    succeessfullyAddedOutOfTotal.second = allKeyIDs.size();

    // import address book keys
    for (long i = 0; i < static_cast<long>(allKeyIDs.size()); i++) {
        AddressBookIt it                    = addrBook.find(allKeyIDs[i]);
        bool          foundKeyInAddressBook = (it != addrBook.end());

        // retrieve key using key ID
        CKey key;
        bool getKeySucceeded = backupWallet.GetKey(allKeyIDs[i], key);
        if (!getKeySucceeded)
            continue;

        // add the key, whether to the address book or simply to reserve
        if (foundKeyInAddressBook) {
            // import from address book
            bool addSucceeded = _AddKeyToLocalWallet(
                key, it->second.name,
                backupWallet.mapKeyMetadata.at(boost::get<CKeyID>(it->first)).nCreateTime, earliestTime,
                true);
            if (addSucceeded)
                succeessfullyAddedOutOfTotal.first++;
        } else {
            // import reserve keys
            bool addSucceeded =
                _AddKeyToLocalWallet(key, "", backupWallet.mapKeyMetadata.at(allKeyIDs[i]).nCreateTime,
                                     earliestTime, importReserveToAddressBook);
            if (addSucceeded)
                succeessfullyAddedOutOfTotal.first++;
        }
    }
    _RescanBlockchain(earliestTime, txdb);
    return succeessfullyAddedOutOfTotal;
}

std::string MD5FromString(const std::string& str)
{
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)str.c_str(), str.size(), result);

    std::ostringstream sout;
    sout << std::hex << std::setfill('0');
    for (long long i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sout << std::setw(2) << (long long)result[i];
    }
    return sout.str();
}

bool WriteStringToFile(const boost::filesystem::path& filepath, const std::string& strToWrite)
{
    if (boost::filesystem::exists(filepath)) {
        boost::filesystem::remove(filepath);
    }
    boost::filesystem::fstream file(filepath, std::ios::out);
    if (!file.good()) {
        return false;
    }
    file << strToWrite;
    file.close();
    return true;
}

std::string ReadStringFromFile(const boost::filesystem::path& filepath)
{
    if (!boost::filesystem::exists(filepath)) {
        return std::string();
    }
    boost::filesystem::fstream file(filepath, std::ios::in);
    std::string                result;
    file >> result;
    file.close();
    return result;
}

std::string GetCurrentWalletHash()
{
    std::set<CKeyID> allKeyIDsSet;
    pwalletMain->GetKeys(allKeyIDsSet);
    // deque to simply elements access
    const std::deque<CKeyID> allKeyIDs(allKeyIDsSet.begin(), allKeyIDsSet.end());

    // concatenate all public keys into one string
    std::string finalStringToHash;
    for (long i = 0; i < static_cast<long>(allKeyIDs.size()); i++) {
        // retrieve key using key ID
        CKey key;
        bool getKeySucceeded = pwalletMain->GetKey(allKeyIDs[i], key);
        if (!getKeySucceeded) {
            NLog.write(b_sev::err, "Failed to get key number {}", i);
            continue;
        }
        finalStringToHash += key.GetPubKey().GetHash().ToString();
    }
    if (finalStringToHash.empty()) {
        throw std::runtime_error("Error: Backup public keys hash is empty. This should not happen.");
    }
    // calculate the MD5 hash of all public keys and return it
    std::string theHash = MD5FromString(finalStringToHash);
    return theHash;
}

void WriteWalletBackupHash()
{
    // if the hash file doesn't exist, then the wallet was never backed-up
    boost::filesystem::path BackupHashFilePath = GetDataDir() / CWallet::BackupHashFilename;
    if (!boost::filesystem::exists(BackupHashFilePath)) {
        boost::filesystem::remove(BackupHashFilePath);
    }

    // if the wallet is not accessible
    if (pwalletMain == NULL) {
        throw std::runtime_error("Wallet pointer is NULL. Can't write backup hash.");
    }

    // if the wallet is locked
    if (pwalletMain->IsLocked()) {
        throw std::runtime_error("Can't backup wallets that are locked.");
    }

    std::string finalHash = GetCurrentWalletHash();
    if (!WriteStringToFile(BackupHashFilePath, finalHash)) {
        throw std::runtime_error("Writing wallet backup data failed.");
    }
}

bool ShouldWalletBeBackedUp()
{
    boost::filesystem::path BackupHashFilePath = GetDataDir() / CWallet::BackupHashFilename;

    // if the hash file doesn't exist, then the wallet was never backed-up
    if (!boost::filesystem::exists(BackupHashFilePath)) {
        return true;
    }

    // if the wallet is not accessible, just ignore checking
    if (pwalletMain == NULL) {
        return false;
    }

    // if the wallet is locked, just ignore checking
    if (pwalletMain->IsLocked()) {
        return false;
    }

    std::string finalHash  = GetCurrentWalletHash();
    std::string storedHash = ReadStringFromFile(BackupHashFilePath);
    if (storedHash == finalHash) {
        return false;
    } else {
        return true;
    }
}

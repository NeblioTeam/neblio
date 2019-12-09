// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <fstream>
#include <iostream>

#include "base58.h"
#include "bitcoinrpc.h"
#include "init.h" // for pwalletMain
#include "main.h"
#include "ui_interface.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/variant/get.hpp>

#include <openssl/md5.h>

#define printf OutputDebugStringF

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
    CTxDump(CWalletTx* ptx = NULL, int nOut = -1)
    {
        pindex     = NULL;
        nValue     = 0;
        fSpent     = false;
        this->ptx  = ptx;
        this->nOut = nOut;
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

    CKey    key;
    bool    fCompressed;
    CSecret secret = vchSecret.GetSecret(fCompressed);
    key.SetSecret(secret, fCompressed);
    CKeyID vchAddress = key.GetPubKey().GetID();
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBookName(vchAddress, strLabel);

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress))
            return Value::null;

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwalletMain->AddKey(key))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'

        pwalletMain->ScanForWalletTransactions(boost::atomic_load(&pindexGenesisBlock).get(), true);
        pwalletMain->ReacceptWalletTransactions();
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

    int64_t nTimeBegin = boost::atomic_load(&pindexBest)->nTime;

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
            printf("Skipping import of %s (key already present)\n",
                   CBitcoinAddress(keyid).ToString().c_str());
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
        printf("Importing %s...\n", CBitcoinAddress(keyid).ToString().c_str());
        if (!pwalletMain->AddKey(key)) {
            fGood = false;
            continue;
        }
        pwalletMain->mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            pwalletMain->SetAddressBookName(keyid, strLabel);
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();

    CBlockIndexSmartPtr pindex = pindexBest;
    while (pindex && pindex->pprev && pindex->nTime > nTimeBegin - 7200)
        pindex = pindex->pprev;

    if (!pwalletMain->nTimeFirstKey || nTimeBegin < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = nTimeBegin;

    printf("Rescanning last %i blocks\n",
           boost::atomic_load(&pindexBest)->nHeight - pindex->nHeight + 1);
    pwalletMain->ScanForWalletTransactions(pindex.get());
    pwalletMain->ReacceptWalletTransactions();
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

Value dumpwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("dumpwallet <filename>\n"
                            "Dumps all wallet keys in a human-readable format.");

    EnsureWalletIsUnlocked();

    ofstream file;
    file.open(params[0].get_str().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CKeyID, int64_t> mapKeyBirth;

    std::set<CKeyID> setKeyPool;

    pwalletMain->GetKeyBirthTimes(mapKeyBirth);

    pwalletMain->GetAllReserveKeys(setKeyPool);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID>> vKeyBirth;
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end();
         it++) {
        vKeyBirth.push_back(std::make_pair(it->second, it->first));
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by neblio %s (%s)\n", CLIENT_BUILD.c_str(),
                      CLIENT_DATE.c_str());
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()).c_str());
    file << strprintf("# * Best block at time of backup was %i (%s),\n", nBestHeight.load(),
                      hashBestChain.ToString().c_str());
    file << strprintf("#   mined on %s\n",
                      EncodeDumpTime(boost::atomic_load(&pindexBest)->nTime).c_str());
    file << "\n";
    for (std::vector<std::pair<int64_t, CKeyID>>::const_iterator it = vKeyBirth.begin();
         it != vKeyBirth.end(); it++) {
        const CKeyID& keyid   = it->second;
        std::string   strTime = EncodeDumpTime(it->first);
        std::string   strAddr = CBitcoinAddress(keyid).ToString();
        bool          IsCompressed;

        CKey key;
        if (pwalletMain->GetKey(keyid, key)) {
            if (pwalletMain->mapAddressBook.count(keyid)) {
                CSecret secret = key.GetSecret(IsCompressed);
                file << strprintf(
                    "%s %s label=%s # addr=%s\n",
                    CBitcoinSecret(secret, IsCompressed).ToString().c_str(), strTime.c_str(),
                    EncodeDumpString(pwalletMain->mapAddressBook[keyid]).c_str(), strAddr.c_str());
            } else if (setKeyPool.count(keyid)) {
                CSecret secret = key.GetSecret(IsCompressed);
                file << strprintf("%s %s reserve=1 # addr=%s\n",
                                  CBitcoinSecret(secret, IsCompressed).ToString().c_str(),
                                  strTime.c_str(), strAddr.c_str());
            } else {
                CSecret secret = key.GetSecret(IsCompressed);
                file << strprintf("%s %s change=1 # addr=%s\n",
                                  CBitcoinSecret(secret, IsCompressed).ToString().c_str(),
                                  strTime.c_str(), strAddr.c_str());
            }
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();
    return Value::null;
}

void _RescanBlockchain(int64_t earliestTime)
{
    CBlockIndexSmartPtr pindex = boost::atomic_load(&pindexBest);
    while (pindex && pindex->pprev && pindex->nTime > earliestTime - 7200)
        pindex = pindex->pprev;

    if (!pwalletMain->nTimeFirstKey || earliestTime < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = earliestTime;

    printf("Rescanning last %i blocks\n",
           boost::atomic_load(&pindexBest)->nHeight - pindex->nHeight + 1);
    pwalletMain->ScanForWalletTransactions(pindex.get());
    pwalletMain->ReacceptWalletTransactions();
    pwalletMain->MarkDirty();
}

bool _AddKeyToLocalWallet(const CKey& Key, const std::string& strLabel, int64_t KeyCreationTime,
                          int64_t& earliestTime, bool addInAddressBook)
{
    CKeyID keyid = Key.GetPubKey().GetID();
    printf("Importing %s...\n", CBitcoinAddress(keyid).ToString().c_str());

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
        pwalletMain->SetAddressBookName(keyid, strLabel);
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
    int64_t earliestTime = boost::atomic_load(&pindexBest)->nTime;

    std::set<CKeyID> allKeyIDsSet;
    backupWallet.GetKeys(allKeyIDsSet);
    // deque to simply elements access
    const std::deque<CKeyID> allKeyIDs(allKeyIDsSet.begin(), allKeyIDsSet.end());
    typedef std::map<CTxDestination, std::string>::const_iterator AddressBookIt;
    std::map<CTxDestination, std::string>&                        addrBook = backupWallet.mapAddressBook;

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
                key, it->second, backupWallet.mapKeyMetadata[boost::get<CKeyID>(it->first)].nCreateTime,
                earliestTime, true);
            if (addSucceeded)
                succeessfullyAddedOutOfTotal.first++;
        } else {
            // import reserve keys
            bool addSucceeded =
                _AddKeyToLocalWallet(key, "", backupWallet.mapKeyMetadata[allKeyIDs[i]].nCreateTime,
                                     earliestTime, importReserveToAddressBook);
            if (addSucceeded)
                succeessfullyAddedOutOfTotal.first++;
        }
    }
    _RescanBlockchain(earliestTime);
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
            printf("Failed to get key number %ld", i);
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

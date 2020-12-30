// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"
#include "base58.h"
#include "block.h"
#include "coincontrol.h"
#include "crypter.h"
#include "kernel.h"
#include "main.h"
#include "ntp1/ntp1transaction.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "walletdb.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>

using namespace std;

const boost::filesystem::path CWallet::BackupHashFilename = "wallet-hash.txt";

//////////////////////////////////////////////////////////////////////////////
//
// mapWallet
//

struct CompareValueOnly
{
    bool operator()(const pair<CAmount, pair<const CWalletTx*, unsigned int>>& t1,
                    const pair<CAmount, pair<const CWalletTx*, unsigned int>>& t2) const
    {
        return t1.first < t2.first;
    }
};

CBitcoinAddress CWallet::getNewAddress(const std::string& label)
{
    return getNewAddress(label, AddressBook::AddressBookPurpose::RECEIVE);
}

CBitcoinAddress CWallet::getNewStakingAddress(const std::string& label)
{
    return getNewAddress(label, AddressBook::AddressBookPurpose::COLD_STAKING);
}

CBitcoinAddress CWallet::getNewAddress(const std::string& addressLabel, const string& purpose)
{
    LOCK2(cs_main, cs_wallet);

    // Refill keypool if wallet is unlocked
    if (!IsLocked())
        TopUpKeyPool();

    CPubKey newKey;
    // Get a key
    if (!GetKeyFromPool(newKey)) {
        // inform the user to top-up the keypool or unlock the wallet
        throw std::runtime_error(
            "Keypool ran out, please call keypoolrefill first, or unlock the wallet.");
    }
    CKeyID keyID = newKey.GetID();

    if (!SetAddressBookEntry(keyID, addressLabel, purpose))
        throw std::runtime_error("CWallet::getNewAddress() : SetAddressBook failed");

    return CBitcoinAddress(keyID);
}

CPubKey CWallet::GenerateNewKey()
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(
        FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = key.GetPubKey();

    // Create new metadata
    int64_t nCreationTime          = GetTime();
    mapKeyMetadata[pubkey.GetID()] = CKeyMetadata(nCreationTime);
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    if (!AddKey(key))
        throw std::runtime_error("CWallet::GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool CWallet::AddKey(const CKey& key)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata

    CPubKey pubkey = key.GetPubKey();

    if (!CCryptoKeyStore::AddKey(key))
        return false;
    if (!fFileBacked)
        return true;
    if (!IsCrypted())
        return CWalletDB(strWalletFile)
            .WriteKey(pubkey, key.GetPrivKey(), mapKeyMetadata[pubkey.GetID()]);
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey& vchPubKey, const vector<unsigned char>& vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey, vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile)
                .WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}

bool CWallet::LoadKeyMetadata(const CPubKey& pubkey, const CKeyMetadata& meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadMinVersion(int nVersion)
{
    AssertLockHeld(cs_wallet);
    nWalletVersion    = nVersion;
    nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion);
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey&                    vchPubKey,
                             const std::vector<unsigned char>& vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

// optional setting to unlock wallet for staking only
// serves to disable the trivial sendmoney when OS account compromised
// provides no real security
bool fWalletUnlockStakingOnly = false;

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        std::string strAddr = CBitcoinAddress(redeemScript.GetID()).ToString();
        printf("%s: Warning: This wallet contains a redeemScript of size %" PRIszu
               " which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
               __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr.c_str());
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    if (!IsLocked())
        return false;

    CCrypter        crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        for (const MasterKeyMap::value_type& pMasterKey : mapMasterKeys) {
            if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt,
                                              pMasterKey.second.nDeriveIterations,
                                              pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
                return true;
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase,
                                     const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter        crypter;
        CKeyingMaterial vMasterKey;
        for (MasterKeyMap::value_type& pMasterKey : mapMasterKeys) {
            if (!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt,
                                              pMasterKey.second.nDeriveIterations,
                                              pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey)) {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt,
                                             pMasterKey.second.nDeriveIterations,
                                             pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations *
                                                      (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt,
                                             pMasterKey.second.nDeriveIterations,
                                             pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations +
                                                       pMasterKey.second.nDeriveIterations * 100 /
                                                           ((double)(GetTimeMillis() - nStartTime))) /
                                                      2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                printf("Wallet passphrase changed to an nDeriveIterations of %i\n",
                       pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt,
                                                  pMasterKey.second.nDeriveIterations,
                                                  pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    if (!walletdb.WriteBestBlock(loc))
        printf("Failed to write best chain to wallet at: %s\n",
               boost::atomic_load(&pindexBest).get()->phashBlock->ToString().c_str());
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
        nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked) {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

int CWallet::GetVersion()
{
    LOCK(cs_wallet);
    return nWalletVersion;
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;
    RandAddSeedPerfmon();

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    RAND_bytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey(nDerivationMethodIndex);

    RandAddSeedPerfmon();
    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    RAND_bytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t  nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000,
                                 kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations,
                                 kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations =
        (kMasterKey.nDeriveIterations +
         kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) /
        2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    printf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt,
                                      kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked) {
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin())
                return false;
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey)) {
            if (fFileBacked)
                pwalletdbEncryption->TxnAbort();
            exit(1); // We now probably have half of our keys encrypted in memory, and half not...die and
                     // let the user reload their unencrypted wallet.
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked) {
            if (!pwalletdbEncryption->TxnCommit())
                exit(1); // We now have keys encrypted in memory, but no on disk...die to avoid confusion
                         // and let the user reload their unencrypted wallet.

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);
    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB* pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

CWallet::TxItems CWallet::OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount)
{
    AssertLockHeld(cs_wallet); // mapWallet
    CWalletDB walletdb(strWalletFile);

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-order multimap.
    TxItems txOrdered;

    // Note: maintaining indices in the database of (account,time) --> txid and (account, time) -->
    // acentry would make this much faster for applications that do this a lot.
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
        CWalletTx* wtx = &((*it).second);
        txOrdered.insert(make_pair(wtx->nOrderPos, TxPair(wtx, (CAccountingEntry*)0)));
    }
    acentries.clear();
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    for (CAccountingEntry& entry : acentries) {
        txOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));
    }

    return txOrdered;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for (PAIRTYPE(const uint256, CWalletTx) & item : mapWallet)
            item.second.MarkDirty();
    }
}

unsigned int CWallet::ComputeTimeSmart(const CWalletTx& wtx) const
{
    unsigned int nTimeSmart = wtx.nTimeReceived;
    if (wtx.hashBlock != 0) {
        if (mapBlockIndex.count(wtx.hashBlock)) {
            int64_t latestNow   = wtx.nTimeReceived;
            int64_t latestEntry = 0;
            {
                // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the
                // future
                int64_t latestTolerated = latestNow + 300;
                TxItems txOrdered       = wtxOrdered;
                for (TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
                    CWalletTx* const pwtx = (*it).second.first;
                    if (pwtx == &wtx)
                        continue;
                    CAccountingEntry* const pacentry = (*it).second.second;
                    int64_t                 nSmartTime;
                    if (pwtx) {
                        nSmartTime = pwtx->nTimeSmart;
                        if (!nSmartTime)
                            nSmartTime = pwtx->nTimeReceived;
                    } else
                        nSmartTime = pacentry->nTime;
                    if (nSmartTime <= latestTolerated) {
                        latestEntry = nSmartTime;
                        if (nSmartTime > latestNow)
                            latestNow = nSmartTime;
                        break;
                    }
                }
            }

            int64_t blocktime = mapBlockIndex[wtx.hashBlock]->GetBlockTime();
            nTimeSmart        = std::max(latestEntry, std::min(blocktime, latestNow));
        } else
            printf("AddToWallet() : found %s in block %s not in index\n",
                   wtx.GetHash().ToString().c_str(), wtx.hashBlock.ToString().c_str());
    }
    return nTimeSmart;
}

void CWallet::SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:

    int              nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom     = nullptr;
    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
        const CWalletTx* wtx = &mapWallet.at(it->second);
        int              n   = wtx->nOrderPos;
        if (n < nMinOrderPos) {
            nMinOrderPos = n;
            copyFrom     = wtx;
        }
    }

    if (!copyFrom) {
        return;
    }

    // Now copy data from copyFrom to rest:
    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
        const uint256& hash   = it->second;
        CWalletTx*     copyTo = &mapWallet[hash];
        if (copyFrom == copyTo)
            continue;
        assert(copyFrom && "Oldest wallet transaction in range assumed to have been found.");
        // if (!copyFrom->IsEquivalentTo(*copyTo)) continue;
        copyTo->mapValue   = copyFrom->mapValue;
        copyTo->vOrderForm = copyFrom->vOrderForm;
        // fTimeReceivedIsTxTime not copied on purpose
        // nTimeReceived not copied on purpose
        copyTo->nTimeSmart     = copyFrom->nTimeSmart;
        copyTo->fFromMe        = copyFrom->fFromMe;
        copyTo->strFromAccount = copyFrom->strFromAccount;
        // nOrderPos not copied on purpose
        // cached members not copied on purpose
    }
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return nullptr;
    return &(it->second);
}

void CWallet::MarkConflicted(const uint256& hashBlock, const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    const std::string bestblock = pindexBest->phashBlock->ToString();
    const std::string bh        = hashBlock.ToString();

    CBlockIndex* pindex;
    assert(mapBlockIndex.count(hashBlock));
    pindex               = mapBlockIndex.at(hashBlock).get();
    int conflictconfirms = 0;
    if (pindex->IsInMainChain()) {
        conflictconfirms = -(nBestHeight - pindex->nHeight + 1);
    }
    //    assert(conflictconfirms < 0);
    if (conflictconfirms >= 0)
        return;

    // Do not flush the wallet here for performance reasons
    CWalletDB walletdb(strWalletFile, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        assert(mapWallet.count(now));
        CWalletTx& wtx            = mapWallet[now];
        int        currentconfirm = wtx.GetDepthInMainChain();
        if (conflictconfirms < currentconfirm) {
            // Block is 'more conflicted' than current confirm; update.
            // Mark transaction as conflicted with this block.
            wtx.nIndex    = -1;
            wtx.hashBlock = hashBlock;
            wtx.MarkDirty();
            wtx.WriteToDisk(&walletdb);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them
            // conflicted too
            auto                     txSpends = mapTxSpends.get();
            TxSpends::const_iterator iter     = txSpends.lower_bound(COutPoint(now, 0));
            while (iter != txSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin : wtx.vin) {
                if (mapWallet.count(txin.prevout.hash))
                    mapWallet[txin.prevout.hash].MarkDirty();
            }
        }
    }
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    auto lock = mapTxSpends.get_lock();
    mapTxSpends.get_unsafe().insert(std::make_pair(outpoint, wtxid));
    //    setLockedCoins.erase(outpoint);

    std::pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.get_unsafe().equal_range(outpoint);
    SyncMetaData(range);
}

void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet.at(wtxid);
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    for (const CTxIn& txin : thisTx.vin)
        AddToSpends(txin.prevout, wtxid);
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb)
{
    uint256 hash = wtxIn.GetHash();

    // update NTP1 transactions
    if (walletNewTxUpdateFunctor) {
        walletNewTxUpdateFunctor->setReferenceBlockHeight();
        walletNewTxUpdateFunctor->run(hash, nBestHeight);
    }

    if (fFromLoadWallet) {
        mapWallet[hash] = wtxIn;
        CWalletTx& wtx  = mapWallet[hash];
        wtx.BindWallet(this);
        wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));
        AddToSpends(hash);
    } else {

        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx&                                    wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew) {
            if (!wtx.nTimeReceived)
                wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext(pwalletdb);
            wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));
            wtx.nTimeSmart = ComputeTimeSmart(wtx);
            AddToSpends(hash);
            for (const CTxIn& txin : wtx.vin) {
                if (mapWallet.count(txin.prevout.hash)) {
                    CWalletTx& prevtx = mapWallet[txin.prevout.hash];
                    if (prevtx.nIndex == -1 && !prevtx.hashUnset()) {
                        MarkConflicted(prevtx.hashBlock, wtx.GetHash());
                    }
                }
            }
        }

        bool fUpdated = false;
        if (!fInsertedNew) {
            // Merge
            if (!wtxIn.hashUnset() && wtxIn.hashBlock != wtx.hashBlock) {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated      = true;
            }
            // If no longer abandoned, update
            if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned()) {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated      = true;
            }
            if (wtxIn.nIndex != -1 &&
                (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex)) {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex        = wtxIn.nIndex;
                fUpdated          = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe) {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated    = true;
            }
        }

        //// debug print
        printf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString().substr(0, 10).c_str(),
               (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk(pwalletdb))
                return false;

        // since AddToWallet is called directly for self-originating transactions, check for
        // consumption of own coins
        //        WalletUpdateSpent(wtx, (wtxIn.hashBlock != 0));

        // Break debit/credit balance caches:
        wtx.MarkDirty();

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if (!strCmd.empty()) {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }
    }

    return true;
}

// Add a transaction to the wallet, or update it.
// pblock is optional, but should be provided if the transaction is known to be in a block.
// If fUpdate is true, existing transactions will be updated.
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate,
                                       bool /*fFindBlock*/)
{
    uint256 hash = tx.GetHash();
    {
        LOCK(cs_wallet);

        if (pblock && !tx.IsCoinBase()) {
            for (const CTxIn& txin : tx.vin) {
                auto        lock     = mapTxSpends.get_lock();
                const auto& txSpends = mapTxSpends.get();
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range =
                    txSpends.equal_range(txin.prevout);
                while (range.first != range.second) {
                    if (range.first->second != tx.GetHash()) {
                        printf("Transaction %s (in block %s) conflicts with wallet transaction %s "
                               "(both spend %s:%i)\n",
                               tx.GetHash().ToString().c_str(), pblock->GetHash().ToString().c_str(),
                               range.first->second.ToString().c_str(),
                               range.first->first.hash.ToString().c_str(), range.first->first.n);
                        MarkConflicted(pblock->GetHash(), range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(hash);
        if (fExisted && !fUpdate)
            return false;
        if (fExisted || IsMine(tx) || IsFromMe(tx)) {
            CWalletTx wtx(this, tx);
            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(pblock);
            // Do not flush the wallet here for performance reasons
            // this is safe, as in case of a crash, we rescan the necessary blocks on startup through our
            // SetBestChain-mechanism
            CWalletDB walletdb(strWalletFile, "r+", false);

            return AddToWallet(wtx, false, &walletdb);
        }
    }
    return false;
}

bool CWallet::EraseFromWallet(uint256 hash)
{
    if (!fFileBacked)
        return false;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
    }
    return true;
}

isminetype CWallet::IsMine(const CTxIn& txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return IsMine(prev.vout.at(txin.prevout.n));
        }
    }
    return ISMINE_NO;
}

CAmount CWallet::GetDebit(const CTxIn& txin, const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]) & filter)
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

isminetype CWallet::IsMine(const CTxOut& txout) const { return ::IsMine(*this, txout.scriptPubKey); }

CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetCredit() : value out of range");
    return ((IsMine(txout) & filter) ? txout.nValue : 0);
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    CTxDestination address;

    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a TX_PUBKEYHASH that is mine but isn't in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (ExtractDestination(txout.scriptPubKey, address) && ::IsMine(*this, address) != ISMINE_NO) {
        if (!mapAddressBook.exists(address))
            return true;
    }
    return false;
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetChange() : value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction& tx) const
{
    for (const CTxOut& txout : tx.vout)
        if (txout.nValue >= nMinimumInputValue && IsMine(txout) != ISMINE_NO)
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransaction& tx) const { return (GetDebit(tx, ISMINE_ALL) > 0); }

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nDebit = 0;
    for (const CTxIn& txin : tx.vin) {
        nDebit += GetDebit(txin, filter);
        if (!MoneyRange(nDebit))
            throw std::runtime_error("CWallet::GetDebit() : value out of range");
    }
    return nDebit;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter, const bool fUnspent) const
{
    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        if (fUnspent && IsSpent(tx.GetHash(), i))
            continue;
        nCredit += GetCredit(tx.vout[i], filter);
    }
    if (!MoneyRange(nCredit))
        throw std::runtime_error("CWallet::GetCredit() : value out of range");
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout) {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error("CWallet::GetChange() : value out of range");
    }
    return nChange;
}

std::set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    std::set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    auto lock = mapTxSpends.get_lock();
    for (const CTxIn& txin : wtx.vin) {
        if (mapTxSpends.get_unsafe().count(txin.prevout) <= 1 /* || wtx.HasZerocoinSpendInputs()*/)
            continue; // No conflict if zero or one spends
        range = mapTxSpends.get_unsafe().equal_range(txin.prevout);
        for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
            result.insert(it->second);
    }
    return result;
}

std::set<uint256> CWalletTx::GetConflicts() const
{
    std::set<uint256> result;
    if (pwallet != NULL) {
        uint256 myHash = GetHash();
        result         = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase() || IsCoinStake()) {
            // Generated block
            if (hashBlock != 0) {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        } else {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end()) {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && hashBlock != 0) {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(list<pair<CTxDestination, CAmount>>& listReceived,
                           list<pair<CTxDestination, CAmount>>& listSent, CAmount& nFee,
                           string& strSentAccount, const isminefilter& filter) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Compute fee:
    CAmount nDebit = GetDebit(filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = GetValueOut();
        nFee              = nDebit - nValueOut;
    }

    // Sent/received.
    for (const CTxOut& txout : vout) {
        // Skip special stake out
        if (txout.scriptPubKey.empty())
            continue;

        isminetype fIsMine;
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (nDebit > 0) {
            // Don't report 'change' txouts
            if (pwallet->IsChange(txout))
                continue;
            fIsMine = pwallet->IsMine(txout);
        } else if (!IsMineCheck((fIsMine = pwallet->IsMine(txout)), ISMINE_SPENDABLE))
            continue;

        // In either case, we need to get the destination address
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address)) {
            printf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                   this->GetHash().ToString().c_str());
            address = CNoDestination();
        }

        // If we are debited by the transaction, add the output as a "sent" entry
        if (nDebit > 0)
            listSent.push_back(make_pair(address, txout.nValue));

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine & filter)
            listReceived.push_back(make_pair(address, txout.nValue));
    }
}

void CWalletTx::GetAccountAmounts(const string& strAccount, CAmount& nReceived, CAmount& nSent,
                                  CAmount& nFee, const isminefilter& filter) const
{
    nReceived = nSent = nFee = 0;

    CAmount                             allFee;
    string                              strSentAccount;
    list<pair<CTxDestination, CAmount>> listReceived;
    list<pair<CTxDestination, CAmount>> listSent;
    GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);

    if (strAccount == strSentAccount) {
        for (const PAIRTYPE(CTxDestination, CAmount) & s : listSent)
            nSent += s.second;
        nFee = allFee;
    }
    for (const PAIRTYPE(CTxDestination, CAmount) & r : listReceived) {
        if (const auto entry = pwallet->mapAddressBook.get(r.first)) {
            if (entry.is_initialized() && entry->name == strAccount)
                nReceived += r.second;
        } else if (strAccount.empty()) {
            nReceived += r.second;
        }
    }
}

bool CWalletTx::WriteToDisk(CWalletDB* pwalletdb)
{
    if (pwalletdb)
        return pwalletdb->WriteTx(GetHash(), *this);
    return CWalletDB(pwallet->strWalletFile).WriteTx(GetHash(), *this);
}

// Scan the block chain (starting in pindexStart) for transactions
// from or to us. If fUpdate is true, found transactions that already
// exist in the wallet will be updated.
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int ret = 0;

    CBlockIndex* pindex = pindexStart;

    uint64_t blockCount = pindexStart->nHeight;

    {
        LOCK2(cs_main, cs_wallet);
        printf("Starting wallet rescan of %d blocks...\n", nBestHeight.load());
        while (pindex) {
            blockCount++;

            if (blockCount % 1000 == 0) {
                uiInterface.InitMessage(_("Rescanning... ") + "(block: " + std::to_string(blockCount) +
                                        "/" + std::to_string(nBestHeight) + ")");
                printf("Done scanning %" PRIu64 " blocks\n", blockCount);
            }

            // no need to read and scan block, if block was created before
            // our wallet birthday (as adjusted for block time variability)
            if (nTimeFirstKey && (pindex->nTime < (nTimeFirstKey - 7200))) {
                pindex = boost::atomic_load(&pindex->pnext).get();
                continue;
            }

            CBlock block;
            block.ReadFromDisk(pindex, true);
            for (CTransaction& tx : block.vtx) {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                    ret++;
            }
            pindex = boost::atomic_load(&pindex->pnext).get();
        }
        uiInterface.InitMessage(_("Updating wallet on disk (do not shutdown)..."));
        FlushWalletDB(true, strWalletFile, nullptr);
        uiInterface.InitMessage(_("Rescanning... ") + "(done)");
        printf("Done rescanning wallet.\n");
    }
    return ret;
}

void CWallet::ReacceptWalletTransactions(bool fFirstLoad)
{
    LOCK2(cs_main, cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

    // Sort pending wallet transactions based on their initial wallet insertion order
    for (PAIRTYPE(const uint256, CWalletTx) & item : mapWallet) {
        const uint256& wtxid = item.first;
        CWalletTx&     wtx   = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain();
        if (!wtx.IsCoinBase() && !wtx.IsCoinStake() && nDepth == 0 && !wtx.isAbandoned()) {
            mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
        }
    }

    // Try to add wallet transactions to memory pool
    for (PAIRTYPE(const int64_t, CWalletTx*) & item : mapSorted) {
        CWalletTx& wtx = *(item.second);

        LOCK(mempool.cs);
        auto mempoolRes = wtx.AcceptToMemoryPool();
        if (mempoolRes.isErr() && fFirstLoad && GetTime() - wtx.GetTxTime() > 12 * 60 * 60) {
            // First load of wallet, failed to accept to mempool, and older than 12 hours... not likely
            // to ever make it in to mempool
            AbandonTransaction(wtx.GetHash());
        }
    }
}

void CWalletTx::RelayWalletTransaction() const
{
    if (!IsCoinBase() && !IsCoinStake()) {
        if (GetDepthInMainChain() == 0 && !isAbandoned()) {
            uint256 hash = GetHash();
            printf("Relaying wtx %s\n", hash.ToString().c_str());

            RelayTransaction(*this);
        }
    }
}

void CWallet::ResendWalletTransactions(bool fForce)
{
    if (!fForce) {
        // Do this infrequently and randomly to avoid giving away
        // that these are our transactions.
        static int64_t nNextTime;
        if (GetTime() < nNextTime)
            return;
        bool fFirst = (nNextTime == 0);
        nNextTime   = GetTime() + GetRand(30 * 60);
        if (fFirst)
            return;

        // Only do it if there's been a new block since last time
        static int64_t nLastTime;
        if (nTimeBestReceived < nLastTime)
            return;
        nLastTime = GetTime();
    }

    // Rebroadcast any of our txes that aren't in a block yet
    printf("ResendWalletTransactions()\n");
    CTxDB txdb("r");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        for (PAIRTYPE(const uint256, CWalletTx) & item : mapWallet) {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (fForce || nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60)
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }
        for (PAIRTYPE(const unsigned int, CWalletTx*) & item : mapSorted) {
            CWalletTx& wtx = *item.second;
            if (wtx.CheckTransaction().isOk())
                wtx.RelayWalletTransaction();
            else
                printf("ResendWalletTransactions() : CheckTransaction failed for transaction %s\n",
                       wtx.GetHash().ToString().c_str());
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Actions
//

CAmount CWallet::GetBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end();
             ++it) {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted()) {
                nTotal += pcoin->GetAvailableCredit();
            }
        }
    }

    return nTotal;
}

CAmount CWallet::GetColdStakingBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& p : mapWallet) {
            const CWalletTx& pcoin = p.second;
            if (pcoin.HasP2CSOutputs() && pcoin.IsTrusted())
                nTotal += pcoin.GetColdStakingCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetStakingBalance(const bool fIncludeColdStaking) const
{
    return GetBalance() +
           (Params().IsColdStakingEnabled() && fIncludeColdStaking ? GetColdStakingBalance() : 0);
}

CAmount CWallet::GetDelegatedBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& p : mapWallet) {
            const CWalletTx& pcoin = p.second;
            if (pcoin.HasP2CSOutputs() && pcoin.IsTrusted())
                nTotal += pcoin.GetStakeDelegationCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& p : mapWallet) {
            const CWalletTx& pcoin = p.second;
            if (!pcoin.IsTrusted() && pcoin.GetDepthInMainChain() == 0 && pcoin.InMempool())
                nTotal += pcoin.GetAvailableCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureColdStakingBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& p : mapWallet) {
            const CWalletTx& pcoin = p.second;
            nTotal += pcoin.GetImmatureCredit(false, ISMINE_COLD);
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureDelegatedBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& p : mapWallet) {
            const CWalletTx& pcoin = p.second;
            nTotal += pcoin.GetImmatureCredit(false, ISMINE_SPENDABLE_DELEGATED);
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureBalance() const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& p : mapWallet) {
            const CWalletTx& pcoin = p.second;
            if (pcoin.IsCoinBase() && pcoin.GetBlocksToMaturity() > 0 && pcoin.IsInMainChain()) {
                nTotal += pcoin.GetImmatureCredit(false);
            }
        }
    }
    return nTotal;
}

// populate vCoins with vector of spendable COutputs
void CWallet::AvailableCoins(vector<COutput>& vCoins, bool fOnlyConfirmed, bool fIncludeColdStaking,
                             bool fIncludeDelegated, const CCoinControl* coinControl) const
{
    vCoins.clear();
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end();
             ++it) {
            const CWalletTx* pcoin = &(*it).second;

            if (!IsFinalTx(*pcoin))
                continue;

            if (fOnlyConfirmed && !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth == 0 && !pcoin->InMempool())
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                isminetype mine = IsMine(pcoin->vout[i]);
                if (mine == ISMINE_NO)
                    continue;

                if (IsMineCheck(mine, ISMINE_WATCH_ONLY))
                    continue;

                if (pcoin->vout[i].nValue < nMinimumInputValue)
                    continue;

                if (IsSpent(pcoin->GetHash(), i))
                    continue;

                if (!(!coinControl || !coinControl->HasSelected() ||
                      coinControl->IsSelected((*it).first, i)))
                    continue;

                // --Skip P2CS outputs
                // skip cold coins
                if (mine == ISMINE_COLD && (!fIncludeColdStaking || !HasDelegator(pcoin->vout[i])))
                    continue;
                // skip delegated coins
                if (mine == ISMINE_SPENDABLE_DELEGATED && !fIncludeDelegated)
                    continue;
                // skip auto-delegated coins
                if (mine == ISMINE_SPENDABLE_STAKEABLE && !fIncludeColdStaking && !fIncludeDelegated)
                    continue;

                // bool fIsValid =
                //     (((mine &
                //        ISMINE_SPENDABLE) !=
                //       ISMINE_NO) ||
                //      ((mine &
                //        (ISMINE_MULTISIG |
                //         (fIncludeColdStaking ? ISMINE_COLD
                //                              : ISMINE_NO) |
                //         (fIncludeDelegated
                //              ? ISMINE_SPENDABLE_DELEGATED
                //              : ISMINE_NO))) !=
                //       ISMINE_NO));

                vCoins.push_back(COutput(pcoin, i, nDepth));
            }
        }
    }
}

bool CWallet::GetAvailableP2CSCoins(std::vector<COutput>& vCoins) const
{
    vCoins.clear();
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            return false;
        }
        TRY_LOCK(cs_wallet, lockWallet);
        if (!lockWallet) {
            return false;
        }

        for (const auto& it : mapWallet) {
            const uint256&   wtxid = it.first;
            const CWalletTx* pcoin = &it.second;

            bool fConflicted;
            int  nDepth = pcoin->GetDepthAndMempool(fConflicted);

            if (fConflicted || nDepth < 0)
                continue;

            if (pcoin->HasP2CSOutputs()) {
                for (int i = 0; i < (int)pcoin->vout.size(); i++) {
                    const auto& utxo = pcoin->vout[i];

                    if (IsSpent(wtxid, i))
                        continue;

                    if (utxo.scriptPubKey.IsPayToColdStaking()) {
                        isminetype mine            = IsMine(utxo);
                        bool       isMineSpendable = mine & ISMINE_SPENDABLE_DELEGATED;
                        if (mine & ISMINE_COLD || isMineSpendable)
                            // Depth is not used, no need waste resources and set it for now.
                            vCoins.emplace_back(COutput(pcoin, i, 0 /*, isMineSpendable*/));
                    }
                }
            }
        }
    }
    return true;
}

void CWallet::AvailableCoinsForStaking(vector<COutput>& vCoins, unsigned int nSpendTime,
                                       bool fIncludeColdStaking, bool fIncludeDelegated) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);
        unsigned int nSMA = Params().StakeMinAge();
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end();
             ++it) {
            const CWalletTx* pcoin = &(*it).second;

            // Filtering by tx timestamp instead of block timestamp may give false positives but never
            // false negatives
            if (pcoin->nTime + nSMA > nSpendTime)
                continue;

            if (pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth == 0 && !pcoin->InMempool())
                continue;

            // get NTP1 information of this transaction
            bool txIsNTP1 = NTP1Transaction::IsTxNTP1(pcoin);

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                isminetype mine = IsMine(pcoin->vout[i]);

                if (IsSpent(pcoin->GetHash(), i))
                    continue;

                if (!(mine & ISMINE_SPENDABLE_STAKEABLE) && !(mine & ISMINE_SPENDABLE))
                    continue;

                // --Skip P2CS outputs
                // skip cold coins
                if (mine == ISMINE_COLD && (!fIncludeColdStaking || !HasDelegator(pcoin->vout[i])))
                    continue;
                // skip delegated coins
                if (mine == ISMINE_SPENDABLE_DELEGATED && !fIncludeDelegated)
                    continue;
                // skip auto-delegated coins
                if (mine == ISMINE_SPENDABLE_STAKEABLE && !fIncludeColdStaking && !fIncludeDelegated)
                    continue;

                // bool fIsValid =
                //     (((mine & ISMINE_SPENDABLE) != ISMINE_NO) ||
                //      ((mine & (ISMINE_MULTISIG | (fIncludeColdStaking ? ISMINE_COLD : ISMINE_NO) |
                //                (fIncludeDelegated ? ISMINE_SPENDABLE_DELEGATED : ISMINE_NO))) !=
                //       ISMINE_NO));

                if (pcoin->vout[i].nValue < nMinimumInputValue) {
                    continue;
                }

                if (txIsNTP1) {
                    // if this output is an NTP1 output, skip it
                    try {
                        const CTransaction* tx = dynamic_cast<const CTransaction*>(pcoin);
                        if (tx == nullptr) {
                            throw std::runtime_error("Unable to case transaction: " +
                                                     pcoin->GetHash().ToString() + " to CTransaction");
                        }
                        std::vector<std::pair<CTransaction, NTP1Transaction>> inputs =
                            NTP1Transaction::GetAllNTP1InputsOfTx(*tx, false);
                        NTP1Transaction ntp1tx;
                        ntp1tx.readNTP1DataFromTx(*tx, inputs);
                        // if this output contains tokens, skip it to avoid burning them
                        if (ntp1tx.getTxOut(i).tokenCount() > 0) {
                            continue;
                        }
                    } catch (std::exception& ex) {
                        printf("Unable to parse script to check whether an output is stakable; error "
                               "says: %s",
                               ex.what());
                    }
                }
                vCoins.push_back(COutput(pcoin, i, nDepth));
            }
        }
    }
}

static void ApproximateBestSubset(vector<pair<CAmount, pair<const CWalletTx*, unsigned int>>> vValue,
                                  CAmount nTotalLower, CAmount nTargetValue, vector<char>& vfBest,
                                  CAmount& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    seed_insecure_rand();

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++) {
        vfIncluded.assign(vValue.size(), false);
        CAmount nTotal         = 0;
        bool    fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++) {
            for (unsigned int i = 0; i < vValue.size(); i++) {
                // The solver here uses a randomized algorithm,
                // the randomness serves no real security purpose but is just
                // needed to prevent degenerate behavior and it is important
                // that the rng fast. We do not use a constant random sequence,
                // because there may be some privacy improvement by making
                // the selection random.
                if (nPass == 0 ? insecure_rand() & 1 : !vfIncluded[i]) {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue) {
                        fReachedTarget = true;
                        if (nTotal < nBest) {
                            nBest  = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

// ppcoin: total coins staked (non-spendable until maturity)
CAmount CWallet::GetStake() const
{
    CAmount nTotal = 0;
    LOCK2(cs_main, cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin, ISMINE_SPENDABLE_ALL, true);
    }
    return nTotal;
}

CAmount CWallet::GetNewMint() const
{
    CAmount nTotal = 0;
    LOCK2(cs_main, cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin, ISMINE_SPENDABLE_ALL, true);
    }
    return nTotal;
}

bool CWallet::AddAccountingEntry(const CAccountingEntry& acentry, CWalletDB& pwalletdb)
{
    if (!pwalletdb.WriteAccountingEntry(acentry))
        return false;

    laccentries.push_back(acentry);
    CAccountingEntry& entry = laccentries.back();
    wtxOrdered.insert(std::make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));

    return true;
}

bool CWallet::SelectCoinsMinConf(CAmount nTargetValue, unsigned int nSpendTime, int nConfMine,
                                 int nConfTheirs, vector<COutput> vCoins,
                                 set<pair<const CWalletTx*, unsigned int>>& setCoinsRet,
                                 CAmount& nValueRet, bool avoidNTP1Outputs)
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<CAmount, pair<const CWalletTx*, unsigned int>> coinLowestLarger;
    coinLowestLarger.first        = std::numeric_limits<CAmount>::max();
    coinLowestLarger.second.first = nullptr;
    vector<pair<CAmount, pair<const CWalletTx*, unsigned int>>> vValue;
    CAmount                                                     nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    for (COutput output : vCoins) {
        const CWalletTx* pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;

        if (avoidNTP1Outputs) {
            // get NTP1 information of this transaction
            bool txIsNTP1 = NTP1Transaction::IsTxNTP1(pcoin);

            if (txIsNTP1) {
                // if this output is an NTP1 output, skip it
                try {
                    const CTransaction* tx = dynamic_cast<const CTransaction*>(pcoin);
                    if (tx == nullptr) {
                        throw std::runtime_error("Unable to cast transaction: " +
                                                 pcoin->GetHash().ToString() + " to CTransaction");
                    }
                    std::vector<std::pair<CTransaction, NTP1Transaction>> inputs =
                        NTP1Transaction::GetAllNTP1InputsOfTx(*tx, false);
                    NTP1Transaction ntp1tx;
                    ntp1tx.readNTP1DataFromTx(*tx, inputs);
                    // if this output contains tokens, skip it to avoid burning them
                    assert(i < static_cast<int>(pcoin->vout.size()));
                    if (ntp1tx.getTxOut(i).tokenCount() > 0) {
                        continue;
                    }
                } catch (std::exception& ex) {
                    printf("Unable to parse script to check whether an output is NTP1; error "
                           "says: %s",
                           ex.what());
                }
            }
        }

        // Follow the timestamp rules
        if (pcoin->nTime > nSpendTime)
            continue;

        CAmount n = pcoin->vout[i].nValue;

        if (n <= MIN_TX_FEE) {
            continue;
        }

        pair<CAmount, pair<const CWalletTx*, unsigned int>> coin = make_pair(n, make_pair(pcoin, i));

        if (n == nTargetValue) {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        } else if (n < nTargetValue + CENT) {
            vValue.push_back(coin);
            nTotalLower += n;
        } else if (n < coinLowestLarger.first) {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue) {
        for (unsigned int i = 0; i < vValue.size(); ++i) {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue) {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    vector<char> vfBest;
    CAmount      nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + CENT)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + CENT, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + CENT) || coinLowestLarger.first <= nBest)) {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    } else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i]) {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        if (fDebug) {
            //// debug print
            printf("SelectCoins() best subset: ");
            for (unsigned int i = 0; i < vValue.size(); i++)
                if (vfBest[i])
                    printf("%s ", FormatMoney(vValue[i].first).c_str());
            printf("total %s\n", FormatMoney(nBest).c_str());
        }
    }

    return true;
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
    const COutPoint                                               outpoint(hash, n);
    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    auto            lock     = mapTxSpends.get_lock();
    const TxSpends& txSpends = mapTxSpends.get_unsafe();
    range                    = txSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it) {
        const uint256&                               wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit   = mapWallet.find(wtxid);
        if (mit != mapWallet.end()) {
            bool      fConflicted;
            const int nDepth = mit->second.GetDepthAndMempool(fConflicted);
            // not in mempool txes can spend coins only if not coinstakes
            const bool fConflictedCoinstake = fConflicted && mit->second.IsCoinStake();
            if (nDepth > 0 || (nDepth == 0 && !mit->second.isAbandoned() && !fConflictedCoinstake))
                return true; // Spent
        }
    }
    return false;
}

bool CWallet::SelectCoins(CAmount nTargetValue, unsigned int nSpendTime,
                          set<pair<const CWalletTx*, unsigned int>>& setCoinsRet, CAmount& nValueRet,
                          const CCoinControl* coinControl, bool fIncludeColdStaking,
                          bool fIncludeDelegated, bool avoidNTP1Outputs) const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, fIncludeColdStaking, fIncludeDelegated, coinControl);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for
    // sure)
    if (coinControl && coinControl->HasSelected()) {
        for (const COutput& out : vCoins) {
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    return (SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 10, vCoins, setCoinsRet, nValueRet,
                               avoidNTP1Outputs) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 1, vCoins, setCoinsRet, nValueRet,
                               avoidNTP1Outputs) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 0, 1, vCoins, setCoinsRet, nValueRet,
                               avoidNTP1Outputs));
}

// Select some coins without random shuffle or best subset approximation
bool CWallet::SelectCoinsForStaking(CAmount nTargetValue, unsigned int nSpendTime,
                                    set<pair<const CWalletTx*, unsigned int>>& setCoinsRet,
                                    CAmount& nValueRet, bool fIncludeColdStaking,
                                    bool fIncludeDelegated) const
{
    vector<COutput> vCoins;
    AvailableCoinsForStaking(vCoins, nSpendTime, fIncludeColdStaking, fIncludeDelegated);

    setCoinsRet.clear();
    nValueRet = 0;

    for (COutput output : vCoins) {
        const CWalletTx* pcoin = output.tx;
        int              i     = output.i;

        // Stop if we've chosen enough inputs
        if (nValueRet >= nTargetValue)
            break;

        CAmount n = pcoin->vout[i].nValue;

        pair<CAmount, pair<const CWalletTx*, unsigned int>> coin = make_pair(n, make_pair(pcoin, i));

        if (n >= nTargetValue) {
            // If input value is greater or equal to target then simply insert
            //    it into the current subset and exit
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            break;
        } else if (n < nTargetValue + CENT) {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
        }
    }

    return true;
}

std::vector<CWalletTx> CWallet::getWalletTxs()
{
    LOCK(cs_wallet);
    std::vector<CWalletTx> result;
    result.reserve(mapWallet.size());
    for (const auto& entry : mapWallet) {
        result.emplace_back(entry.second);
    }
    return result;
}

void AddCoinsToInputsSet(set<pair<const CWalletTx*, unsigned int>>& setInputs, const NTP1OutPoint& input)
{
    std::vector<COutput> coins;
    pwalletMain->AvailableCoins(coins);
    auto itCoin = std::find_if(coins.begin(), coins.end(), [&input](const COutput& o) {
        return (o.tx->GetHash() == input.getHash() && o.i == (int)input.getIndex());
    });
    if (itCoin == coins.end()) {
        throw std::runtime_error("In the main wallet, could not find the output " +
                                 input.getHash().ToString() + ":" + ::ToString(input.getIndex()) +
                                 " which was used as an input");
    }
    // add the input if it's not alreadt in setInputs (comparison conditions are not the default ones,
    // that's why find_if is used)
    auto itInput = std::find_if(
        setInputs.begin(), setInputs.end(), [&input](const pair<const CWalletTx*, unsigned int>& p) {
            return p.first->GetHash() == input.getHash() && p.second == input.getIndex();
        });
    if (itInput == setInputs.end()) {
        setInputs.insert(std::make_pair(itCoin->tx, itCoin->i));
    }
}

void CWallet::SetTxNTP1OpRet(CTransaction& wtxNew, const std::shared_ptr<NTP1Script>& script)
{

    std::string opRetScriptBin = script->calculateScriptBin();
    std::string opRetScriptHex = boost::algorithm::hex(opRetScriptBin);

    // find OP_RETURN output
    auto it = std::find_if(wtxNew.vout.begin(), wtxNew.vout.end(), [](const CTxOut& o) {
        std::string scriptPubKeyStr = o.scriptPubKey.ToString();
        return scriptPubKeyStr.substr(0, std::string("OP_RETURN").length()) == "OP_RETURN";
    });

    if (it == wtxNew.vout.end()) {
        // no OP_RETURN found and there are TIs. Something is wrong.
        throw std::runtime_error("Could not find OP_RETURN output to fix change output index");
    }

    if (opRetScriptBin.size() > Params().OpReturnMaxSize()) {
        // the blockchain consensus rules prevents OP_RETURN sizes larger than DataSize(nBestHeight)
        throw std::runtime_error("The data associated with the transaction is larger than the maximum "
                                 "allowed size for metadata (" +
                                 ToString(Params().OpReturnMaxSize()) + " bytes).");
    }

    it->scriptPubKey = CScript() << OP_RETURN << ParseHex(opRetScriptHex);
}

void CWallet::SetTxNTP1OpRet(CTransaction&                                       wtxNew,
                             const std::vector<NTP1Script::TransferInstruction>& TIs,
                             const std::string                                   ntp1metadata,
                             boost::optional<IssueTokenData>                     issuanceData)
{
    if (TIs.empty()) {
        // no OP_RETURN, no NTP1 outputs
        return;
    }
    for (NTP1Script::TransferInstruction ti : TIs) {
        if (ti.outputIndex > 31) {
            throw std::runtime_error("Invalid output index was reached (" +
                                     std::to_string(ti.outputIndex) +
                                     "). Output indices in NTP1 cannot exceed 31 or there would be a "
                                     "risk of burning tokens. Try to use less outputs/recipients.");
        }
    }
    // set the OP_RETURN script
    if (issuanceData.is_initialized()) {
        const IssueTokenData& d = issuanceData.get();
        auto agg = NTP1Script::IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable;

        std::shared_ptr<NTP1Script_Issuance> script =
            NTP1Script_Issuance::CreateScript(d.symbol, d.amount, TIs, ntp1metadata, true, 0, agg);

        SetTxNTP1OpRet(wtxNew, script);
    } else {
        std::shared_ptr<NTP1Script_Transfer> script =
            NTP1Script_Transfer::CreateScript(TIs, ntp1metadata);

        SetTxNTP1OpRet(wtxNew, script);
    }
}

std::vector<NTP1Script::TransferInstruction>
CWallet::AddNTP1TokenInputsToTx(CTransaction& wtxNew, const NTP1SendTxData& ntp1TxData,
                                const int tokenOutputsOffset)
{
    std::vector<NTP1Script::TransferInstruction> TIs;

    std::vector<IntermediaryTI> ntp1IntermediaryTIs = ntp1TxData.getIntermediaryTIs();
    // because we don't add NTP1 input, its presense in TIs must be subtracted, to make position number i
    // contain token number i, where issued tokens don't exist
    // if no issuance exists, this remains zero
    int NTP1IssuanceFoundOffset = 0;

    // loop over inputs and insert them to inputs in order and at the beginning of vin array
    for (int i = 0; i < (int)ntp1IntermediaryTIs.size(); i++) {
        auto& iti = ntp1IntermediaryTIs[i];

        auto inputIt = std::find_if(wtxNew.vin.begin(), wtxNew.vin.end(), [&iti](const CTxIn& Input) {
            return Input.prevout.hash == iti.input.getHash() && Input.prevout.n == iti.input.getIndex();
        });
        if (inputIt == wtxNew.vin.end()) {
            // if this iti is for issuance, don't add it its input to NEBL inputs
            // otherwise this is not issuance, because issuance doesn't have input
            if (!iti.isNTP1TokenIssuance) {
                // add the input only if it doesn't exist
                wtxNew.vin.insert(wtxNew.vin.begin() + i - NTP1IssuanceFoundOffset,
                                  CTxIn(iti.input.getHash(), iti.input.getIndex()));
            } else {
                assert(NTP1IssuanceFoundOffset == 0);
                if (NTP1IssuanceFoundOffset > 0) {
                    throw std::runtime_error(
                        "Seems like there is an attempt to issue multiple tokens in one transaction.");
                }
                NTP1IssuanceFoundOffset = 1;
            }
        } else {
            // if the input already exists, move it to position i
            CTxIn toMove = *inputIt;
            wtxNew.vin.erase(inputIt);
            wtxNew.vin.insert(wtxNew.vin.begin() + i - NTP1IssuanceFoundOffset, toMove);
        }

        // loop over outputs (TIs) that will consume the input
        for (int j = 0; j < (int)iti.TIs.size(); j++) {
            auto& ti = iti.TIs[j];
            if (ti.outputIndex != IntermediaryTI::CHANGE_OUTPUT_FAKE_INDEX) {
                // the outputs the come from the IntermediaryTI start at zero, but the outputs in reality
                // are shifted by an offset because there are other outputs that are added by Neblio with
                // only nebls, with no NTP1 tokens in them
                ti.outputIndex += tokenOutputsOffset;
            }

            TIs.push_back(iti.TIs[j]);
        }
    }

    return TIs;
}

int CWallet::AddNTP1TokenOutputsToTx(CTransaction& wtxNew, const NTP1SendTxData& ntp1TxData)
{
    // some invalid value
    int opReturnIndex = -10;

    const std::vector<IntermediaryTI>&             ITIs = ntp1TxData.getIntermediaryTIs();
    typedef std::vector<IntermediaryTI>::size_type s_t;
    // calculate total number of TIs
    int totalTIs =
        std::accumulate(ITIs.begin(), ITIs.end(), 0, [](const s_t& current, const IntermediaryTI& iti) {
            return current + iti.TIs.size();
        });
    if (totalTIs > 0) {
        // it's important to set this as OP_RETURN to find it later; it's searched for
        CScript scriptPubKey = CScript() << OP_RETURN << ParseHex("00");
        opReturnIndex        = wtxNew.vout.size();
        wtxNew.vout.push_back(CTxOut(MIN_TX_FEE, scriptPubKey));
    }

    const int tokenOutputsOffset = opReturnIndex + 1;

    // create token outputs, basically the output number will be tokenOutputsOffset + i
    // where i is the recipient number in the list
    for (const auto& r : ntp1TxData.getNTP1TokenRecipientsList()) {
        CScript scriptPubKey;
        scriptPubKey.SetDestination(CBitcoinAddress(r.destination).Get());
        wtxNew.vout.push_back(CTxOut(MIN_TX_FEE, scriptPubKey));
    }

    return tokenOutputsOffset;
}

uint64_t GetTotalNeblsInInputs(const std::vector<NTP1OutPoint>& inputs)
{
    uint64_t total = 0;

    std::vector<COutput> avOutputs;
    pwalletMain->AvailableCoins(avOutputs);

    for (const auto& input : inputs) {
        // find the output (now input) in the list of available coins
        auto it = std::find_if(avOutputs.begin(), avOutputs.end(), [&input](const COutput& avOutput) {
            return (input.getHash() == avOutput.tx->GetHash() && (int)input.getIndex() == avOutput.i);
        });
        if (it == avOutputs.end()) {
            throw std::runtime_error("A used input: " + input.getHash().ToString() + ":" +
                                     ::ToString(input.getIndex()) +
                                     " was not found in the available outputs.");
        }

        // add the output value to the total
        const CTransaction* tx    = it->tx;
        int                 index = it->i;
        if (index + 1 > (int)tx->vout.size()) {
            throw std::runtime_error("A used input: " + input.getHash().ToString() + ":" +
                                     ::ToString(input.getIndex()) +
                                     " has an index that's out of range.");
        }
        total += tx->vout[index].nValue;
    }
    return total;
}

void CreateErrorMsg(std::string* errorMsg, const std::string& msg)
{
    printf("Emitting error: %s\n", msg.c_str());
    if (errorMsg) {
        *errorMsg = msg;
    }
}

bool CWallet::CreateTransaction(const vector<pair<CScript, CAmount>>& vecSend, CWalletTx& wtxNew,
                                CReserveKey& reservekey, CAmount& nFeeRet, NTP1SendTxData ntp1TxData,
                                const RawNTP1MetadataBeforeSend& ntp1metadata, bool isNTP1Issuance,
                                const CCoinControl* coinControl, std::string* errorMsg,
                                bool fIncludeDelegated)
{
    CAmount nValue = 0;
    for (const PAIRTYPE(CScript, CAmount) & s : vecSend) {
        if (nValue < 0) {
            CreateErrorMsg(errorMsg, "Value required is less than zero.");
            return false;
        }
        nValue += s.second;
    }

    if ((vecSend.empty() || nValue < 0) && ntp1TxData.getTotalTokensInInputs().size() == 0 &&
        !isNTP1Issuance) {
        CreateErrorMsg(errorMsg, "Invalid selected inputs/outputs/values for the transaction.");
        return false;
    }

    wtxNew.BindWallet(this);

    {
        LOCK2(cs_main, cs_wallet);
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            nFeeRet = nTransactionFee;
            while (true) {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;

                CAmount nTotalValue = nValue + nFeeRet;
                double  dPriority   = 0;
                // vouts to the payees
                for (const PAIRTYPE(CScript, CAmount) & s : vecSend) {
                    wtxNew.vout.push_back(CTxOut(s.second, s.first));
                }

                int changeOutputIndex = -10;

                // Choose coins to use
                set<pair<const CWalletTx*, unsigned int>> setCoins;
                CAmount                                   nValueIn = 0;
                if (!SelectCoins(nTotalValue, wtxNew.nTime, setCoins, nValueIn, coinControl, false,
                                 fIncludeDelegated, isNTP1Issuance)) {
                    CreateErrorMsg(errorMsg,
                                   "Failed to collect nebls for the transaction. You may have chosen to "
                                   "spend balance you do not have. For example, you chose to spend "
                                   "balance that is delegated without enabling delegated balance.");
                    return false;
                }

                for (PAIRTYPE(const CWalletTx*, unsigned int) pcoin : setCoins) {
                    CAmount nCredit = pcoin.first->vout[pcoin.second].nValue;
                    dPriority += (double)nCredit * pcoin.first->GetDepthInMainChain();
                }

                // select NTP1 tokens to determine change (this may be superfluous the first time this is
                // called since it's already called before this whole function, but that's fine; it's
                // necessary to call this after adding any new inputs to check whether any new inputs
                // have token change)
                {
                    bool takeInputsFromCoinControl =
                        coinControl != nullptr && coinControl->HasSelected();
                    try {
                        // join both inputs from NTP1SendTxData and setCoins
                        std::set<COutPoint> inputs;
                        for (const auto& s : setCoins) {
                            inputs.insert(COutPoint(s.first->GetHash(), s.second));
                        }
                        // for (const auto s : ntp1TxData.getUsedInputs()) {
                        //     inputs.insert(COutPoint(s.getHash(),
                        //     s.getIndex()));
                        // }

                        std::vector<COutPoint> inputsFromCoinControl =
                            (takeInputsFromCoinControl ? coinControl->GetSelected()
                                                       : vector<COutPoint>());
                        inputs.insert(inputsFromCoinControl.begin(), inputsFromCoinControl.end());

                        ntp1TxData.selectNTP1Tokens(
                            ntp1TxData.getWallet(), std::vector<COutPoint>(inputs.begin(), inputs.end()),
                            ntp1TxData.getNTP1TokenRecipientsList(), !takeInputsFromCoinControl);

                        // calculate the total nebls in all inputs (the other place where this is
                        // calculated is basically legacy and will be remove in the future)
                        std::vector<NTP1OutPoint> usedInputs = ntp1TxData.getUsedInputs();

                        nValueIn = GetTotalNeblsInInputs(usedInputs);

                    } catch (std::exception& ex) {
                        printf("Failed to select NTP1 tokens with error: %s\n", ex.what());
                        CreateErrorMsg(errorMsg, "Failed to select NTP1 tokens with error: " +
                                                     std::string(ex.what()));
                        return false;
                    }
                }

                int tokenOutputOffset = -1;

                // add NTP1 outputs to tx
                try {
                    tokenOutputOffset = AddNTP1TokenOutputsToTx(wtxNew, ntp1TxData);
                } catch (std::exception& ex) {
                    printf("Error in CreateTransaction() while adding NTP1 outputs: %s\n", ex.what());
                    CreateErrorMsg(errorMsg, "Error in CreateTransaction() while adding NTP1 oututs: " +
                                                 std::string(ex.what()));
                    return false;
                }

                bool ntp1TokenChangeExists = false;
                if (ntp1TxData.hasNTP1Tokens()) {
                    ntp1TokenChangeExists = (ntp1TxData.getChangeTokens().size() != 0);
                }

                CAmount nChange = nValueIn - nValue - nFeeRet;
                // if sub-cent change is required, the fee must be raised to at least MIN_TX_FEE
                // or until nChange becomes zero
                // NOTE: this depends on the exact behaviour of GetMinFee
                if (nFeeRet < MIN_TX_FEE && nChange > 0 && nChange < CENT) {
                    CAmount nMoveToFee = min(nChange, MIN_TX_FEE - nFeeRet);
                    nChange -= nMoveToFee;
                    nFeeRet += nMoveToFee;
                }

                CKeyID changeKeyID;

                if (nChange > 0 || ntp1TokenChangeExists) {
                    // Fill a vout to ourself
                    // TODO: pass in scriptChange instead of reservekey so
                    // change transaction isn't always pay-to-bitcoin-address
                    CScript scriptChange;

                    // coin control: send change to custom address
                    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange)) {
                        scriptChange.SetDestination(coinControl->destChange);
                        changeKeyID = boost::get<CKeyID>(coinControl->destChange);
                    }

                    // no coin control: send change to newly generated address
                    else {
                        // Note: We use a new key here to keep it from being obvious which side is the
                        // change.
                        //  The drawback is that by not reusing a previous key, the change may be lost if
                        //  a backup is restored, if the backup doesn't have the new private key for the
                        //  change. If we reused the old key, it would be possible to add code to look
                        //  for and rediscover unknown transactions that were written with keys of ours
                        //  to recover post-backup change.

                        // Reserve a new key pair from key pool
                        CPubKey vchPubKey;

                        bool r = reservekey.GetReservedKey(vchPubKey);
                        ignore_unused(r);
                        assert(r); // should never fail, as we just unlocked

                        scriptChange.SetDestination(vchPubKey.GetID());

                        changeKeyID = vchPubKey.GetID();
                    }

                    // create change for NEBLs, if that exists
                    if (nChange > 0) {
                        wtxNew.vout.push_back(CTxOut(nChange, scriptChange));
                    }
                    // create change for NTP1, if that exists
                    if (ntp1TokenChangeExists) {
                        changeOutputIndex = wtxNew.vout.size();
                        wtxNew.vout.push_back(CTxOut(MIN_TX_FEE, scriptChange));
                    }
                } else {
                    reservekey.ReturnKey();
                }

                // Fill vin
                for (const PAIRTYPE(const CWalletTx*, unsigned int) & coin : setCoins) {
                    wtxNew.vin.push_back(CTxIn(coin.first->GetHash(), coin.second));
                }

                // add NTP1 inputs to tx
                std::vector<NTP1Script::TransferInstruction> TIs;
                try {
                    TIs = AddNTP1TokenInputsToTx(wtxNew, ntp1TxData, tokenOutputOffset);
                    for (const auto& iti : ntp1TxData.getIntermediaryTIs()) {
                        // null input is for issuance, but we don't add it to inputs
                        if (iti.input.isNull()) {
                            if (!isNTP1Issuance) {
                                throw std::runtime_error("A null NTP1 input was found, which is not "
                                                         "valid for Non-issuance transactions.");
                            }
                            continue;
                        }
                        AddCoinsToInputsSet(setCoins, iti.input);
                    }
                } catch (std::exception& ex) {
                    printf("Error in CreateTransaction() while adding NTP1 inputs: %s\n", ex.what());
                    CreateErrorMsg(errorMsg, "Error in CreateTransaction() while adding NTP1 inputs: " +
                                                 std::string(ex.what()));
                    return false;
                }

                if (ntp1TokenChangeExists && changeOutputIndex < 0) {
                    printf("Failed to deduce the OP_RETURN output index in CreateTransaction()\n");
                    CreateErrorMsg(errorMsg,
                                   "Failed to deduce the OP_RETURN output index in CreateTransaction()");
                    return false;
                }

                if (changeOutputIndex >= 0 && wtxNew.vout[changeOutputIndex].nValue < MIN_TX_FEE) {
                    wtxNew.vout[changeOutputIndex].nValue = MIN_TX_FEE;
                }

                try {
                    NTP1SendTxData::FixTIsChangeOutputIndex(TIs, changeOutputIndex);
                    std::string processedMetadata = ntp1metadata.applyMetadataEncryption(
                        wtxNew, ntp1TxData.getNTP1TokenRecipientsList());
                    CWallet::SetTxNTP1OpRet(wtxNew, TIs, processedMetadata,
                                            ntp1TxData.getNTP1TokenIssuanceData());
                } catch (std::exception& ex) {
                    printf("Error while setting up NTP1 data: %s\n", ex.what());
                    CreateErrorMsg(errorMsg,
                                   "Error while setting up NTP1 data: " + std::string(ex.what()));
                    return false;
                }

                // Sign
                for (const PAIRTYPE(const CWalletTx*, unsigned int) & coin : setCoins) {
                    // find the output from the set in the list of inputs of the new tx
                    auto it =
                        std::find_if(wtxNew.vin.begin(), wtxNew.vin.end(), [&coin](const CTxIn& in) {
                            return (in.prevout.hash == coin.first->GetHash() &&
                                    in.prevout.n == coin.second);
                        });
                    assert(it != wtxNew.vin.end());
                    if (it == wtxNew.vin.end()) {
                        CreateErrorMsg(
                            errorMsg,
                            "Could not find input after having inserted it in the transaction.");
                        return false;
                    }
                    int nIn = std::distance(wtxNew.vin.begin(), it);
                    if (SignSignature(*this, *coin.first, wtxNew, nIn) != SignatureState::Verified) {
                        CreateErrorMsg(errorMsg, "Error while signing transactions inputs.");
                        return false;
                    }
                }

                // Limit size
                unsigned int nBytes =
                    ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK, PROTOCOL_VERSION);
                if (nBytes >= MAX_STANDARD_TX_SIZE) {
                    CreateErrorMsg(errorMsg, "Transaction size is bigger than the allowed limit. Try to "
                                             "split the transaction to multiple smaller ones.");
                    return false;
                }
                dPriority /= nBytes;

                // Check that enough fee is included
                CAmount NTP1Fee = ntp1TxData.getRequiredNeblsForOutputs();
                CAmount nPayFee = nTransactionFee * (1 + (CAmount)nBytes / 1000) + NTP1Fee;
                CAmount nMinFee = wtxNew.GetMinFee(1, GMF_SEND, nBytes) + NTP1Fee;

                if (nFeeRet < max(nPayFee, nMinFee)) {
                    nFeeRet = max(nPayFee, nMinFee);
                    continue;
                }

                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey, CAmount nValue, CWalletTx& wtxNew,
                                CReserveKey& reservekey, CAmount& nFeeRet,
                                const NTP1SendTxData& ntp1TxData, std::string* strError,
                                const RawNTP1MetadataBeforeSend& ntp1metadata, bool isNTP1Issuance,
                                const CCoinControl* coinControl, bool fIncludeDelegated)
{
    vector<pair<CScript, CAmount>> vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, ntp1TxData, ntp1metadata,
                             isNTP1Issuance, coinControl, strError, fIncludeDelegated);
}

// NovaCoin: get current stake weight
bool CWallet::GetStakeWeight(uint64_t& nMinWeight, uint64_t& nMaxWeight, uint64_t& nWeight) const
{
    // Choose coins to use
    const CAmount nBalance = GetBalance();

    nMinWeight = nMaxWeight = nWeight = 0;

    if (nBalance <= nReserveBalance)
        return false;

    set<pair<const CWalletTx*, unsigned int>> setCoins;
    CAmount                                   nValueIn = 0;

    if (!SelectCoinsForStaking(nBalance - nReserveBalance, GetTime(), setCoins, nValueIn))
        return false;

    if (setCoins.empty())
        return false;

    return GetStakeWeight(setCoins, nMinWeight, nMaxWeight, nWeight);
}

// NovaCoin: get current stake weight
bool CWallet::GetStakeWeight(const set<pair<const CWalletTx*, unsigned int>>& setCoins,
                             uint64_t& nMinWeight, uint64_t& nMaxWeight, uint64_t& nWeight)
{
    nMinWeight = nMaxWeight = nWeight = 0;

    if (setCoins.empty())
        return false;

    for (PAIRTYPE(const CWalletTx*, unsigned int) pcoin : setCoins) {
        const int64_t nTimeWeight = GetWeight((int64_t)pcoin.first->nTime, (int64_t)GetTime());
        const CBigNum bnCoinDayWeight =
            CBigNum(pcoin.first->vout[pcoin.second].nValue) * nTimeWeight / COIN / (24 * 60 * 60);

        const uint64_t dayWeight = bnCoinDayWeight.getuint64();

        // Weight is greater than zero
        if (nTimeWeight > 0) {
            nWeight += dayWeight;
        }

        // Weight is greater than zero, but the maximum value isn't reached yet
        if (nTimeWeight > 0 && nTimeWeight < Params().StakeMaxAge()) {
            nMinWeight += dayWeight;
        }

        // Maximum weight was reached
        if (nTimeWeight == Params().StakeMaxAge()) {
            nMaxWeight += dayWeight;
        }
    }

    return true;
}

// Call after CreateTransaction unless you want to abort
bool CWallet::CommitTransaction(const CWalletTx& wtxNew, CReserveKey& reservekey)
{
    {
        LOCK2(cs_main, cs_wallet);
        printf("CommitTransaction: %s\n", wtxNew.ToString().c_str());
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile, "r+") : NULL;

            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew, false, pwalletdb);

            // Mark old coins as spent
            std::set<uint256> updated_hahes;
            for (const CTxIn& txin : wtxNew.vin) {
                // notify only once
                if (updated_hahes.find(txin.prevout.hash) != updated_hahes.end())
                    continue;

                auto it = mapWallet.find(txin.prevout.hash);
                if (it != mapWallet.end()) {
                    CWalletTx& coin = it->second;
                    coin.BindWallet(this);
                    NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
                } else {
                    printf("Failed to commit transaction %s. An input was not found in the local wallet",
                           wtxNew.GetHash().ToString().c_str());
                    return false;
                }

                updated_hahes.insert(txin.prevout.hash);
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast
        auto mempoolRes = wtxNew.AcceptToMemoryPool();
        if (mempoolRes.isErr()) {
            // This must not fail. The transaction has already been signed and recorded.
            printf("CommitTransaction() : Error: Transaction not valid. Error: %s\n",
                   mempoolRes.unwrapErr().GetRejectReason().c_str());
            return false;
        }
        wtxNew.RelayWalletTransaction();
    }
    return true;
}

string CWallet::SendMoney(CScript scriptPubKey, CAmount nValue, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    CAmount     nFeeRequired;

    if (IsLocked()) {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }
    if (fWalletUnlockStakingOnly) {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }

    CTxDestination dest;
    if (!ExtractDestination(scriptPubKey, dest)) {
        throw std::runtime_error("Unable to extract address from scriptPubKey.");
    }

    CBitcoinAddress destAddress(dest);

    NTP1SendTokensOneRecipientData ntp1recipient;
    ntp1recipient.amount      = nValue;
    ntp1recipient.destination = destAddress.ToString();
    ntp1recipient.tokenId     = NTP1SendTxData::NEBL_TOKEN_ID;

    std::vector<NTP1SendTokensOneRecipientData> ntp1recipients(1, ntp1recipient);

    boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
    ntp1wallet->setRetrieveFullMetadata(false);
    ntp1wallet->update();

    NTP1SendTxData tokenSelector;
    tokenSelector.selectNTP1Tokens(ntp1wallet, vector<COutPoint>(), ntp1recipients, true);

    if (!CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired, tokenSelector)) {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError =
                strprintf(_("Error: This transaction requires a transaction fee of at least %s because "
                            "of its amount, complexity, or use of recently received funds  "),
                          FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your "
                 "wallet were already spent, such as if you used a copy of wallet.dat and coins were "
                 "spent in the copy but not marked as spent here.");

    return "";
}

string CWallet::SendMoneyToDestination(const CTxDestination& address, CAmount nValue, CWalletTx& wtxNew,
                                       bool fAskFee)
{
    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");
    if (nValue + nTransactionFee > GetBalance())
        return _("Insufficient funds");

    // Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address);

    return SendMoney(scriptPubKey, nValue, wtxNew, fAskFee);
}

string CWallet::SendNTP1ToDestination(const CTxDestination& address, CAmount nValue,
                                      const std::string& TokenId, CWalletTx& wtxNew,
                                      boost::shared_ptr<NTP1Wallet>    ntp1wallet,
                                      const RawNTP1MetadataBeforeSend& ntp1metadata, bool fAskFee)
{
    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");

    CReserveKey reservekey(this);
    CAmount     nFeeRequired;

    if (IsLocked()) {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }
    if (fWalletUnlockStakingOnly) {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }

    NTP1SendTokensOneRecipientData ntp1recipient;
    ntp1recipient.amount      = nValue;
    ntp1recipient.destination = CBitcoinAddress(address).ToString();
    ntp1recipient.tokenId     = TokenId;

    std::vector<NTP1SendTokensOneRecipientData> ntp1recipients(1, ntp1recipient);

    NTP1SendTxData tokenSelector;
    tokenSelector.selectNTP1Tokens(ntp1wallet, vector<COutPoint>(), ntp1recipients, true);

    if (!CreateTransaction(vector<pair<CScript, CAmount>>(), wtxNew, reservekey, nFeeRequired,
                           tokenSelector, ntp1metadata)) {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError =
                strprintf(_("Error: This transaction requires a transaction fee of at least %s because "
                            "of its amount, complexity, or use of recently received funds  "),
                          FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("SendMoney() : %s\n", strError.c_str());
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your "
                 "wallet were already spent, such as if you used a copy of wallet.dat and coins were "
                 "spent in the copy but not marked as spent here.");

    return "";
}

DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;

    DBErrors nLoadWalletRet = CWalletDB(strWalletFile, "cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    // This wallet is in its first run if all of these are empty
    fFirstRunRet = mapKeys.empty() && /*mapCryptedKeys.empty() &&*/ mapMasterKeys.empty() &&
                   /*setWatchOnly.empty() &&*/ mapScripts.empty();

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;

    NewThread(ThreadFlushWalletDB, &strWalletFile);
    return DB_LOAD_OK;
}

bool CWallet::SetAddressBookEntry(const CTxDestination& address, const string& strName,
                                  const std::string& strPurpose)
{
    bool fUpdated = HasAddressBookEntry(address);
    {
        AddressBook::CAddressBookData d;
        d.name = strName;
        if (!strPurpose.empty()) /* update purpose only if requested */
            d.purpose = strPurpose;
        mapAddressBook.set(address, d);
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address) != ISMINE_NO, strPurpose,
                             (fUpdated ? CT_UPDATED : CT_NEW));
    if (!fFileBacked)
        return false;
    std::string addressStr = CBitcoinAddress(address).ToString();
    if (!strPurpose.empty() && !CWalletDB(strWalletFile).WritePurpose(addressStr, strPurpose))
        return false;
    return CWalletDB(strWalletFile).WriteName(addressStr, strName);
}

bool CWallet::DelAddressBookName(const CTxDestination& address)
{
    std::string strAddress = CBitcoinAddress(address).ToString();
    std::string purpose    = purposeForAddress(address);

    {
        LOCK(cs_wallet); // mapAddressBook

        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address) != ISMINE_NO, purpose,
                             CT_DELETED);

    if (!fFileBacked)
        return false;
    CWalletDB(strWalletFile).ErasePurpose(strAddress);
    return CWalletDB(strWalletFile).EraseName(CBitcoinAddress(address).ToString());
}

std::string CWallet::purposeForAddress(const CTxDestination& address) const
{
    const auto mi = mapAddressBook.get(address);
    if (mi.is_initialized()) {
        return mi->purpose;
    }
    return "";
}

bool CWallet::HasAddressBookEntry(const CTxDestination& address) const
{
    return mapAddressBook.exists(address);
}

bool CWallet::HasDelegator(const CTxOut& out) const
{
    CTxDestination delegator;
    if (!ExtractDestination(out.scriptPubKey, delegator, false))
        return false;
    {
        const auto mi = mapAddressBook.get(delegator);
        if (!mi.is_initialized())
            return false;
        return mi->purpose == AddressBook::AddressBookPurpose::DELEGATOR;
    }
}

void CWallet::PrintWallet(const CBlock& block)
{
    {
        LOCK(cs_wallet);
        if (block.IsProofOfWork() && mapWallet.count(block.vtx[0].GetHash())) {
            CWalletTx& wtx = mapWallet[block.vtx[0].GetHash()];
            printf("    mine:  %d  %d  %" PRId64 "", wtx.GetDepthInMainChain(),
                   wtx.GetBlocksToMaturity(), wtx.GetCredit(ISMINE_ALL));
        }
        if (block.IsProofOfStake() && mapWallet.count(block.vtx[1].GetHash())) {
            CWalletTx& wtx = mapWallet[block.vtx[1].GetHash()];
            printf("    stake: %d  %d  %" PRId64 "", wtx.GetDepthInMainChain(),
                   wtx.GetBlocksToMaturity(), wtx.GetCredit(ISMINE_ALL));
        }
    }
    printf("\n");
}

void CWallet::Inventory(const uint256& hash)
{
    {
        LOCK(cs_wallet);
        std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
        if (mi != mapRequestCount.end())
            (*mi).second++;
    }
}

unsigned int CWallet::GetKeyPoolSize()
{
    AssertLockHeld(cs_wallet); // setKeyPool
    return setKeyPool.size();
}

bool CWallet::GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end()) {
            wtx = (*mi).second;
            return true;
        }
    }
    return false;
}

bool GetWalletFile(CWallet* pwallet, string& strWalletFileOut)
{
    if (!pwallet->fFileBacked)
        return false;
    strWalletFileOut = pwallet->strWalletFile;
    return true;
}

//
// Mark old keypool keys as used,
// and generate all new keys
//
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        for (int64_t nIndex : setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64_t nKeys = max(GetArg("-keypool", 100), (int64_t)0);
        for (int i = 0; i < nKeys; i++) {
            int64_t nIndex = i + 1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey()));
            setKeyPool.insert(nIndex);
        }
        printf("CWallet::NewKeyPool wrote %" PRId64 " new keys\n", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool(unsigned int nSize)
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize;
        if (nSize > 0)
            nTargetSize = nSize;
        else
            nTargetSize = max(GetArg("-keypool", 100), (int64_t)0);

        while (setKeyPool.size() < (nTargetSize + 1)) {
            int64_t nEnd = 1;
            if (!setKeyPool.empty())
                nEnd = *(--setKeyPool.end()) + 1;
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey())))
                throw runtime_error("TopUpKeyPool() : writing generated key failed");
            setKeyPool.insert(nEnd);
            printf("keypool added key %" PRId64 ", size=%" PRIszu "\n", nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool)
{
    nIndex            = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if (setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool() : read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool() : unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
        if (fDebug && GetBoolArg("-printkeypool"))
            printf("keypool reserve %" PRId64 "\n", nIndex);
    }
}

int64_t CWallet::AddReserveKey(const CKeyPool& keypool)
{
    {
        LOCK2(cs_main, cs_wallet);
        CWalletDB walletdb(strWalletFile);

        int64_t nIndex = 1 + *(--setKeyPool.end());
        if (!walletdb.WritePool(nIndex, keypool))
            throw runtime_error("AddReserveKey() : writing added key failed");
        setKeyPool.insert(nIndex);
        return nIndex;
    }
    return -1;
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    if (fFileBacked) {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
    if (fDebug)
        printf("keypool keep %" PRId64 "\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
    if (fDebug)
        printf("keypool return %" PRId64 "\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result)
{
    int64_t  nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex == -1) {
            if (IsLocked())
                return false;
            result = GenerateNewKey();
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    int64_t  nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return GetTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances()
{
    map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        for (PAIRTYPE(uint256, CWalletTx) walletEntry : mapWallet) {
            CWalletTx* pcoin = &walletEntry.second;

            if (!IsFinalTx(*pcoin) || !pcoin->IsTrusted())
                continue;

            if ((pcoin->IsCoinBase() || pcoin->IsCoinStake()) && pcoin->GetBlocksToMaturity() > 0)
                continue;

            bool fConflicted;
            int  nDepth = pcoin->GetDepthAndMempool(fConflicted);
            if (fConflicted)
                continue;
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                CTxDestination addr;
                if (IsMine(pcoin->vout[i]) == ISMINE_NO)
                    continue;
                if (!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set<set<CTxDestination>> CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    set<set<CTxDestination>> groupings;
    set<CTxDestination>      grouping;

    for (PAIRTYPE(uint256, CWalletTx) walletEntry : mapWallet) {
        CWalletTx* pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0) {
            bool any_mine = false;
            // group all input addresses with each other
            for (CTxIn txin : pcoin->vin) {
                CTxDestination address;
                if (IsMine(txin) == ISMINE_NO) /* If this input isn't mine, ignore it */
                    continue;
                if (!ExtractDestination(
                        mapWallet.at(txin.prevout.hash).vout.at(txin.prevout.n).scriptPubKey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine) {
                for (CTxOut txout : pcoin->vout)
                    if (IsChange(txout)) {
                        CWalletTx      tx = mapWallet.at(pcoin->vin[0].prevout.hash);
                        CTxDestination txoutAddr;
                        if (!ExtractDestination(txout.scriptPubKey, txoutAddr))
                            continue;
                        grouping.insert(txoutAddr);
                    }
            }
            if (grouping.size() > 0) {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i]) != ISMINE_NO) {
                CTxDestination address;
                if (!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set<set<CTxDestination>*> uniqueGroupings;        // a set of pointers to groups of addresses
    map<CTxDestination, set<CTxDestination>*> setmap; // map addresses to the unique group containing it
    for (set<CTxDestination> grouping : groupings) {
        // make a set of all the groups hit by this new group
        set<set<CTxDestination>*>                           hits;
        map<CTxDestination, set<CTxDestination>*>::iterator it;
        for (CTxDestination address : grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        set<CTxDestination>* merged = new set<CTxDestination>(grouping);
        for (set<CTxDestination>* hit : hits) {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for (CTxDestination element : *merged)
            setmap[element] = merged;
    }

    set<set<CTxDestination>> ret;
    for (set<CTxDestination>* uniqueGrouping : uniqueGroupings) {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

void CWallet::SyncTransaction(const CTransaction& tx, const CBlock* pblock)
{
    LOCK2(cs_main, cs_wallet);
    if (!AddToWalletIfInvolvingMe(tx, pblock, true))
        return; // Not one of ours

    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    for (const CTxIn& txin : tx.vin) {
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
            it->second.MarkDirty();
        }
    }
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1) {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex    = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex    = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    for (const int64_t& id : setKeyPool) {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes() : read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes() : unknown key in key pool");
        setAddress.insert(keyID);
    }
}

void CWallet::UpdatedTransaction(const uint256& hashTx)
{
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
    }
}

void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin();
         it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndexSmartPtr pindexMax = CBlock::FindBlockByHeight(
        std::max(0, nBestHeight - 144)); // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndexSmartPtr> mapKeyFirstBlock;
    std::set<CKeyID>                      setKeys;
    GetKeys(setKeys);
    for (const CKeyID& keyid : setKeys) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }
    setKeys.clear();

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end();
         it++) {
        // iterate over all wallet transactions...
        const CWalletTx&                  wtx  = it->second;
        BlockIndexMapType::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && blit->second->IsInMainChain()) {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            for (const CTxOut& txout : wtx.vout) {
                // iterate over all their outputs
                ::ExtractAffectedKeys(*this, txout.scriptPubKey, vAffected);
                for (const CKeyID& keyid : vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndexSmartPtr>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = boost::atomic_load(&blit->second);
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (std::map<CKeyID, CBlockIndexSmartPtr>::const_iterator it = mapKeyFirstBlock.begin();
         it != mapKeyFirstBlock.end(); it++) {
        mapKeyBirth[it->first] = it->second->nTime - 7200; // block times can be 2h off
    }
}

bool CWallet::AbandonTransaction(const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    CWalletDB walletdb(strWalletFile, "r+");

    std::set<uint256> todo;
    std::set<uint256> done;

    // Can't mark abandoned if confirmed or in mempool
    assert(mapWallet.count(hashTx));
    CWalletTx& origtx = mapWallet[hashTx];
    if (origtx.GetDepthInMainChain() > 0 || origtx.InMempool()) {
        return false;
    }

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        assert(mapWallet.count(now));
        CWalletTx& wtx            = mapWallet[now];
        int        currentconfirm = wtx.GetDepthInMainChain();
        // If the orig tx was not in block, none of its spends can be
        assert(currentconfirm <= 0);
        // if (currentconfirm < 0) {Tx and spends are already conflicted, no need to abandon}
        if (currentconfirm <= 0 && !wtx.isAbandoned()) {
            // If the orig tx was not in block/mempool, none of its spends can be in mempool
            assert(!wtx.InMempool());
            wtx.nIndex = -1;
            wtx.setAbandoned();
            wtx.MarkDirty();
            wtx.WriteToDisk(&walletdb);
            NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them
            // abandoned too
            auto                     txSpends = mapTxSpends.get();
            TxSpends::const_iterator iter     = txSpends.lower_bound(COutPoint(now, 0));
            while (iter != txSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin : wtx.vin) {
                if (mapWallet.count(txin.prevout.hash))
                    mapWallet[txin.prevout.hash].MarkDirty();
            }
        }
    }

    return true;
}

bool CWalletTx::IsTrusted() const
{
    // Quick answer in most cases
    if (!IsFinalTx(*this))
        return false;

    bool fConflicted = false;
    int  nDepth      = GetDepthAndMempool(fConflicted);
    if (fConflicted) // Don't trust unconfirmed transactions from us unless they are in the mempool.
        return false;

    if (nDepth >= 1)
        return true;
    if (nDepth < 0)
        return false;
    if (fConfChange || !IsFromMe(ISMINE_ALL)) // using wtx's cached debit
        return false;

    // Trusted if all inputs are from us and are in the mempool:
    for (const CTxIn& txin : vin) {
        // Transactions not sent by us: not trusted
        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == nullptr)
            return false;
        const CTxOut& parentOut = parent->vout[txin.prevout.n];
        if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
            return false;
    }

    return true;
}

int CWalletTx::GetDepthAndMempool(bool& fConflicted) const
{
    int ret     = GetDepthInMainChain();
    fConflicted = (ret == 0 && !InMempool()); // not in chain nor in mempool
    return ret;
}

bool CWalletTx::InMempool() const
{
    LOCK(mempool.cs);
    return mempool.exists(GetHash());
}

CAmount CWalletTx::GetDebit(const isminefilter& filter) const
{
    if (vin.empty())
        return 0;

    CAmount debit = 0;
    if (filter & ISMINE_SPENDABLE) {
        if (c_DebitCached)
            debit += *c_DebitCached;
        else {
            c_DebitCached = pwallet->GetDebit(*this, ISMINE_SPENDABLE);
            debit += *c_DebitCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY) {
        if (c_WatchDebitCached)
            debit += *c_WatchDebitCached;
        else {
            c_WatchDebitCached = pwallet->GetDebit(*this, ISMINE_WATCH_ONLY);
            debit += *c_WatchDebitCached;
        }
    }
    if (filter & ISMINE_COLD) {
        if (c_ColdDebitCached)
            debit += *c_ColdDebitCached;
        else {
            c_ColdDebitCached = pwallet->GetDebit(*this, ISMINE_COLD);
            debit += *c_ColdDebitCached;
        }
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        if (c_DelegatedDebitCached)
            debit += *c_DelegatedDebitCached;
        else {
            c_DelegatedDebitCached = pwallet->GetDebit(*this, ISMINE_SPENDABLE_DELEGATED);
            debit += *c_DelegatedDebitCached;
        }
    }
    return debit;
}

void CWalletTx::Init(const CWallet* pwalletIn)
{
    pwallet = pwalletIn;
    vtxPrev.clear();
    mapValue.clear();
    vOrderForm.clear();
    fTimeReceivedIsTxTime = false;
    nTimeReceived         = 0;
    nTimeSmart            = 0;
    fFromMe               = false;
    strFromAccount.clear();
    vfSpent.clear();

    c_WatchDebitCached           = boost::none;
    c_WatchCreditCached          = boost::none;
    c_AvailableWatchCreditCached = boost::none;
    c_ImmatureWatchCreditCached  = boost::none;

    c_ColdDebitCached  = boost::none;
    c_ColdCreditCached = boost::none;

    c_DelegatedDebitCached  = boost::none;
    c_DelegatedCreditCached = boost::none;

    c_DebitCached           = boost::none;
    c_CreditCached          = boost::none;
    c_AvailableCreditCached = boost::none;
    c_ChangeCached          = boost::none;

    nOrderPos = -1;
}

CAmount CWalletTx::GetAvailableCredit(bool /*fUseCache*/) const
{
    return GetUnspentCredit(ISMINE_SPENDABLE_ALL);
}

CAmount CWalletTx::GetColdStakingCredit(bool /*fUseCache*/) const
{
    return GetUnspentCredit(ISMINE_COLD);
}

CAmount CWalletTx::GetStakeDelegationCredit(bool /*fUseCache*/) const
{
    return GetUnspentCredit(ISMINE_SPENDABLE_DELEGATED);
}

CAmount CWalletTx::GetUnspentCredit(const isminefilter& filter) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (GetBlocksToMaturity() > 0)
        return 0;

    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE) {
        const auto f = ISMINE_SPENDABLE;
        credit += pwallet->GetCredit(*this, f, true);
    }
    if (filter & ISMINE_WATCH_ONLY) {
        const auto f = ISMINE_WATCH_ONLY;
        credit += pwallet->GetCredit(*this, f, true);
    }
    if (filter & ISMINE_COLD) {
        const auto f = ISMINE_COLD;
        credit += pwallet->GetCredit(*this, f, true);
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        const auto f = ISMINE_SPENDABLE_DELEGATED;
        credit += pwallet->GetCredit(*this, f, true);
    }
    return credit;
}

CAmount CWalletTx::GetChange() const
{
    if (c_ChangeCached)
        return *c_ChangeCached;
    c_ChangeCached = pwallet->GetChange(*this);
    return *c_ChangeCached;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache, const isminefilter& filter) const
{
    LOCK(cs_main);
    if ((IsCoinBase() || IsCoinStake()) && GetBlocksToMaturity() > 0 && IsInMainChain()) {
        if (fUseCache && c_ImmatureCreditCached && filter == ISMINE_SPENDABLE_ALL)
            return *c_ImmatureCreditCached;
        c_ImmatureCreditCached = pwallet->GetCredit(*this, filter, false);
        return *c_ImmatureCreditCached;
    }

    return 0;
}

bool CWalletTx::IsFromMe(const isminefilter& filter) const { return (GetDebit(filter) > 0); }

CAmount CWalletTx::GetCredit(const isminefilter& filter) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if ((IsCoinBase() || IsCoinStake()) && GetBlocksToMaturity() > 0)
        return 0;

    CAmount credit = 0;

    if (filter & ISMINE_SPENDABLE) {
        // GetBalance can assume transactions in mapWallet won't change
        if (c_CreditCached)
            credit += *c_CreditCached;
        else {
            c_CreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE, false);
            credit += *c_CreditCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY) {
        if (c_WatchCreditCached)
            credit += *c_WatchCreditCached;
        else {
            c_WatchCreditCached = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY, false);
            credit += *c_WatchCreditCached;
        }
    }
    if (filter & ISMINE_COLD) {
        if (c_ColdCreditCached)
            credit += *c_ColdCreditCached;
        else {
            c_ColdCreditCached = pwallet->GetCredit(*this, ISMINE_COLD, false);
            credit += *c_ColdCreditCached;
        }
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        if (c_DelegatedCreditCached)
            credit += *c_DelegatedCreditCached;
        else {
            c_DelegatedCreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE_DELEGATED, false);
            credit += *c_DelegatedCreditCached;
        }
    }
    return credit;
}

void CWalletTx::MarkDirty()
{
    c_CreditCached               = boost::none;
    c_AvailableCreditCached      = boost::none;
    c_DebitCached                = boost::none;
    c_ChangeCached               = boost::none;
    c_WatchDebitCached           = boost::none;
    c_WatchCreditCached          = boost::none;
    c_AvailableWatchCreditCached = boost::none;
    c_ImmatureWatchCreditCached  = boost::none;
    c_ColdDebitCached            = boost::none;
    c_ColdCreditCached           = boost::none;
    c_DelegatedDebitCached       = boost::none;
    c_DelegatedCreditCached      = boost::none;
}

void CWalletTx::BindWallet(CWallet* pwalletIn)
{
    pwallet = pwalletIn;
    MarkDirty();
}

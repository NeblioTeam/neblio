// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLETDB_H
#define BITCOIN_WALLETDB_H

#include "base58.h"
#include "db.h"

class CKeyPool;
class CAccount;
class CAccountingEntry;

/** Error statuses for the wallet database */
enum DBErrors
{
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_NONCRITICAL_ERROR,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
    DB_NEED_REWRITE
};

class CKeyMetadata
{
public:
    static const int CURRENT_VERSION = 1;
    int              nVersion;
    int64_t          nCreateTime; // 0 means unknown

    CKeyMetadata();
    CKeyMetadata(int64_t nCreateTime_);

    IMPLEMENT_SERIALIZE(READWRITE(this->nVersion); nVersion = this->nVersion; READWRITE(nCreateTime);)

    void SetNull();
};

class CLedgerKey
{
public:
    CPubKey vchPubKey;
    CKeyID accountPubKeyID;
    uint32_t account;
    bool isChange;
    uint32_t index;

    CLedgerKey() {}

    CLedgerKey(const CPubKey& vchPubKeyIn, const CKeyID& accountPubKeyIDIn, uint32_t accountIn, bool isChangeIn, uint32_t indexIn) :
        vchPubKey(vchPubKeyIn), accountPubKeyID(accountPubKeyIDIn), account(accountIn), isChange(isChangeIn), index(indexIn) {};

    IMPLEMENT_SERIALIZE(READWRITE(vchPubKey); READWRITE(accountPubKeyID); READWRITE(account); READWRITE(isChange); READWRITE(index);)
};

/** Access to the wallet database (wallet.dat) */
class CWalletDB : public CDB
{
public:
    // WARNING: TODO: The fact that the filename is std::string may be a problem with non-ascii
    // filenames, check! Keep in mind that this may work with wallet.dat, but any other name (which may
    // come from a backup) may not work.
    CWalletDB(std::string strFilename, const char* pszMode = "r+", bool fFlushOnCloseIn = true);

private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);

public:
    bool WriteName(const std::string& strAddress, const std::string& strName);

    bool EraseName(const std::string& strAddress);

    bool WriteTx(uint256 hash, const CWalletTx& wtx);

    bool EraseTx(uint256 hash);

    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta);

    bool WriteLedgerKey(const CLedgerKey& ledgerKey);

    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret,
                         const CKeyMetadata& keyMeta);

    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);

    bool WriteBestBlock(const CBlockLocator& locator);

    bool ReadBestBlock(CBlockLocator& locator) { return Read(std::string("bestblock"), locator); }

    bool WriteOrderPosNext(int64_t nOrderPosNext);

    bool ReadPool(int64_t nPool, CKeyPool& keypool);

    bool WritePool(int64_t nPool, const CKeyPool& keypool);

    bool ErasePool(int64_t nPool);

    bool WriteMinVersion(int nVersion) { return Write(std::string("minversion"), nVersion); }

    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);

    bool WritePurpose(const std::string& strAddress, const std::string& strPurpose);
    bool ErasePurpose(const std::string& strPurpose);

private:
    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);

public:
    bool    WriteAccountingEntry(const CAccountingEntry& acentry);
    int64_t GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors    ReorderTransactions(CWallet*);
    DBErrors    LoadWallet(CWallet* pwallet);
    static bool Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys);
    static bool Recover(CDBEnv& dbenv, std::string filename);
};

#endif // BITCOIN_WALLETDB_H

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLET_H
#define BITCOIN_WALLET_H

#include <boost/container/flat_map.hpp>
#include <string>
#include <vector>

#include <stdlib.h>

#include "key.h"
#include "keystore.h"
#include "merkletx.h"
#include "ntp1/ntp1sendtxdata.h"
#include "script.h"
#include "ui_interface.h"
#include "util.h"
#include "walletdb.h"

extern bool fWalletUnlockStakingOnly;
extern bool fConfChange;
class CAccountingEntry;
class CWalletTx;
class CReserveKey;
class COutput;
class CCoinControl;

std::pair<long, long> ImportBackupWallet(const std::string& Src, std::string& PassPhrase,
                                         bool importReserveToAddressBook);
bool                  IsWalletEncrypted(const std::string& Src);

bool ShouldWalletBeBackedUp();
void WriteWalletBackupHash();

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's
                          // clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_LATEST = 60000
};

class WalletNewTxUpdateFunctor : public boost::enable_shared_from_this<WalletNewTxUpdateFunctor>
{
    // reload balances if the current height less than the registered height plus this next value
public:
    static const int HEIGHT_OFFSET_TOLERANCE = 10;
    virtual void     run(uint256 /*txhash*/, int /*CurrentBlockHeight*/) {}
    virtual void     setReferenceBlockHeight() {}
};

struct CoinStakeResult
{
    CTransaction tx;
    CKey         key;
};

/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;

    CKeyPool() { nTime = GetTime(); }

    CKeyPool(const CPubKey& vchPubKeyIn)
    {
        nTime     = GetTime();
        vchPubKey = vchPubKeyIn;
    }

    IMPLEMENT_SERIALIZE(if (!(nType & SER_GETHASH)) READWRITE(nVersion); READWRITE(nTime);
                        READWRITE(vchPubKey);)
};

/** A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore
{
private:
    bool SelectCoinsForStaking(CAmount nTargetValue, unsigned int nSpendTime,
                               std::set<std::pair<const CWalletTx*, unsigned int>>& setCoinsRet,
                               CAmount&                                             nValueRet) const;
    bool SelectCoins(CAmount nTargetValue, unsigned int nSpendTime,
                     std::set<std::pair<const CWalletTx*, unsigned int>>& setCoinsRet,
                     CAmount& nValueRet, const CCoinControl* coinControl = nullptr,
                     bool avoidNTP1Outputs = false) const;

    CWalletDB* pwalletdbEncryption;

    // the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    // the maximum wallet format version: memory-only variable that specifies to what version this wallet
    // may be upgraded
    int nWalletMaxVersion;

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef boost::container::flat_multimap<COutPoint, uint256> TxSpends;
    LockedVar<TxSpends>                                         mapTxSpends;
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

    /** Mark a transaction (and its in-wallet descendants) as conflicting with a particular block.
     * hashBlock: the block that contains the input that is being spent
     * hashTx: The hash of the new transaction that is trying to spend that input
     */
    void MarkConflicted(const uint256& hashBlock, const uint256& hashTx);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

public:
    /// Main wallet lock.
    /// This lock protects all the fields added by CWallet
    ///   except for:
    ///      fFileBacked (immutable after instantiation)
    ///      strWalletFile (immutable after instantiation)
    mutable CCriticalSection cs_wallet;

    // this function is supposed to be called every time a new transcation is added to the wallet
    boost::shared_ptr<WalletNewTxUpdateFunctor> walletNewTxUpdateFunctor;
    void setFunctorOnTxInsert(boost::shared_ptr<WalletNewTxUpdateFunctor> func)
    {
        boost::atomic_store(&walletNewTxUpdateFunctor, func);
    }

    bool        fFileBacked;
    std::string strWalletFile;

    std::set<int64_t>              setKeyPool;
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

    static const boost::filesystem::path BackupHashFilename;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap                               mapMasterKeys;
    unsigned int                               nMasterKeyMaxID;

    CWallet() { SetNull(); }
    CWallet(std::string strWalletFileIn)
    {
        SetNull();

        strWalletFile = strWalletFileIn;
        fFileBacked   = true;
    }
    void SetNull()
    {
        nWalletVersion      = FEATURE_BASE;
        nWalletMaxVersion   = FEATURE_BASE;
        fFileBacked         = false;
        nMasterKeyMaxID     = 0;
        pwalletdbEncryption = nullptr;
        nOrderPosNext       = 0;
        nTimeFirstKey       = 0;
    }

    std::map<uint256, CWalletTx> mapWallet;
    std::list<CAccountingEntry>  laccentries;

    int64_t                nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<CTxDestination, std::string> mapAddressBook;

    int64_t nTimeFirstKey;

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    // check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf)
    {
        AssertLockHeld(cs_wallet);
        return nWalletMaxVersion >= wf;
    }

    void AvailableCoinsForStaking(std::vector<COutput>& vCoins, unsigned int nSpendTime) const;
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed = true,
                        const CCoinControl* coinControl = nullptr) const;
    bool SelectCoinsMinConf(CAmount nTargetValue, unsigned int nSpendTime, int nConfMine,
                            int nConfTheirs, std::vector<COutput> vCoins,
                            std::set<std::pair<const CWalletTx*, unsigned int>>& setCoinsRet,
                            CAmount& nValueRet, bool avoidNTP1Outputs = false) const;

    bool IsSpent(const uint256& hash, unsigned int n) const;

    // keystore implementation
    // Generate a new key
    CPubKey GenerateNewKey();
    // Adds a key to the store, and saves it to disk.
    bool AddKey(const CKey& key);
    // Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key) { return CCryptoKeyStore::AddKey(key); }
    // Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey& pubkey, const CKeyMetadata& metadata);

    bool LoadMinVersion(int nVersion);

    // Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    // Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase,
                                const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const;

    /* Mark a transaction (and it in-wallet descendants) as abandoned so its inputs may be respent. */
    bool AbandonTransaction(const uint256& hashTx);

    /** Increment the next transaction order id
        @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB* pwalletdb = nullptr);

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair>           TxItems;

    TxItems wtxOrdered;

    /** Get the wallet's activity log
        @return multimap of ordered transactions and accounting entries
        @warning Returned pointers are *only* valid within the scope of passed acentries
     */
    TxItems OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount = "");

    void         MarkDirty();
    unsigned int ComputeTimeSmart(const CWalletTx& wtx) const;
    bool         AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb);
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate = false,
                                  bool fFindBlock = false);
    bool EraseFromWallet(uint256 hash);
    //    void    WalletUpdateSpent(const CTransaction& prevout, bool fBlock = false);
    int     ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    void    ReacceptWalletTransactions(bool fFirstLoad = false);
    void    ResendWalletTransactions(bool fForce = false);
    void    SyncTransaction(const CTransaction& tx, const CBlock* pblock);
    CAmount GetBalance() const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    CAmount GetStake() const;
    CAmount GetNewMint() const;
    bool    AddAccountingEntry(const CAccountingEntry&, CWalletDB& pwalletdb);
    bool    CreateTransaction(const std::vector<std::pair<CScript, CAmount>>& vecSend, CWalletTx& wtxNew,
                              CReserveKey& reservekey, CAmount& nFeeRet, NTP1SendTxData ntp1TxData,
                              const RawNTP1MetadataBeforeSend& ntp1metadata = RawNTP1MetadataBeforeSend(),
                              bool isNTP1Issuance = false, const CCoinControl* coinControl = nullptr,
                              std::string* errorMsg = nullptr);
    bool    CreateTransaction(CScript scriptPubKey, CAmount nValue, CWalletTx& wtxNew,
                              CReserveKey& reservekey, CAmount& nFeeRet, const NTP1SendTxData& ntp1TxData,
                              const RawNTP1MetadataBeforeSend& ntp1metadata = RawNTP1MetadataBeforeSend(),
                              bool isNTP1Issuance = false, const CCoinControl* coinControl = nullptr);
    bool    CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);

    bool        GetStakeWeight(const CKeyStore& keystore, uint64_t& nMinWeight, uint64_t& nMaxWeight,
                               uint64_t& nWeight);
    void        FindStakeKernel(const CKeyStore& keystore, CKey& key, unsigned int nBits,
                                const std::set<std::pair<const CWalletTx*, unsigned int>>& setCoins,
                                CTxDB& txdb, CTransaction& txNew, std::vector<const CWalletTx*>& vwtxPrev,
                                CScript& scriptPubKeyKernel, CAmount& nCredit);
    bool        CreateCoinStake(const CKeyStore& keystore, unsigned int nBits, CAmount nFees,
                                CTransaction& txNew, CKey& key);
    static void UpdateStakeSearchTimes(int64_t nSearchTime);

    std::string SendMoney(CScript scriptPubKey, CAmount nValue, CWalletTx& wtxNew, bool fAskFee = false);
    std::string SendMoneyToDestination(const CTxDestination& address, CAmount nValue, CWalletTx& wtxNew,
                                       bool fAskFee = false);
    std::string
    SendNTP1ToDestination(const CTxDestination& address, CAmount nValue, const std::string& tokenId,
                          CWalletTx& wtxNew, boost::shared_ptr<NTP1Wallet> ntp1wallet,
                          const RawNTP1MetadataBeforeSend& ntp1metadata = RawNTP1MetadataBeforeSend(),
                          bool                             fAskFee      = false);

    bool    NewKeyPool();
    bool    TopUpKeyPool(unsigned int nSize = 0);
    int64_t AddReserveKey(const CKeyPool& keypool);
    void    ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool);
    void    KeepKey(int64_t nIndex);
    void    ReturnKey(int64_t nIndex);
    bool    GetKeyFromPool(CPubKey& key);
    int64_t GetOldestKeyPoolTime();
    void    GetAllReserveKeys(std::set<CKeyID>& setAddress) const;

    std::set<std::set<CTxDestination>> GetAddressGroupings();
    std::map<CTxDestination, CAmount>  GetAddressBalances();

    isminetype IsMine(const CTxIn& txin) const;
    CAmount    GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const;
    CAmount    GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool       IsChange(const CTxOut& txout) const;
    CAmount    GetChange(const CTxOut& txout) const;
    bool       IsMine(const CTransaction& tx) const;
    bool       IsFromMe(const CTransaction& tx) const;
    CAmount    GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount    GetCredit(const CTransaction& tx, const isminefilter& filter, const bool fUnspent) const;
    CAmount    GetChange(const CTransaction& tx) const;
    void       SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);

    bool SetAddressBookName(const CTxDestination& address, const std::string& strName);

    bool DelAddressBookName(const CTxDestination& address);

    void UpdatedTransaction(const uint256& hashTx);

    void PrintWallet(const CBlock& block);

    void Inventory(const uint256& hash);

    unsigned int GetKeyPoolSize();

    bool GetTransaction(const uint256& hashTx, CWalletTx& wtx);

    // signify that a particular wallet feature is now used. this may change nWalletVersion and
    // nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = nullptr, bool fExplicit = false);

    // change which version we're allowed to upgrade to (note that this does not immediately imply
    // upgrading to that format)
    bool SetMaxVersion(int nVersion);

    // get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion();

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    /** Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void(CWallet* wallet, const CTxDestination& address,
                                 const std::string& label, bool isMine, ChangeType status)>
        NotifyAddressBookChanged;

    /** Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void(CWallet* wallet, const uint256& hashTx, ChangeType status)>
        NotifyTransactionChanged;

    /**
     * @brief AddNTP1TokenInputsToTx
     * @param wtxNew
     * @param nativeInputs
     * @param ntp1TxData
     * @param tokenOutputsOffset
     * @return returns transfer instructions that are compatible with these
     * inputs for everything EXCEPT change, where change will remain unchanged from intermediary TIs
     */
    static std::vector<NTP1Script::TransferInstruction>
    AddNTP1TokenInputsToTx(CTransaction& wtxNew, const NTP1SendTxData& ntp1TxData,
                           const int tokenOutputsOffset);

    /**
     * creates outputs in transaction. This
     * is important because the outputs have to be determined BEFORE change is determined
     *
     * @brief AddNTP1TokenOutputsToTx
     * @param wtxNew
     * @param nativeInputs
     * @param ntp1TxData
     * @return token outputs offset (to be used with TIs)
     */
    static int AddNTP1TokenOutputsToTx(CTransaction& wtxNew, const NTP1SendTxData& ntp1TxData);

    /**
     * Find the OP_RETURN output and set the correct OP_RETURN argument in it based on the given TIs
     * (transfer instructions). If TIs list is empty, noting will be changed.
     *
     * @brief SetTxNTP1OpRet
     * @param wtxNew
     * @param TIs
     */
    static void SetTxNTP1OpRet(CTransaction& wtxNew, const std::shared_ptr<NTP1Script>& script);
    static void
    SetTxNTP1OpRet(CTransaction& wtxNew, const std::vector<NTP1Script::TransferInstruction>& TIs,
                   const std::string               ntp1metadata,
                   boost::optional<IssueTokenData> issuanceData = boost::optional<IssueTokenData>());
};

/** A key allocated from the key pool. */
class CReserveKey
{
protected:
    CWallet* pwallet;
    int64_t  nIndex;
    CPubKey  vchPubKey;

public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex  = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        if (!fShutdown)
            ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey& pubkey);
    void KeepKey();
};

typedef std::map<std::string, std::string> mapValue_t;

static void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n")) {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}

static void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

/** A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    std::vector<CMerkleTx>                           vtxPrev;
    mapValue_t                                       mapValue;
    std::vector<std::pair<std::string, std::string>> vOrderForm;
    unsigned int                                     fTimeReceivedIsTxTime;
    unsigned int                                     nTimeReceived; // time received by this node
    unsigned int                                     nTimeSmart;
    char                                             fFromMe;
    std::string                                      strFromAccount;
    std::vector<char>                                vfSpent;   // which outputs are already spent
    int64_t                                          nOrderPos; // position in ordered transaction list

    // memory only
    mutable boost::optional<CAmount> c_WatchDebitCached;
    mutable boost::optional<CAmount> c_WatchCreditCached;
    mutable boost::optional<CAmount> c_ImmatureWatchCreditCached;
    mutable boost::optional<CAmount> c_AvailableWatchCreditCached;

    mutable boost::optional<CAmount> c_ColdDebitCached;
    mutable boost::optional<CAmount> c_ColdCreditCached;

    mutable boost::optional<CAmount> c_DebitCached;
    mutable boost::optional<CAmount> c_CreditCached;
    mutable boost::optional<CAmount> c_AvailableCreditCached;
    mutable boost::optional<CAmount> c_ChangeCached;

    mutable boost::optional<CAmount> c_DelegatedDebitCached;
    mutable boost::optional<CAmount> c_DelegatedCreditCached;
    mutable boost::optional<CAmount> c_ImmatureCreditCached;

    CWalletTx() { Init(nullptr); }

    CWalletTx(const CWallet* pwalletIn) { Init(pwalletIn); }

    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn) { Init(pwalletIn); }

    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn) { Init(pwalletIn); }

    void Init(const CWallet* pwalletIn);

    IMPLEMENT_SERIALIZE(
        CWalletTx* pthis  = const_cast<CWalletTx*>(this); if (fRead) pthis->Init(nullptr);
        char       fSpent = false;

        if (!fRead) {
            pthis->mapValue["fromaccount"] = pthis->strFromAccount;

            std::string str;
            for (char f : vfSpent) {
                str += (f ? '1' : '0');
                if (f)
                    fSpent = true;
            }
            pthis->mapValue["spent"] = str;

            WriteOrderPos(pthis->nOrderPos, pthis->mapValue);

            if (nTimeSmart)
                pthis->mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        nSerSize += SerReadWrite(s, *(CMerkleTx*)this, nType, nVersion, ser_action);
        // clang-format off
        READWRITE(vtxPrev);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);
        // clang-format on

        if (fRead) {
            pthis->strFromAccount = pthis->mapValue["fromaccount"];

            if (mapValue.count("spent"))
                for (char c : pthis->mapValue["spent"])
                    pthis->vfSpent.push_back(c != '0');
            else
                pthis->vfSpent.assign(vout.size(), fSpent);

            ReadOrderPos(pthis->nOrderPos, pthis->mapValue);

            pthis->nTimeSmart =
                mapValue.count("timesmart") ? (unsigned int)atoi64(pthis->mapValue["timesmart"]) : 0;
        }

        pthis->mapValue.erase("fromaccount");
        pthis->mapValue.erase("version"); pthis->mapValue.erase("spent"); pthis->mapValue.erase("n");
        pthis->mapValue.erase("timesmart");)

    // make sure balances are recalculated
    void MarkDirty();

    void BindWallet(CWallet* pwalletIn);

    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetAvailableCredit(bool fUseCache = true) const;
    CAmount GetUnspentCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(
        bool                fUseCache = true,
        const isminefilter& filter = static_cast<isminefilter>(isminetype::ISMINE_SPENDABLE_ALL)) const;
    CAmount GetChange() const;

    void GetAmounts(std::list<std::pair<CTxDestination, CAmount>>& listReceived,
                    std::list<std::pair<CTxDestination, CAmount>>& listSent, CAmount& nFee,
                    std::string& strSentAccount, const isminefilter& filter) const;

    void GetAccountAmounts(const std::string& strAccount, CAmount& nReceived, CAmount& nSent,
                           CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const;

    bool IsTrusted() const;

    bool InMempool() const;

    int GetDepthAndMempool(bool& fConflicted) const;

    bool WriteToDisk(CWalletDB* pwalletdb);

    int64_t GetTxTime() const;
    int     GetRequestCount() const;

    void              RelayWalletTransaction();
    std::set<uint256> GetConflicts() const;
};

class COutput
{
public:
    const CWalletTx* tx;
    int              i;
    int              nDepth;

    COutput(const CWalletTx* txIn, int iIn, int nDepthIn)
    {
        tx     = txIn;
        i      = iIn;
        nDepth = nDepthIn;
    }

    std::string ToString() const
    {
        return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString().substr(0, 10).c_str(), i,
                         nDepth, FormatMoney(tx->vout[i].nValue).c_str());
    }

    void print() const { printf("%s\n", ToString().c_str()); }
};

/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey    vchPrivKey;
    int64_t     nTimeCreated;
    int64_t     nTimeExpires;
    std::string strComment;
    //// todo: add something to note what created it (user, getnewaddress, change)
    ////   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires = 0)
    {
        nTimeCreated = (nExpires ? GetTime() : 0);
        nTimeExpires = nExpires;
    }

    IMPLEMENT_SERIALIZE(if (!(nType & SER_GETHASH)) READWRITE(nVersion); READWRITE(vchPrivKey);
                        READWRITE(nTimeCreated); READWRITE(nTimeExpires); READWRITE(strComment);)
};

/** Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount() { SetNull(); }

    void SetNull() { vchPubKey = CPubKey(); }

    IMPLEMENT_SERIALIZE(if (!(nType & SER_GETHASH)) READWRITE(nVersion); READWRITE(vchPubKey);)
};

/** Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    int64_t     nCreditDebit;
    int64_t     nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t  mapValue;
    int64_t     nOrderPos; // position in ordered transaction list
    uint64_t    nEntryNo;

    CAccountingEntry() { SetNull(); }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime        = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
    }

    IMPLEMENT_SERIALIZE(
        CAccountingEntry& me = *const_cast<CAccountingEntry*>(this);
        if (!(nType & SER_GETHASH)) READWRITE(nVersion);
        // Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit); READWRITE(nTime); READWRITE(strOtherAccount);

        if (!fRead) {
            WriteOrderPos(nOrderPos, me.mapValue);

            if (!(mapValue.empty() && _ssExtra.empty())) {
                CDataStream ss(nType, nVersion);
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                me.strComment.append(ss.str());
            }
        }

        READWRITE(strComment);

        size_t nSepPos = strComment.find("\0", 0, 1); if (fRead) {
            me.mapValue.clear();
            if (std::string::npos != nSepPos) {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()),
                               nType, nVersion);
                ss >> me.mapValue;
                me._ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(me.nOrderPos, me.mapValue);
        } if (std::string::npos != nSepPos) me.strComment.erase(nSepPos);

        me.mapValue.erase("n");)

private:
    std::vector<char> _ssExtra;
};

bool GetWalletFile(CWallet* pwallet, std::string& strWalletFileOut);

#endif

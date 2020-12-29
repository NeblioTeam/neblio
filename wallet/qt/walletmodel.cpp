#include "walletmodel.h"
#include "addresstablemodel.h"
#include "coincontrol.h"
#include "guiconstants.h"
#include "main.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "guiutil.h"

#include "base58.h"
#include "ui_interface.h"
#include "wallet.h"
#include "walletdb.h" // for BackupWallet

#include <QSet>
#include <QSettings>
#include <QTimer>

#include "init.h"
#include "ntp1/ntp1sendtxdata.h"
#include "ntp1/ntp1tools.h"

#include "udaddress.h"

WalletModel::WalletModel(CWallet* wallet, OptionsModel* optionsModel, QObject* parent)
    : QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
      transactionTableModel(0), cachedBalance(0), cachedStake(0), cachedUnconfirmedBalance(0),
      cachedImmatureBalance(0), cachedNumTransactions(0), cachedEncryptionStatus(Unencrypted),
      cachedNumBlocks(0)
{
    addressTableModel     = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &WalletModel::pollBalanceChanged);
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();

    qRegisterMetaType<QSharedPointer<BalancesWorker>>("QSharedPointer<BalancesWorker>");
    qRegisterMetaType<WalletModel*>("WalletModel*");

    balancesThread.setObjectName("neblio-balancesWorker"); // thread name
    balancesThread.start();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();

    balancesThread.quit();
    balancesThread.wait();
}

qint64 WalletModel::getBalance() const { return wallet->GetBalance(); }

qint64 WalletModel::getUnconfirmedBalance() const { return wallet->GetUnconfirmedBalance(); }

qint64 WalletModel::getStake() const { return wallet->GetStake(); }

qint64 WalletModel::getImmatureBalance() const { return wallet->GetImmatureBalance(); }

int WalletModel::getNumTransactions() const
{
    int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if (cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

int64_t WalletModel::getCreationTime() const { return wallet->nTimeFirstKey; }

void WalletModel::pollBalanceChanged()
{
    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if (!lockWallet)
        return;

    // Don't continue processing if the chain tip time is less than the first
    // key creation time as there is no need to iterate over the transaction
    // table model in this case.
    auto tip = pindexBest;
    if (pindexBest && tip->GetBlockTime() < getCreationTime())
        return;

    if (nBestHeight != cachedNumBlocks) {
        // Balance and number of transactions might have changed
        cachedNumBlocks = nBestHeight;

        checkBalanceChanged();

        // dynamically measure how long updating confirmations takes, and adjust the refresh rate of that
        using namespace std::chrono;
        high_resolution_clock::time_point t1 = high_resolution_clock::now();

        if (transactionTableModel) {
            transactionTableModel->updateConfirmations();
        }

        // the other part of dynamic setting of refresh rate
        high_resolution_clock::time_point t2 = high_resolution_clock::now();
        int duration = static_cast<int>(duration_cast<milliseconds>(t2 - t1).count());
        if (duration < 250) {
            MODEL_UPDATE_DELAY.store(500);
        } else if (duration > 10000) {
            MODEL_UPDATE_DELAY.store(30000);
        } else {
            MODEL_UPDATE_DELAY.store(duration * 3);
        }
        pollTimer->setInterval(MODEL_UPDATE_DELAY.load());
    }
}

void WalletModel::checkBalanceChanged()
{
    if (isBalancesWorkerRunning) {
        QTimer::singleShot(1000, this, &WalletModel::checkBalanceChanged);
        return;
    }

    isBalancesWorkerRunning = true;

    QSharedPointer<BalancesWorker> worker = QSharedPointer<BalancesWorker>::create();
    worker->moveToThread(&balancesThread);
    connect(worker.data(), &BalancesWorker::resultReady, this, &WalletModel::updateBalancesIfChanged,
            Qt::QueuedConnection);
    GUIUtil::AsyncQtCall(worker.data(), [this, worker]() { worker->getBalances(this, worker); });
}

QThread* WalletModel::getBalancesThread() { return &balancesThread; }

void WalletModel::updateBalancesIfChanged(qint64 newBalance, qint64 newStake,
                                          qint64 newUnconfirmedBalance, qint64 newImmatureBalance)
{
    // force sync since we got a vector from another thread
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (!firstUpdateOfBalanceDone || cachedBalance != newBalance || cachedStake != newStake ||
        cachedUnconfirmedBalance != newUnconfirmedBalance ||
        cachedImmatureBalance != newImmatureBalance) {
        firstUpdateOfBalanceDone = true;
        cachedBalance            = newBalance;
        cachedStake              = newStake;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance    = newImmatureBalance;
        emit balanceChanged(newBalance, newStake, newUnconfirmedBalance, newImmatureBalance);
    }

    isBalancesWorkerRunning = false;
}

void WalletModel::updateTransaction(const QString& hash, int status)
{
    if (transactionTableModel)
        transactionTableModel->updateTransaction(hash, status);

    // Balance and number of transactions might have changed
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if (cachedNumTransactions != newNumTransactions) {
        cachedNumTransactions = newNumTransactions;
        emit numTransactionsChanged(newNumTransactions);
    }
}

void WalletModel::updateAddressBook(const QString& address, const QString& label, bool isMine,
                                    const QString& purpose, int status)
{
    if (addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, purpose, status);
}

bool WalletModel::validateAddress(const QString& address)
{
    std::string addressInputStr = address.toStdString();
    if (auto addr = GetNeblioAddressFromUDAddress(addressInputStr)) {
        CBitcoinAddress addressParsed(*addr);
        return addressParsed.IsValid();
    } else {
        CBitcoinAddress addressParsed(address.toStdString());
        return addressParsed.IsValid();
    }
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(QList<SendCoinsRecipient>        recipients,
                                                    boost::shared_ptr<NTP1Wallet>    ntp1wallet,
                                                    const RawNTP1MetadataBeforeSend& ntp1metadata,
                                                    bool                             fSpendDelegated,
                                                    const CCoinControl*              coinControl)
{
    qint64  total = 0;
    QString hex;

    if (recipients.empty()) {
        return OK;
    }

    // convert UD domains to neblio addresses
    for (SendCoinsRecipient& rcp : recipients) {
        const std::string address = rcp.address.toStdString();
        if (IsUDAddressSyntaxValid(address)) {
            if (auto converted = GetNeblioAddressFromUDAddress(address)) {
                rcp.address = QString::fromStdString(*converted);
            } else {
                return InvalidAddress;
            }
        }
    }

    // Pre-check input data for validity
    for (const SendCoinsRecipient& rcp : recipients) {
        if (!validateAddress(rcp.address)) {
            return InvalidAddress;
        }

        if (rcp.amount <= 0) {
            return InvalidAmount;
        }
        if (rcp.tokenId == QString::fromStdString(NTP1SendTxData::NEBL_TOKEN_ID)) {
            // add only nebl amounts
            total += rcp.amount;
        }
    }

    int64_t              nBalance = 0;
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, coinControl);

    for (const COutput& out : vCoins) {
        nBalance += out.tx->vout[out.i].nValue;
    }

    if (total > nBalance) {
        return AmountExceedsBalance;
    }

    if ((total + nTransactionFee) > nBalance) {
        return SendCoinsReturn(AmountWithFeeExceedsBalance, nTransactionFee);
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        // This first time selection is used adds recipients and the amounts to the selector
        bool           takeInputsFromCoinControl = coinControl != nullptr && coinControl->HasSelected();
        NTP1SendTxData tokenCalculator;
        {
            // create recipients list
            std::vector<NTP1SendTokensOneRecipientData> ntp1recipients;
            std::transform(recipients.begin(), recipients.end(), std::back_inserter(ntp1recipients),
                           [](const SendCoinsRecipient& r) {
                               NTP1SendTokensOneRecipientData res;
                               res.amount      = r.amount;
                               res.destination = r.address.toStdString();
                               res.tokenId     = r.tokenId.toStdString();
                               return res;
                           });

            try {
                tokenCalculator.selectNTP1Tokens(
                    ntp1wallet,
                    (takeInputsFromCoinControl ? coinControl->GetSelected() : std::vector<COutPoint>()),
                    ntp1recipients, !takeInputsFromCoinControl);
            } catch (std::exception& ex) {
                SendCoinsReturn ret(StatusCode::NTP1TokenCalculationsFailed);
                ret.msg =
                    QString("Could not reserve the outputs necessary for spending. Error: ") + ex.what();
                return ret;
            }
        }

        // Sendmany
        std::vector<std::pair<CScript, int64_t>> vecSend;
        foreach (const SendCoinsRecipient& rcp, recipients) {
            CScript scriptPubKey;
            scriptPubKey.SetDestination(CBitcoinAddress(rcp.address.toStdString()).Get());
            // here we add only nebls. NTP1 tokens will be added at CreateTransaction()
            if (rcp.tokenId == QString::fromStdString(NTP1SendTxData::NEBL_TOKEN_ID)) {
                vecSend.push_back(make_pair(scriptPubKey, rcp.amount));
            }
        }

        CWalletTx   wtx;
        CReserveKey keyChange(wallet);
        int64_t     nFeeRequired = 0;
        std::string errorMsg;
        const bool  fCreated =
            wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, tokenCalculator,
                                      ntp1metadata, false, coinControl, &errorMsg, fSpendDelegated);

        if (!fCreated) {
            if ((total + nFeeRequired) > nBalance) // FIXME: could cause collisions in the future
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance, nFeeRequired);
            }
            SendCoinsReturn ret(TransactionCreationFailed);
            ret.msg = QString::fromStdString("Error while creating the transaction: " + errorMsg);
            return ret;
        }
        // verify the NTP1 transaction before commiting
        try {
            std::vector<std::pair<CTransaction, NTP1Transaction>> inputsTxs =
                NTP1Transaction::GetAllNTP1InputsOfTx(wtx, false);
            NTP1Transaction ntp1tx;
            ntp1tx.readNTP1DataFromTx(wtx, inputsTxs);
        } catch (std::exception& ex) {
            printf("An invalid NTP1 transaction was created; an exception was thrown: %s\n", ex.what());
            SendCoinsReturn ret(StatusCode::NTP1TokenCalculationsFailed);
            ret.msg =
                "Unable to create the transaction. The transaction created would result in an invalid "
                "transaction. Please report your transaction details to the Neblio team. The "
                "error is: " +
                QString(ex.what());
            return ret;
        }
        if (!uiInterface.ThreadSafeAskFee(nFeeRequired, tr("Sending...").toStdString())) {
            return Aborted;
        }
        if (!wallet->CommitTransaction(wtx, keyChange)) {
            return TransactionCommitFailed;
        }
        hex = QString::fromStdString(wtx.GetHash().GetHex());
    }

    // Add addresses / update labels that we've sent to to the address book
    foreach (const SendCoinsRecipient& rcp, recipients) {
        std::string    strAddress = rcp.address.toStdString();
        CTxDestination dest       = CBitcoinAddress(strAddress).Get();
        std::string    strLabel   = rcp.label.toStdString();
        {
            auto mi = wallet->mapAddressBook.get(dest);

            // Check if we have a new address or an updated label
            if (!mi.is_initialized() || mi->name != strLabel) {
                wallet->SetAddressBookEntry(dest, strLabel);
            }
        }
    }

    return SendCoinsReturn(OK, 0, hex);
}

OptionsModel* WalletModel::getOptionsModel() { return optionsModel; }

AddressTableModel* WalletModel::getAddressTableModel() { return addressTableModel; }

TransactionTableModel* WalletModel::getTransactionTableModel() { return transactionTableModel; }

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if (!wallet->IsCrypted()) {
        return Unencrypted;
    } else if (wallet->IsLocked()) {
        return Locked;
    } else {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString& passphrase)
{
    if (encrypted) {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    } else {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString& passPhrase)
{
    if (locked) {
        // Lock
        return wallet->Lock();
    } else {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString& oldPass, const SecureString& newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString& filename)
{
    return BackupWallet(*wallet, filename.toLocal8Bit().data());
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel* walletmodel, CCryptoKeyStore* /*wallet*/)
{
    OutputDebugStringF("NotifyKeyStoreStatusChanged\n");
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel*          walletmodel, CWallet* /*wallet*/,
                                     const CTxDestination& address, const std::string& label,
                                     bool isMine, const std::string& purpose, ChangeType status)
{
    OutputDebugStringF("NotifyAddressBookChanged %s %s isMine=%i purpose=%s status=%i\n",
                       CBitcoinAddress(address).ToString().c_str(), label.c_str(), isMine,
                       purpose.c_str(), status);
    QMetaObject::invokeMethod(
        walletmodel, "updateAddressBook", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(CBitcoinAddress(address).ToString())),
        Q_ARG(QString, QString::fromStdString(label)), Q_ARG(bool, isMine),
        Q_ARG(QString, QString::fromStdString(purpose)), Q_ARG(int, status));
}

static void NotifyTransactionChanged(WalletModel* walletmodel, CWallet* /*wallet*/, const uint256& hash,
                                     ChangeType status)
{
    OutputDebugStringF("NotifyTransactionChanged %s status=%i\n", hash.GetHex().c_str(), status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(hash.GetHex())), Q_ARG(int, status));
}

void WalletModel::subscribeToCoreSignals()
{
    using namespace boost::placeholders;

    // Connect signals to wallet
    wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.connect(
        boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    using namespace boost::placeholders;

    // Disconnect signals from wallet
    wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.disconnect(
        boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;

    if ((!was_locked) && fWalletUnlockStakingOnly) {
        setWalletLocked(true);
        was_locked = getEncryptionStatus() == Locked;
    }
    if (was_locked) {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked && !fWalletUnlockStakingOnly);
}

WalletModel::UnlockContext::UnlockContext(WalletModel* wallet, bool valid, bool relock)
    : wallet(wallet), valid(valid), relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if (valid && relock) {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this      = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const
{
    return wallet->GetPubKey(address, vchPubKeyOut);
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    LOCK2(cs_main, wallet->cs_wallet);
    for (const COutPoint& outpoint : vOutpoints) {
        if (!wallet->mapWallet.count(outpoint.hash))
            continue;
        bool fConflicted = false;
        int  nDepth      = wallet->mapWallet[outpoint.hash].GetDepthAndMempool(fConflicted);
        if (nDepth < 0 || fConflicted)
            continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vOutputs.push_back(out);
    }
}

// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address)
void WalletModel::listCoins(std::map<QString, std::vector<COutput>>& mapCoins) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins);

    LOCK2(cs_main, wallet->cs_wallet); // ListLockedCoins, mapWallet
    std::vector<COutPoint> vLockedCoins;

    // add locked coins
    for (const COutPoint& outpoint : vLockedCoins) {
        if (!wallet->mapWallet.count(outpoint.hash))
            continue;
        bool fConflicted = false;
        int  nDepth      = wallet->mapWallet[outpoint.hash].GetDepthAndMempool(fConflicted);
        if (nDepth < 0 || fConflicted)
            continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vCoins.push_back(out);
    }

    for (const COutput& out : vCoins) {
        COutput cout = out;

        while (wallet->IsChange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0 &&
               IsMineCheck(wallet->IsMine(cout.tx->vin[0]), isminetype::ISMINE_SPENDABLE_ALL)) {
            if (!wallet->mapWallet.count(cout.tx->vin[0].prevout.hash))
                break;
            cout =
                COutput(&wallet->mapWallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0);
        }

        CTxDestination address;
        if (!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address))
            continue;
        mapCoins[CBitcoinAddress(address).ToString().c_str()].push_back(out);
    }
}

bool WalletModel::isLockedCoin(uint256 /*hash*/, unsigned int /*n*/) const { return false; }

void WalletModel::lockCoin(COutPoint& /*output*/) { return; }

void WalletModel::unlockCoin(COutPoint& /*output*/) { return; }

void WalletModel::listLockedCoins(std::vector<COutPoint>& /*vOutpts*/) { return; }

bool WalletModel::whitelistAddressFromColdStaking(const QString& addressStr)
{
    return updateAddressBookPurpose(addressStr, AddressBook::AddressBookPurpose::DELEGATOR);
}

bool WalletModel::blacklistAddressFromColdStaking(const QString& addressStr)
{
    return updateAddressBookPurpose(addressStr, AddressBook::AddressBookPurpose::DELEGABLE);
}

bool WalletModel::updateAddressBookPurpose(const QString& addressStr, const std::string& purpose)
{
    CBitcoinAddress address(addressStr.toStdString());
    CKeyID          keyID;
    if (!getKeyId(address, keyID))
        return false;
    return pwalletMain->SetAddressBookEntry(keyID, getLabelForAddress(address), purpose);
}

std::string WalletModel::getLabelForAddress(const CBitcoinAddress& address)
{
    std::string label = "";
    {
        const auto mi = wallet->mapAddressBook.get(address.Get());
        if (mi.is_initialized()) {
            label = mi->name;
        }
    }
    return label;
}

bool WalletModel::getKeyId(const CBitcoinAddress& address, CKeyID& keyID)
{
    if (!address.IsValid())
        return ::error("Invalid neblio address: %s", address.ToString().c_str());

    if (!address.GetKeyID(keyID))
        return ::error("Unable to get KeyID from neblio address: %s", address.ToString().c_str());

    return true;
}

CWallet* WalletModel::getWallet() { return wallet; }

void BalancesWorker::getBalances(WalletModel* walletModel, QSharedPointer<BalancesWorker> workerPtr)
{
    const qint64 newBalance            = walletModel->getBalance();
    const qint64 newStake              = walletModel->getStake();
    const qint64 newUnconfirmedBalance = walletModel->getUnconfirmedBalance();
    const qint64 newImmatureBalance    = walletModel->getImmatureBalance();

    emit resultReady(newBalance, newStake, newUnconfirmedBalance, newImmatureBalance);
    workerPtr.reset();
}

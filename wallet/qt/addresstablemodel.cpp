#include "addresstablemodel.h"
#include "guiutil.h"
#include "ledger/bip32.h"
#include "ledger/error.h"
#include "ledger/utils.h"
#include "ledgerBridge.h"
#include "walletmodel.h"

#include "base58.h"
#include "wallet.h"

#include <boost/variant.hpp>

#include <QColor>
#include <QFont>

const QString AddressTableModel::Send          = "S";
const QString AddressTableModel::Receive       = "R";
const QString AddressTableModel::ReceiveLedger = "RL";
// proxyModel->setFilterFixedString(AddressTableModel::Receive);

struct AddressTableEntry
{
    enum Type
    {
        Sending,
        Receiving,
        ReceivingLedger
    };

    Type     type;
    QString  label;
    QString  address;
    uint32_t ledgerAccount;
    uint32_t ledgerIndex;

    AddressTableEntry() {}
    AddressTableEntry(Type typeIn, const QString& labelIn, const QString& addressIn,
                      uint32_t ledgerAccountIn, uint32_t ledgerIndexIn)
        : type(typeIn), label(labelIn), address(addressIn), ledgerAccount(ledgerAccountIn),
          ledgerIndex(ledgerIndexIn)
    {
    }
};

struct AddressTableEntryLessThan
{
    bool operator()(const AddressTableEntry& a, const AddressTableEntry& b) const
    {
        return a.address < b.address;
    }
    bool operator()(const AddressTableEntry& a, const QString& b) const { return a.address < b; }
    bool operator()(const QString& a, const AddressTableEntry& b) const { return a < b.address; }
};

AddressTableEntry::Type GetEntryType(isminetype fMine)
{
    if (fMine == isminetype::ISMINE_LEDGER) {
        return AddressTableEntry::ReceivingLedger;
    }

    if (fMine) {
        return AddressTableEntry::Receiving;
    }

    return AddressTableEntry::Sending;
}

// Private implementation
class AddressTablePriv
{
public:
    CWallet*                 wallet;
    QList<AddressTableEntry> cachedAddressTable;
    AddressTableModel*       parent;

    AddressTablePriv(CWallet* walletIn, AddressTableModel* parentIn) : wallet(walletIn), parent(parentIn)
    {
    }

    void refreshAddressTable()
    {
        cachedAddressTable.clear();
        {
            const auto addressBookMap = wallet->mapAddressBook.getInternalMap();
            for (const auto& item : addressBookMap) {
                const CBitcoinAddress&  address = item.first;
                const std::string&      strName = item.second.name;
                isminetype              fMine   = IsMine(*wallet, address.Get());
                AddressTableEntry::Type type    = GetEntryType(fMine);

                CKeyID     ledgerKedId;
                CLedgerKey ledgerKey;
                if (type == AddressTableEntry::ReceivingLedger) {
                    address.GetKeyID(ledgerKedId);
                    wallet->GetLedgerKey(ledgerKedId, ledgerKey);
                }

                cachedAddressTable.append(AddressTableEntry(type, QString::fromStdString(strName),
                                                            QString::fromStdString(address.ToString()),
                                                            ledgerKey.account, ledgerKey.index));
            }
        }
        // qLowerBound() and qUpperBound() require our cachedAddressTable list to be sorted in asc order
        std::sort(cachedAddressTable.begin(), cachedAddressTable.end(), AddressTableEntryLessThan());
    }

    void updateEntry(const QString& address, const QString& label, isminetype isMine,
                     const QString& /*purpose*/, int status)
    {
        // Find address / label in model
        QList<AddressTableEntry>::iterator lower = std::lower_bound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        QList<AddressTableEntry>::iterator upper = std::upper_bound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        int                     lowerIndex   = (lower - cachedAddressTable.begin());
        int                     upperIndex   = (upper - cachedAddressTable.begin());
        bool                    inModel      = (lower != upper);
        AddressTableEntry::Type newEntryType = GetEntryType(isMine);

        switch (status) {
        case CT_NEW:
            if (inModel) {
                NLog.write(b_sev::warn,
                           "Warning: AddressTablePriv::updateEntry: Got CT_NEW, but entry is "
                           "already in model");
                break;
            }
            {
                CKeyID     ledgerKeyId;
                CLedgerKey ledgerKey;
                if (newEntryType == AddressTableEntry::ReceivingLedger) {
                    CBitcoinAddress(address.toStdString()).GetKeyID(ledgerKeyId);
                    wallet->GetLedgerKey(ledgerKeyId, ledgerKey);
                }

                parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
                cachedAddressTable.insert(
                    lowerIndex,
                    AddressTableEntry(newEntryType, label, address, ledgerKey.account, ledgerKey.index));
                parent->endInsertRows();
            }
            break;
        case CT_UPDATED:
            if (!inModel) {
                NLog.write(b_sev::warn,
                           "Warning: AddressTablePriv::updateEntry: Got CT_UPDATED, but entry "
                           "is not in model");
                break;
            }
            lower->type  = newEntryType;
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if (!inModel) {
                NLog.write(b_sev::warn,
                           "Warning: AddressTablePriv::updateEntry: Got CT_DELETED, but entry "
                           "is not in model");
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex - 1);
            cachedAddressTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size() { return cachedAddressTable.size(); }

    AddressTableEntry* index(int idx)
    {
        if (idx >= 0 && idx < cachedAddressTable.size()) {
            return &cachedAddressTable[idx];
        } else {
            return 0;
        }
    }
};

AddressTableModel::AddressTableModel(CWallet* walletIn, WalletModel* parent)
    : QAbstractTableModel(parent), walletModel(parent), wallet(walletIn), priv(0)
{
    columns << tr("Label") << tr("Address") << tr("Is Ledger") << tr("Ledger Account")
            << tr("Ledger Index") << tr("Ledger Path");
    priv = new AddressTablePriv(walletIn, this);
    priv->refreshAddressTable();
}

AddressTableModel::~AddressTableModel() { delete priv; }

int AddressTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int AddressTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant AddressTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        bool isLedger = rec->type == AddressTableEntry::ReceivingLedger;
        switch (index.column()) {
        case Label:
            if (rec->label.isEmpty() && role == Qt::DisplayRole) {
                return tr("(no label)");
            } else {
                return rec->label;
            }
        case Address:
            return rec->address;
        case IsLedger:
            return isLedger ? tr("Yes") : "";
        case LedgerAccount:
            return isLedger ? QString::number(rec->ledgerAccount) : "";
        case LedgerIndex:
            return isLedger ? QString::number(rec->ledgerIndex) : "";
        case LedgerPath: {
            // IsLedger, LedgerAccount and LedgerIndex are hidden, we display the info here
            // (however, they still need to be above due to EditAddressDialog data mapping)
            std::string path = ledger::Bip32Path(rec->ledgerAccount, false, rec->ledgerIndex).ToString();
            return isLedger ? QString::fromStdString(path) : "-";
        }
        }
    } else if (role == Qt::FontRole) {
        if (index.column() == Address) {
            return GUIUtil::monospaceFont();
        }
        if (index.column() == LedgerPath) {
            return GUIUtil::monospaceFont();
        }
        return QVariant();
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == LedgerPath) {
            return Qt::AlignCenter;
        }
        return QVariant();
    } else if (role == TypeRole) {
        switch (rec->type) {
        case AddressTableEntry::Sending:
            return Send;
        case AddressTableEntry::Receiving:
            return Receive;
        case AddressTableEntry::ReceivingLedger:
            return ReceiveLedger;
        default:
            break;
        }
    }
    return QVariant();
}

/* Look up purpose for address in address book
 */
std::string AddressTableModel::purposeForAddress(const std::string& address) const
{
    return wallet->purposeForAddress(CBitcoinAddress(address).Get());
}

bool AddressTableModel::isWhitelisted(const std::string& address) const
{
    return purposeForAddress(address).compare(AddressBook::AddressBookPurpose::DELEGATOR) == 0;
}

bool AddressTableModel::checkLabelAvailability(const QString& label, bool isLedgerAddress)
{
    auto labelAvailability = wallet->CheckLabelAvailability(label.toStdString(), isLedgerAddress);
    if (labelAvailability == LabelAvailability::USED_BY_LEDGER) {
        editStatus = LABEL_USED_BY_LEDGER;
        return false;
    }
    if (isLedgerAddress && labelAvailability == LabelAvailability::NOT_USABLE_FOR_LEDGER) {
        editStatus = LABEL_NOT_USABLE_FOR_LEDGER;
        return false;
    }
    return true;
}

bool AddressTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;
    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());

    editStatus = OK;

    if (role == Qt::EditRole) {
        switch (index.column()) {
        case Label:
            // Do nothing, if old label == new label
            if (rec->label == value.toString()) {
                editStatus = NO_CHANGES;
                return false;
            }
            if (!checkLabelAvailability(value.toString(),
                                        rec->type == AddressTableEntry::ReceivingLedger))
                return false;
            wallet->SetAddressBookEntry(CBitcoinAddress(rec->address.toStdString()).Get(),
                                        value.toString().toStdString());
            break;
        case Address:
            // Do nothing, if old address == new address
            if (CBitcoinAddress(rec->address.toStdString()) ==
                CBitcoinAddress(value.toString().toStdString())) {
                editStatus = NO_CHANGES;
                return false;
            }
            // Refuse to set invalid address, set error status and return false
            else if (!walletModel->validateAddress(value.toString())) {
                editStatus = INVALID_ADDRESS;
                return false;
            }
            // Check for duplicate addresses to prevent accidental deletion of addresses, if you try
            // to paste an existing address over another address (with a different label)
            else if (wallet->mapAddressBook.exists(
                         CBitcoinAddress(value.toString().toStdString()).Get())) {
                editStatus = DUPLICATE_ADDRESS;
                return false;
            }
            // Double-check that we're not overwriting a receiving address
            else if (rec->type == AddressTableEntry::Sending) {
                {
                    LOCK(wallet->cs_wallet);
                    // Remove old entry
                    wallet->DelAddressBookName(CBitcoinAddress(rec->address.toStdString()).Get());
                    // Add new entry with new address
                    wallet->SetAddressBookEntry(CBitcoinAddress(value.toString().toStdString()).Get(),
                                                rec->label.toStdString());
                }
            }
            break;
        }
        return true;
    }
    return false;
}

QVariant AddressTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags AddressTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    // Can edit label and address for sending addresses,
    // and only label for receiving addresses.
    switch (rec->type) {
    case AddressTableEntry::Sending:
        if (index.column() == Label || index.column() == Address)
            retval |= Qt::ItemIsEditable;
        break;
    case AddressTableEntry::Receiving:
    case AddressTableEntry::ReceivingLedger:
        if (index.column() == Label)
            retval |= Qt::ItemIsEditable;
        break;
    default:
        break;
    }
    return retval;
}

QModelIndex AddressTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    AddressTableEntry* data = priv->index(row);
    if (data) {
        return createIndex(row, column, priv->index(row));
    } else {
        return QModelIndex();
    }
}

void AddressTableModel::updateEntry(const QString& address, const QString& label, isminetype isMine,
                                    const QString& purpose, int status)
{
    // Update address book model from Bitcoin core
    priv->updateEntry(address, label, isMine, purpose, status);
}

QString AddressTableModel::addRow(const QString& type, const QString& label, const QString& address,
                                  const QString& ledgerAccount, const QString& ledgerIndex)
{
    std::string strLabel   = label.toStdString();
    std::string strAddress = address.toStdString();

    editStatus = OK;

    if (!checkLabelAvailability(label, type == ReceiveLedger))
        return QString();

    if (type == Send) {
        if (!walletModel->validateAddress(address)) {
            editStatus = INVALID_ADDRESS;
            return QString();
        }
        // Check for duplicate addresses
        {
            if (wallet->mapAddressBook.exists(CBitcoinAddress(strAddress).Get())) {
                editStatus = DUPLICATE_ADDRESS;
                return QString();
            }
        }
    } else if (type == Receive) {
        // Generate a new address to associate with given label
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());
        if (!ctx.isValid()) {
            // Unlock wallet failed or was cancelled
            editStatus = WALLET_UNLOCK_FAILURE;
            return QString();
        }
        CPubKey newKey;
        if (!wallet->GetKeyFromPool(newKey)) {
            editStatus = KEY_GENERATION_FAILURE;
            return QString();
        }
        strAddress = CBitcoinAddress(newKey.GetID()).ToString();
    } else if (type == ReceiveLedger) {
        bool     accountOk = true;
        uint32_t account   = ledgerAccount.toUInt(&accountOk);
        if (!accountOk || !ledgerbridge::LedgerBridge::ValidateAccountIndex(account)) {
            editStatus = INVALID_LEDGER_ACCOUNT;
            return QString();
        }

        bool     indexOk = true;
        uint32_t index   = ledgerIndex.toUInt(&indexOk);
        if (!indexOk || !ledgerbridge::LedgerBridge::ValidateAddressIndex(index)) {
            editStatus = INVALID_LEDGER_INDEX;
            return QString();
        }

        try {
            strAddress = wallet->ImportLedgerKey(account, index);
        } catch (const ledger::LedgerException& e) {
            editStatus  = LEDGER_ERROR;
            ledgerError = e.GetErrorCode();
            return QString();
        }
    } else {
        return QString();
    }

    // Add entry
    {
        LOCK(wallet->cs_wallet);
        // Double-check label availability (now that we have acquired the lock)
        if (!checkLabelAvailability(label, type == ReceiveLedger))
            return QString();
        wallet->SetAddressBookEntry(CBitcoinAddress(strAddress).Get(), strLabel);
    }
    return QString::fromStdString(strAddress);
}

bool AddressTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_UNUSED(parent);
    AddressTableEntry* rec = priv->index(row);
    if (count != 1 || !rec || rec->type == AddressTableEntry::Receiving ||
        rec->type == AddressTableEntry::ReceivingLedger) {
        // Can only remove one row at a time, and cannot remove rows not in model.
        // Also refuse to remove receiving addresses.
        return false;
    }
    {
        LOCK(wallet->cs_wallet);
        wallet->DelAddressBookName(CBitcoinAddress(rec->address.toStdString()).Get());
    }
    return true;
}

/* Look up label for address in address book, if not found return empty string.
 */
QString AddressTableModel::labelForAddress(const QString& address) const
{
    {
        CBitcoinAddress address_parsed(address.toStdString());

        const auto mi = wallet->mapAddressBook.get(address_parsed.Get());
        if (mi.is_initialized()) {
            return QString::fromStdString(mi->name);
        }
    }
    return QString();
}

int AddressTableModel::lookupAddress(const QString& address) const
{
    QModelIndexList lst =
        match(index(0, Address, QModelIndex()), Qt::EditRole, address, 1, Qt::MatchExactly);
    if (lst.isEmpty()) {
        return -1;
    } else {
        return lst.at(0).row();
    }
}

void AddressTableModel::emitDataChanged(int idx)
{
    emit dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length() - 1, QModelIndex()));
}

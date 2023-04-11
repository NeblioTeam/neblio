#ifndef ADDRESSTABLEMODEL_H
#define ADDRESSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include "ledger/error.h"
#include "wallet_ismine.h"

class AddressTablePriv;
class CWallet;
class WalletModel;

/**
   Qt model of the address book in the core. This allows views to access and modify the address book.
 */
class AddressTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit AddressTableModel(CWallet* walletIn, WalletModel* parent = 0);
    ~AddressTableModel();

    enum ColumnIndex
    {
        Label   = 0, /**< User specified label */
        Address = 1,  /**< Bitcoin address */
        IsLedger = 2,
        LedgerAccount = 3,
        LedgerIndex = 4,
        LedgerPath = 5,
    };

    enum RoleIndex
    {
        TypeRole = Qt::UserRole /**< Type of address (#Send or #Receive) */
    };

    /** Return status of edit/insert operation */
    enum EditStatus
    {
        OK,                          /**< Everything ok */
        NO_CHANGES,                  /**< No changes were made during edit operation */
        INVALID_ADDRESS,             /**< Unparseable address */
        DUPLICATE_ADDRESS,           /**< Address already in address book */
        INVALID_LEDGER_ACCOUNT,      /**< Ledger account outside of recommended range */
        INVALID_LEDGER_INDEX,        /**< Ledger index outside of recommended range */
        WALLET_UNLOCK_FAILURE,       /**< Wallet could not be unlocked to create new receiving address */
        KEY_GENERATION_FAILURE,      /**< Generating a new public key for a receiving address failed */
        LABEL_USED_BY_LEDGER,        /**< Using this label would break Ledger's unique label requirement */
        LABEL_NOT_USABLE_FOR_LEDGER, /**< Ledger address requires a fresh, unique label */
        LEDGER_ERROR                 /**< Ledger operation error */
    };

    static const QString Send;    /**< Specifies send address */
    static const QString Receive; /**< Specifies receive address */
    static const QString ReceiveLedger; /**< Specifies Ledger address */

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int           rowCount(const QModelIndex& parent) const;
    int           columnCount(const QModelIndex& parent) const;
    QVariant      data(const QModelIndex& index, int role) const;
    bool          setData(const QModelIndex& index, const QVariant& value, int role);
    QVariant      headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex   index(int row, int column, const QModelIndex& parent) const;
    bool          removeRows(int row, int count, const QModelIndex& parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex& index) const;
    /*@}*/

    /* Add an address to the model.
       Returns the added address on success, and an empty string otherwise.
     */
    QString addRow(const QString& type, const QString& label, const QString& address, const QString& ledgerAccount, const QString& ledgerIndex);

    /* Look up label for address in address book, if not found return empty string.
     */
    QString labelForAddress(const QString& address) const;

    /* Look up row index of an address in the model.
       Return -1 if not found.
     */
    int lookupAddress(const QString& address) const;

    EditStatus getEditStatus() const { return editStatus; }

    ledger::ErrorCode getLedgerError() const { return ledgerError; }

    std::string purposeForAddress(const std::string& address) const;

    bool isWhitelisted(const std::string& address) const;

    bool isLabelUsedByLedger(const QString& label);
    bool isLabelUsableForLedger(const QString& label);
    bool checkLabelAvailability(const QString& label, bool isLedgerAddress);

private:
    WalletModel*      walletModel;
    CWallet*          wallet;
    AddressTablePriv* priv;
    QStringList       columns;
    EditStatus        editStatus;
    ledger::ErrorCode ledgerError;

    /** Notify listeners that data changed. */
    void emitDataChanged(int index);

    bool validateLedgerPathItem(uint32_t value, uint32_t top) const;

signals:
    void defaultAddressChanged(const QString& address);

public slots:
    /* Update address list from core.
     */
    void updateEntry(const QString& address, const QString& label, isminetype isMine, const QString& purpose,
                     int status);

    friend class AddressTablePriv;
};

#endif // ADDRESSTABLEMODEL_H

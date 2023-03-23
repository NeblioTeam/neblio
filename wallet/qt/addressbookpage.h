#ifndef ADDRESSBOOKPAGE_H
#define ADDRESSBOOKPAGE_H

#include <QDialog>

namespace Ui {
    class AddressBookPage;
}
class AddressTableModel;
class OptionsModel;

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

/** Widget that shows a list of sending or receiving addresses.
  */
class AddressBookPage : public QDialog
{
    Q_OBJECT

public:
    enum Tabs {
        SendingTab = 0,   /**< Tab for destination addresses */
        ReceivingTab = 1, /**< Tab for addresses controlled by the app or Ledger device */
        LedgerTab = 2,    /**< Tab for source addresses controlled by Ledger device */
    };

    enum Mode {
        ForSending, /**< Open address book to pick a destination address */
        ForEditing  /**< Open address book for editing */
    };

    explicit AddressBookPage(Mode modeIn, Tabs tabIn, QWidget *parent = 0);
    ~AddressBookPage();

    void setModel(AddressTableModel *modelIn);
    void setOptionsModel(OptionsModel *optionsModelIn);
    const QString &getReturnAddress() const { return returnAddress; }
    const QString &getReturnLabel() const { return returnLabel; }

public slots:
    void done(int retval);
    void exportClicked();

private:
    Ui::AddressBookPage *ui;
    AddressTableModel *model;
    OptionsModel *optionsModel;
    Mode mode;
    Tabs tab;
    QString returnAddress;
    QString returnLabel;
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;
    QAction *deleteAction;
    QString newAddressToSelect;

private slots:
    void on_deleteButton_clicked();
    void on_newAddressButton_clicked();
    /** Copy address of currently selected address entry to clipboard */
    void on_copyToClipboard_clicked();
    void on_signMessage_clicked();
    void on_verifyMessage_clicked();
    void selectionChanged();
    void on_showQRCode_clicked();
    /** Spawn contextual menu (right mouse menu) for address book entry */
    void contextualMenu(const QPoint &point);

    /** Copy label of currently selected address entry to clipboard */
    void onCopyLabelAction();
    /** Edit currently selected address entry */
    void onEditAction();

    /** New entry/entries were added to address table */
    void selectNewAddress(const QModelIndex &parent, int begin, int end);

signals:
    void signMessage(QString addr);
    void verifyMessage(QString addr);
};

#endif // ADDRESSBOOKDIALOG_H

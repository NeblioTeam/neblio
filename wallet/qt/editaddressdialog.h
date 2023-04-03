#ifndef EDITADDRESSDIALOG_H
#define EDITADDRESSDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui {
    class EditAddressDialog;
}
class AddressTableModel;

/* Object for adding a Ledger address row in a separate thread.
*/
class AddLedgerRowWorker : public QObject
{
    Q_OBJECT

public slots:
    // we use the shared pointer argument to ensure that workerPtr will be deleted after doing the
    // retrieval
    void addRow(Ui::EditAddressDialog *ui, AddressTableModel *model, QSharedPointer<AddLedgerRowWorker> workerPtr);

signals:
    void resultReady(QString);
};

/** Dialog for editing an address and associated information.
 */
class EditAddressDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        NewReceivingAddress,
        NewSendingAddress,
        EditReceivingAddress,
        EditSendingAddress,
    };

    explicit EditAddressDialog(Mode modeIn, QWidget *parent = 0);
    ~EditAddressDialog();

    void setModel(AddressTableModel *model);
    void loadRow(int row);

    QString getAddress() const;
    void setAddressEditValue(const QString &addressIn);

public slots:
    void accept();
    void updateLedgerPathLabel();
    void setAddress(QString addressIn);
    void on_ledgerCheckBox_toggled(bool checked);

private:
    bool saveCurrentRow();

    Ui::EditAddressDialog *ui;
    QDataWidgetMapper *mapper;
    Mode mode;
    AddressTableModel *model;
    bool ledgerItemsVisible;

    QString address;
};

#endif // EDITADDRESSDIALOG_H

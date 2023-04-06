#include "editaddressdialog.h"
#include "ui_editaddressdialog.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "ledger/bip32.h"
#include <ledger/messagebox.h>
#include "ledger/utils.h"

#include <QDataWidgetMapper>
#include <QtGui/QIntValidator>
#include <QTimer>

void AddLedgerRowWorker::addRow(Ui::EditAddressDialog *ui, AddressTableModel *model, QSharedPointer<AddLedgerRowWorker> workerPtr) {
    auto address = model->addRow(
        AddressTableModel::ReceiveLedger,
        ui->labelEdit->text(),
        ui->addressEdit->text(),
        ui->ledgerAccountEdit->text(),
        ui->ledgerIndexEdit->text()
    );

    emit resultReady(address);
    workerPtr.reset();
}

EditAddressDialog::EditAddressDialog(Mode modeIn, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditAddressDialog), mapper(0), mode(modeIn), model(0)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->addressEdit, this);

    bool ledgerItemsEnabled = true;
    switch(modeIn)
    {
    case NewReceivingAddress:
        setWindowTitle(tr("New receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case NewSendingAddress:
        setWindowTitle(tr("New sending address"));
        ledgerItemsEnabled = false;
        break;
    case EditReceivingAddress:
        setWindowTitle(tr("Edit receiving address"));
        ui->addressEdit->setEnabled(false);
        ledgerItemsEnabled = false;
        break;
    case EditSendingAddress:
        setWindowTitle(tr("Edit sending address"));
        ledgerItemsEnabled = false;
        break;
    }

    if (!ledgerItemsEnabled) {
        ui->ledgerCheckBox->setEnabled(false);
        ui->ledgerAccountEdit->setEnabled(false);
        ui->ledgerIndexEdit->setEnabled(false);
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);

    // Ledger submenu collapsed by default
    ledgerItemsVisible = false;
    ui->ledgerWidget->setVisible(ledgerItemsVisible);
    ui->ledgerPathLabel->setVisible(ledgerItemsVisible);
    ui->ledgerInfoLabel->setVisible(ledgerItemsVisible);

    // Ledger account and index defaults and validators
    ui->ledgerAccountEdit->setText("0");
    ui->ledgerIndexEdit->setText("0");
    GUIUtil::setupIntWidget(ui->ledgerAccountEdit, this, 0, ledger::MAX_RECOMMENDED_ACCOUNT);
    GUIUtil::setupIntWidget(ui->ledgerIndexEdit, this, 0, ledger::MAX_RECOMMENDED_INDEX);

    connect(this->ui->ledgerAccountEdit, SIGNAL(textChanged(QString)), this, SLOT(updateLedgerPathLabel()));
    connect(this->ui->ledgerIndexEdit, SIGNAL(textChanged(QString)), this, SLOT(updateLedgerPathLabel()));
    updateLedgerPathLabel();
}

EditAddressDialog::~EditAddressDialog()
{
    delete ui;
}

void EditAddressDialog::setModel(AddressTableModel *modelIn)
{
    this->model = modelIn;
    if(!modelIn)
        return;

    mapper->setModel(modelIn);
    mapper->addMapping(ui->labelEdit, AddressTableModel::Label);
    mapper->addMapping(ui->addressEdit, AddressTableModel::Address);
    mapper->addMapping(ui->ledgerCheckBox, AddressTableModel::IsLedger);
    mapper->addMapping(ui->ledgerAccountEdit, AddressTableModel::LedgerAccount);
    mapper->addMapping(ui->ledgerIndexEdit, AddressTableModel::LedgerIndex);
}

void EditAddressDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditAddressDialog::saveCurrentRow()
{
    if(!model)
        return false;

    bool isLedger = ui->ledgerCheckBox->isChecked();
    switch(mode)
    {
    case NewReceivingAddress:
        if (isLedger)
        {
            QSharedPointer<AddLedgerRowWorker> worker = QSharedPointer<AddLedgerRowWorker>::create();
            ledger::MessageBox msgBox(this, worker);
            connect(worker.data(), SIGNAL(resultReady(QString)), this, SLOT(setAddress(QString)));
            connect(worker.data(), SIGNAL(resultReady(QString)), &msgBox, SLOT(quit()));
            QTimer::singleShot(0, worker.data(), [this, worker]() { worker->addRow(ui, model, worker); });
            msgBox.exec();
        } else {
            address = model->addRow(
                AddressTableModel::Receive,
                ui->labelEdit->text(),
                ui->addressEdit->text(),
                ui->ledgerAccountEdit->text(),
                ui->ledgerIndexEdit->text()
            );
        }
        break;
    case NewSendingAddress:
        address = model->addRow(
                AddressTableModel::Send,
                ui->labelEdit->text(),
                ui->addressEdit->text(),
                ui->ledgerAccountEdit->text(),
                ui->ledgerIndexEdit->text()
            );
        break;
    case EditReceivingAddress:
    case EditSendingAddress:
        if(mapper->submit())
        {
            address = ui->addressEdit->text();
        }
        break;
    }
    return !address.isEmpty();
}

void EditAddressDialog::accept()
{
    if(!model)
        return;

    if(!saveCurrentRow())
    {
        switch(model->getEditStatus())
        {
        case AddressTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case AddressTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case AddressTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not a valid neblio address.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is already in the address book.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::INVALID_LEDGER_ACCOUNT:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered Ledger account \"%1\" is not in the recommended range (0-%2).").arg(ui->ledgerAccountEdit->text()).arg(ledger::MAX_RECOMMENDED_ACCOUNT),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::INVALID_LEDGER_INDEX:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered Ledger address index \"%1\" is not in the recommended range (0-%2).").arg(ui->ledgerIndexEdit->text()).arg(ledger::MAX_RECOMMENDED_INDEX),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("Could not unlock wallet."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::KEY_GENERATION_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("New key generation failed."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::LABEL_USED_BY_LEDGER:
            QMessageBox::critical(this, windowTitle(),
                tr("This label is already used by a Ledger address."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::LABEL_NOT_USABLE_FOR_LEDGER:
            QMessageBox::critical(this, windowTitle(),
                tr("Ledger addresses require a unique label."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::LEDGER_ERROR:
            QMessageBox::critical(this, windowTitle(),
                tr("A Ledger error occured: %1\n\nIf you did not cancel the operation intentionally, make sure that your device is connected to the computer and the Neblio app is opened on the device.").arg(QString::fromStdString(model->getLedgerErrorMessage())),
                QMessageBox::Ok, QMessageBox::Ok);
            break;

        }
        return;
    }
    QDialog::accept();
}

void EditAddressDialog::updateLedgerPathLabel()
{
    if (!ledgerItemsVisible) {
        return;
    }

    auto path = ledger::Bip32Path(
        ui->ledgerAccountEdit->text().toStdString(),
        false,
        ui->ledgerIndexEdit->text().toStdString()
    ).ToString();

    ui->ledgerPathLabel->setText(tr("Ledger path: %1").arg(QString::fromStdString(path)));
}

void EditAddressDialog::setAddress(QString addressIn)
{
    this->address = addressIn;
}

void EditAddressDialog::on_ledgerCheckBox_toggled(bool checked)
{
    ledgerItemsVisible = checked;

    ui->ledgerWidget->setVisible(ledgerItemsVisible);
    ui->ledgerPathLabel->setVisible(ledgerItemsVisible);
    ui->ledgerInfoLabel->setVisible(ledgerItemsVisible);

    // reset dialog height after hiding ledger items
    setFixedHeight(sizeHint().height());
}

QString EditAddressDialog::getAddress() const
{
    return address;
}

void EditAddressDialog::setAddressEditValue(const QString &addressIn)
{
    this->address = addressIn;
    ui->addressEdit->setText(addressIn);
}

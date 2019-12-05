#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"

#include "transactiontablemodel.h"

#include <QMessageBox>
#include <QModelIndex>

#include "init.h"
#include "ntp1/ntp1script.h"
#include "qt/json/JsonTreeNode.h"

void TransactionDescDialog::setMetadata(const QString& metadataStr, const QString& txid)
{
    json_spirit::Value res;
    std::string        str = metadataStr.toStdString();
    ui->metadataTextView->setPlainText(metadataStr);
    try {
        json_spirit::read_or_throw(str, res);
        json_spirit::Value ser = json_spirit::find_value(res.get_obj(), METADATA_SER_FIELD__VERSION);
        if (!(ser == json_spirit::Value::null)) {
            // data is encrypted
            boost::optional<std::string> decError;
            std::string decrypted = CTransaction::DecryptMetadataOfTx(metadataStr.toStdString(),
                                                                      txid.toStdString(), decError);
            if (decError) {
                QMessageBox::warning(this, "Message decryption failed",
                                     QString::fromStdString(*decError));
                return;
            }

            ui->metadataTextView->setPlainText(QString::fromStdString(decrypted));
            ui->metadataTreeView->setVisible(true);
            ui->metadataTextView->setVisible(false);
            ui->switchJsonTreeTextButton->setVisible(true);
            ui->metadataTreeView->setModel(&jsonTreeModel);
            json_spirit::read_or_throw(decrypted, res);
            jsonTreeModel.setRoot(JsonTreeNode::ImportFromJson(res));
        } else {
            ui->metadataTreeView->setVisible(true);
            ui->metadataTextView->setVisible(false);
            ui->switchJsonTreeTextButton->setVisible(true);
            ui->metadataTreeView->setModel(&jsonTreeModel);
            jsonTreeModel.setRoot(JsonTreeNode::ImportFromJson(res));
        }
    } catch (std::exception& ex) {
        printf("Error while decoding transaction: %s", ex.what());
        ui->metadataTreeView->setVisible(false);
        ui->switchJsonTreeTextButton->setVisible(false);
        // just display metadata as is
        if (!metadataStr.isEmpty()) {
            ui->metadataTextView->setVisible(true);
        }
        return;
    }
}

TransactionDescDialog::TransactionDescDialog(const QModelIndex& idx, QWidget* parent)
    : QDialog(parent), ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    QString desc     = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    QString metadata = idx.data(TransactionTableModel::NTP1MetadataRole).toString();
    setMetadata(metadata, idx.data(TransactionTableModel::TxIDRole).toString());
    ui->detailText->setHtml(desc);
    connect(ui->switchJsonTreeTextButton, &QPushButton::clicked, this,
            &TransactionDescDialog::slot_switchJsonTreeToText);
}

TransactionDescDialog::~TransactionDescDialog() { delete ui; }

void TransactionDescDialog::slot_switchJsonTreeToText()
{
    bool treeVisible = ui->metadataTreeView->isVisible();
    ui->metadataTreeView->setVisible(!treeVisible);
    ui->metadataTextView->setVisible(treeVisible);
}

#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"

#include "transactiontablemodel.h"

#include <QMessageBox>
#include <QModelIndex>

#include "init.h"
#include "ntp1/ntp1script.h"
#include "qt/json/JsonTreeNode.h"

void TransactionDescDialog::setMetadata(const QString& metadataStr)
{
    json_spirit::Value res;
    std::string        str = metadataStr.toStdString();
    ui->metadataTextView->setPlainText(metadataStr);
    try {
        json_spirit::read_or_throw(str, res);
    } catch (...) {
        ui->metadataTreeView->setVisible(false);
        if (!metadataStr.isEmpty()) {
            ui->metadataTextView->setVisible(true);
        }
        ui->switchJsonTreeTextButton->setVisible(false);
        return;
    }
    ui->metadataTreeView->setVisible(true);
    ui->metadataTextView->setVisible(false);
    ui->switchJsonTreeTextButton->setVisible(true);
    ui->metadataTreeView->setModel(&jsonTreeModel);
    jsonTreeModel.setRoot(JsonTreeNode::ImportFromJson(res));
}

TransactionDescDialog::TransactionDescDialog(const QModelIndex& idx, QWidget* parent)
    : QDialog(parent), ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    QString desc     = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    QString metadata = idx.data(TransactionTableModel::NTP1MetadataRole).toString();
    setMetadata(metadata);
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

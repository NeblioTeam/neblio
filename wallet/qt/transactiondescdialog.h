#ifndef TRANSACTIONDESCDIALOG_H
#define TRANSACTIONDESCDIALOG_H

#include "qt/json/JsonTreeModel.h"
#include <QDialog>

namespace Ui {
class TransactionDescDialog;
}
QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing transaction details. */
class TransactionDescDialog : public QDialog
{
    Q_OBJECT

    void setMetadata(const QString& metadataStr);

public:
    explicit TransactionDescDialog(const QModelIndex& idx, QWidget* parent = 0);
    ~TransactionDescDialog();

private:
    Ui::TransactionDescDialog* ui;
    JsonTreeModel              jsonTreeModel;

private slots:
    void slot_switchJsonTreeToText();
};

#endif // TRANSACTIONDESCDIALOG_H

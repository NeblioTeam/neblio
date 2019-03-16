/********************************************************************************
** Form generated from reading UI file 'transactiondescdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.7.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TRANSACTIONDESCDIALOG_H
#define UI_TRANSACTIONDESCDIALOG_H

#include <QPlainTextEdit>
#include <QPushButton>
#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTextEdit>
#include <json/JsonTreeView.h>

QT_BEGIN_NAMESPACE

class Ui_TransactionDescDialog
{
public:
    QGridLayout*      mainLayout;
    QTextEdit*        detailText;
    QDialogButtonBox* buttonBox;
    QPushButton*      switchJsonTreeTextButton;
    JsonTreeView*     metadataTreeView;
    QPlainTextEdit*   metadataTextView;

    void setupUi(QDialog* TransactionDescDialog)
    {
        if (TransactionDescDialog->objectName().isEmpty())
            TransactionDescDialog->setObjectName(QStringLiteral("TransactionDescDialog"));
        TransactionDescDialog->resize(620, 500);
        TransactionDescDialog->setStyleSheet(QStringLiteral(""));
        mainLayout = new QGridLayout(TransactionDescDialog);
        mainLayout->setObjectName(QStringLiteral("verticalLayout"));
        detailText = new QTextEdit(TransactionDescDialog);
        detailText->setObjectName(QStringLiteral("detailText"));
        detailText->setReadOnly(true);

        mainLayout->addWidget(detailText, 0, 0, 1, 2);

        metadataTreeView = new JsonTreeView(TransactionDescDialog);
        mainLayout->addWidget(metadataTreeView, 1, 0, 1, 2);
        metadataTextView = new QPlainTextEdit(TransactionDescDialog);
        metadataTextView->setVisible(false);
        metadataTextView->setReadOnly(true);
        mainLayout->addWidget(metadataTextView, 1, 0, 1, 2);

        buttonBox = new QDialogButtonBox(TransactionDescDialog);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Close);

        switchJsonTreeTextButton = new QPushButton("Switch tree/text view", TransactionDescDialog);

        mainLayout->addWidget(switchJsonTreeTextButton, 2, 0, 1, 1);
        mainLayout->addWidget(buttonBox, 2, 1, 1, 1);

        retranslateUi(TransactionDescDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), TransactionDescDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), TransactionDescDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(TransactionDescDialog);
    } // setupUi

    void retranslateUi(QDialog* TransactionDescDialog)
    {
        TransactionDescDialog->setWindowTitle(
            QApplication::translate("TransactionDescDialog", "Transaction details", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        detailText->setToolTip(QApplication::translate(
            "TransactionDescDialog", "This pane shows a detailed description of the transaction",
            Q_NULLPTR));
#endif // QT_NO_TOOLTIP
    }  // retranslateUi
};

namespace Ui {
class TransactionDescDialog : public Ui_TransactionDescDialog
{
};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TRANSACTIONDESCDIALOG_H

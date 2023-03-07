/********************************************************************************
** Form generated from reading UI file 'editaddressdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.7.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_EDITADDRESSDIALOG_H
#define UI_EDITADDRESSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtGui/QIntValidator>
#include "ledger/utils.h"

QT_BEGIN_NAMESPACE

class Ui_EditAddressDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *labelEdit;
    QLabel *label_2;
    QLineEdit *addressEdit;
    QDialogButtonBox *buttonBox;

    QCheckBox *ledgerCheckBox;
    QWidget *ledgerWidget;
    QFormLayout *ledgerFormLayout;
    QLabel *ledgerAccountLabel;
    QLineEdit *ledgerAccountEdit;
    QLabel *ledgerIndexLabel;
    QLineEdit *ledgerIndexEdit;
    QLabel *ledgerInfoLabel;

    void setupUi(QDialog *EditAddressDialog)
    {
        if (EditAddressDialog->objectName().isEmpty())
            EditAddressDialog->setObjectName(QStringLiteral("EditAddressDialog"));
        EditAddressDialog->resize(457, 126);
        EditAddressDialog->setStyleSheet(QStringLiteral("background-color: white;"));
        verticalLayout = new QVBoxLayout(EditAddressDialog);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        formLayout = new QFormLayout();
        formLayout->setObjectName(QStringLiteral("formLayout"));
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        label = new QLabel(EditAddressDialog);
        label->setObjectName(QStringLiteral("label"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        labelEdit = new QLineEdit(EditAddressDialog);
        labelEdit->setObjectName(QStringLiteral("labelEdit"));

        formLayout->setWidget(0, QFormLayout::FieldRole, labelEdit);

        label_2 = new QLabel(EditAddressDialog);
        label_2->setObjectName(QStringLiteral("label_2"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label_2);

        addressEdit = new QLineEdit(EditAddressDialog);
        addressEdit->setObjectName(QStringLiteral("addressEdit"));

        formLayout->setWidget(1, QFormLayout::FieldRole, addressEdit);


        verticalLayout->addLayout(formLayout);


        ledgerCheckBox = new QCheckBox(EditAddressDialog);
        ledgerCheckBox->setObjectName(QStringLiteral("ledgerCheckBox"));
        verticalLayout->addWidget(ledgerCheckBox);

        ledgerWidget = new QWidget(EditAddressDialog);
        ledgerWidget->setObjectName(QStringLiteral("ledgerWidget"));

        ledgerFormLayout = new QFormLayout();
        ledgerFormLayout->setObjectName(QStringLiteral("ledgerFormLayout"));
        ledgerFormLayout->setContentsMargins(0, 0, 0, 0);
        ledgerWidget->setLayout(ledgerFormLayout);

        ledgerAccountLabel = new QLabel(EditAddressDialog);
        ledgerAccountLabel->setObjectName(QStringLiteral("ledgerAccountLabel"));
        ledgerFormLayout->setWidget(0, QFormLayout::LabelRole, ledgerAccountLabel);

        ledgerAccountEdit = new QLineEdit(EditAddressDialog);
        ledgerAccountEdit->setObjectName(QStringLiteral("ledgerAccountEdit"));
        ledgerAccountEdit->setValidator(new QIntValidator(0, ledger::utils::MAX_RECOMMENDED_ACCOUNT));
        ledgerAccountEdit->setText("0");
        ledgerFormLayout->setWidget(0, QFormLayout::FieldRole, ledgerAccountEdit);

        ledgerIndexLabel = new QLabel(EditAddressDialog);
        ledgerIndexLabel->setObjectName(QStringLiteral("ledgerIndexLabel"));
        ledgerFormLayout->setWidget(1, QFormLayout::LabelRole, ledgerIndexLabel);

        ledgerIndexEdit = new QLineEdit(EditAddressDialog);
        ledgerIndexEdit->setObjectName(QStringLiteral("ledgerIndexEdit"));
        ledgerIndexEdit->setValidator(new QIntValidator(0, ledger::utils::MAX_RECOMMENDED_INDEX));
        ledgerIndexEdit->setText("0");
        ledgerFormLayout->setWidget(1, QFormLayout::FieldRole, ledgerIndexEdit);

        verticalLayout->addWidget(ledgerWidget);

        ledgerInfoLabel = new QLabel(EditAddressDialog);
        ledgerInfoLabel->setObjectName(QStringLiteral("ledgerInfoLabel"));
        ledgerInfoLabel->setWordWrap(true);
        verticalLayout->addWidget(ledgerInfoLabel);


        buttonBox = new QDialogButtonBox(EditAddressDialog);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);

#ifndef QT_NO_SHORTCUT
        label->setBuddy(labelEdit);
        label_2->setBuddy(addressEdit);
        ledgerAccountLabel->setBuddy(ledgerAccountEdit);
        ledgerIndexLabel->setBuddy(ledgerIndexEdit);
#endif // QT_NO_SHORTCUT

        retranslateUi(EditAddressDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), EditAddressDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), EditAddressDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(EditAddressDialog);
    } // setupUi

    void retranslateUi(QDialog *EditAddressDialog)
    {
        EditAddressDialog->setWindowTitle(QApplication::translate("EditAddressDialog", "Edit Address", Q_NULLPTR));
        label->setText(QApplication::translate("EditAddressDialog", "&Label", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        labelEdit->setToolTip(QApplication::translate("EditAddressDialog", "The label associated with this address book entry", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        label_2->setText(QApplication::translate("EditAddressDialog", "&Address", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        addressEdit->setToolTip(QApplication::translate("EditAddressDialog", "The address associated with this address book entry. This can only be modified for sending addresses.", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        ledgerCheckBox->setText(QApplication::translate("EditAddressDialog", "L&edger address", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        ledgerCheckBox->setToolTip(QApplication::translate("EditAddressDialog", "Should the address be controlled by a Ledger hardware wallet", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        ledgerAccountLabel->setText(QApplication::translate("EditAddressDialog", "Ledger acc&ount number", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        ledgerAccountEdit->setToolTip(QApplication::translate("EditAddressDialog", "The account number in the Ledger address derivation path", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        ledgerIndexLabel->setText(QApplication::translate("EditAddressDialog", "Ledger a&ddress index", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        ledgerIndexEdit->setToolTip(QApplication::translate("EditAddressDialog", "The address index in the Ledger address derivation path", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        ledgerInfoLabel->setText(QApplication::translate("EditAddressDialog", "Make sure to connect your Ledger device and open the neblio app before continuing.", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class EditAddressDialog: public Ui_EditAddressDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EDITADDRESSDIALOG_H

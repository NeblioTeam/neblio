/********************************************************************************
** Form generated from reading UI file 'optionsdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.7.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OPTIONSDIALOG_H
#define UI_OPTIONSDIALOG_H

#include "bitcoinamountfield.h"
#include "messageboxwithtimer.h"
#include "qvalidatedlineedit.h"
#include "qvaluecombobox.h"
#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_OptionsDialog
{
public:
    QVBoxLayout*        verticalLayout;
    QTabWidget*         tabWidget;
    QWidget*            tabMain;
    QVBoxLayout*        verticalLayout_Main;
    QLabel*             transactionFeeInfoLabel;
    QHBoxLayout*        horizontalLayoutFee;
    QLabel*             transactionFeeLabel;
    BitcoinAmountField* transactionFee;
    QSpacerItem*        horizontalSpacerFee;
    QLabel*             reserveBalanceInfoLabel;
    QHBoxLayout*        horizontalLayoutReserveBalance;
    QLabel*             reserveBalanceLabel;
    BitcoinAmountField* reserveBalance;
    QSpacerItem*        horizontalSpacerReserveBalance;
    QCheckBox*          bitcoinAtStartup;
    QSpacerItem*        verticalSpacer_Main;
    QWidget*            tabNetwork;
    QVBoxLayout*        verticalLayout_Network;
    QCheckBox*          mapPortUpnp;
    QCheckBox*          connectSocks;
    QHBoxLayout*        horizontalLayout_Network;
    QLabel*             proxyIpLabel;
    QValidatedLineEdit* proxyIp;
    QLabel*             proxyPortLabel;
    QLineEdit*          proxyPort;
    QLabel*             socksVersionLabel;
    QValueComboBox*     socksVersion;
    QSpacerItem*        horizontalSpacer_Network;
    QSpacerItem*        verticalSpacer_Network;
    QWidget*            tabWindow;
    QVBoxLayout*        verticalLayout_Window;
    QCheckBox*          minimizeToTray;
    QCheckBox*          minimizeOnClose;
    QSpacerItem*        verticalSpacer_Window;
    QWidget*            tabDisplay;
    QVBoxLayout*        verticalLayout_Display;
    QHBoxLayout*        horizontalLayout_1_Display;
    QLabel*             langLabel;
    QValueComboBox*     lang;
    QHBoxLayout*        horizontalLayout_2_Display;
    QLabel*             unitLabel;
    QValueComboBox*     unit;
    QCheckBox*          displayAddresses;
    QCheckBox*          coinControlFeatures;
    QSpacerItem*        verticalSpacer_Display;
    QHBoxLayout*        horizontalLayout_Buttons;
    QSpacerItem*        horizontalSpacer_1;
    QLabel*             statusLabel;
    QSpacerItem*        horizontalSpacer_2;
    QPushButton*        okButton;
    QPushButton*        cancelButton;
    QPushButton*        applyButton;
    QWidget*            tabNTPOptions;
    QGridLayout*        tabNTPOptionsLayout;
    QPushButton*        clearNTP1DataCacheButton;
    QPushButton*        scheduleWalletRescanButton;
    QPushButton*        scheduleWalletResyncButton;

    void setupUi(QDialog* OptionsDialog)
    {
        if (OptionsDialog->objectName().isEmpty())
            OptionsDialog->setObjectName(QStringLiteral("OptionsDialog"));
        OptionsDialog->resize(540, 380);
        OptionsDialog->setModal(true);
        verticalLayout = new QVBoxLayout(OptionsDialog);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        tabWidget = new QTabWidget(OptionsDialog);
        tabWidget->setObjectName(QStringLiteral("tabWidget"));
        tabWidget->setTabPosition(QTabWidget::North);
        tabMain = new QWidget();
        tabMain->setObjectName(QStringLiteral("tabMain"));
        verticalLayout_Main = new QVBoxLayout(tabMain);
        verticalLayout_Main->setObjectName(QStringLiteral("verticalLayout_Main"));
        transactionFeeInfoLabel = new QLabel(tabMain);
        transactionFeeInfoLabel->setObjectName(QStringLiteral("transactionFeeInfoLabel"));
        transactionFeeInfoLabel->setTextFormat(Qt::PlainText);
        transactionFeeInfoLabel->setWordWrap(true);

        verticalLayout_Main->addWidget(transactionFeeInfoLabel);

        horizontalLayoutFee = new QHBoxLayout();
        horizontalLayoutFee->setObjectName(QStringLiteral("horizontalLayoutFee"));
        transactionFeeLabel = new QLabel(tabMain);
        transactionFeeLabel->setObjectName(QStringLiteral("transactionFeeLabel"));
        transactionFeeLabel->setTextFormat(Qt::PlainText);

        horizontalLayoutFee->addWidget(transactionFeeLabel);

        transactionFee = new BitcoinAmountField(false, tabMain);
        transactionFee->setObjectName(QStringLiteral("transactionFee"));

        horizontalLayoutFee->addWidget(transactionFee);

        horizontalSpacerFee = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayoutFee->addItem(horizontalSpacerFee);

        verticalLayout_Main->addLayout(horizontalLayoutFee);

        reserveBalanceInfoLabel = new QLabel(tabMain);
        reserveBalanceInfoLabel->setObjectName(QStringLiteral("reserveBalanceInfoLabel"));
        reserveBalanceInfoLabel->setTextFormat(Qt::PlainText);
        reserveBalanceInfoLabel->setWordWrap(true);

        verticalLayout_Main->addWidget(reserveBalanceInfoLabel);

        horizontalLayoutReserveBalance = new QHBoxLayout();
        horizontalLayoutReserveBalance->setObjectName(QStringLiteral("horizontalLayoutReserveBalance"));
        reserveBalanceLabel = new QLabel(tabMain);
        reserveBalanceLabel->setObjectName(QStringLiteral("reserveBalanceLabel"));
        reserveBalanceLabel->setTextFormat(Qt::PlainText);

        horizontalLayoutReserveBalance->addWidget(reserveBalanceLabel);

        reserveBalance = new BitcoinAmountField(false, tabMain);
        reserveBalance->setObjectName(QStringLiteral("reserveBalance"));

        horizontalLayoutReserveBalance->addWidget(reserveBalance);

        horizontalSpacerReserveBalance =
            new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayoutReserveBalance->addItem(horizontalSpacerReserveBalance);

        verticalLayout_Main->addLayout(horizontalLayoutReserveBalance);

        bitcoinAtStartup = new QCheckBox(tabMain);
        bitcoinAtStartup->setObjectName(QStringLiteral("bitcoinAtStartup"));

        verticalLayout_Main->addWidget(bitcoinAtStartup);

        verticalSpacer_Main = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        scheduleWalletRescanButton = new QPushButton("Schedule wallet rescan", tabMain);
        verticalLayout_Main->addWidget(scheduleWalletRescanButton);
        scheduleWalletRescanButton->clearFocus();

        scheduleWalletResyncButton = new QPushButton("Schedule blockchain resync", tabMain);
        verticalLayout_Main->addWidget(scheduleWalletResyncButton);
        scheduleWalletResyncButton->clearFocus();

        verticalLayout_Main->addItem(verticalSpacer_Main);

        tabWidget->addTab(tabMain, QString());
        /// End of tab main
        tabNetwork = new QWidget();
        tabNetwork->setObjectName(QStringLiteral("tabNetwork"));
        verticalLayout_Network = new QVBoxLayout(tabNetwork);
        verticalLayout_Network->setObjectName(QStringLiteral("verticalLayout_Network"));
        mapPortUpnp = new QCheckBox(tabNetwork);
        mapPortUpnp->setObjectName(QStringLiteral("mapPortUpnp"));

        verticalLayout_Network->addWidget(mapPortUpnp);

        connectSocks = new QCheckBox(tabNetwork);
        connectSocks->setObjectName(QStringLiteral("connectSocks"));

        verticalLayout_Network->addWidget(connectSocks);

        horizontalLayout_Network = new QHBoxLayout();
        horizontalLayout_Network->setObjectName(QStringLiteral("horizontalLayout_Network"));
        proxyIpLabel = new QLabel(tabNetwork);
        proxyIpLabel->setObjectName(QStringLiteral("proxyIpLabel"));
        proxyIpLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_Network->addWidget(proxyIpLabel);

        proxyIp = new QValidatedLineEdit(tabNetwork);
        proxyIp->setObjectName(QStringLiteral("proxyIp"));
        proxyIp->setMaximumSize(QSize(140, 16777215));

        horizontalLayout_Network->addWidget(proxyIp);

        proxyPortLabel = new QLabel(tabNetwork);
        proxyPortLabel->setObjectName(QStringLiteral("proxyPortLabel"));
        proxyPortLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_Network->addWidget(proxyPortLabel);

        proxyPort = new QLineEdit(tabNetwork);
        proxyPort->setObjectName(QStringLiteral("proxyPort"));
        proxyPort->setMaximumSize(QSize(55, 16777215));

        horizontalLayout_Network->addWidget(proxyPort);

        socksVersionLabel = new QLabel(tabNetwork);
        socksVersionLabel->setObjectName(QStringLiteral("socksVersionLabel"));
        socksVersionLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_Network->addWidget(socksVersionLabel);

        socksVersion = new QValueComboBox(tabNetwork);
        socksVersion->setObjectName(QStringLiteral("socksVersion"));

        horizontalLayout_Network->addWidget(socksVersion);

        horizontalSpacer_Network = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Network->addItem(horizontalSpacer_Network);

        verticalLayout_Network->addLayout(horizontalLayout_Network);

        verticalSpacer_Network = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_Network->addItem(verticalSpacer_Network);

        tabWidget->addTab(tabNetwork, QString());
        tabWindow = new QWidget();
        tabWindow->setObjectName(QStringLiteral("tabWindow"));
        verticalLayout_Window = new QVBoxLayout(tabWindow);
        verticalLayout_Window->setObjectName(QStringLiteral("verticalLayout_Window"));
        minimizeToTray = new QCheckBox(tabWindow);
        minimizeToTray->setObjectName(QStringLiteral("minimizeToTray"));

        verticalLayout_Window->addWidget(minimizeToTray);

        minimizeOnClose = new QCheckBox(tabWindow);
        minimizeOnClose->setObjectName(QStringLiteral("minimizeOnClose"));

        verticalLayout_Window->addWidget(minimizeOnClose);

        verticalSpacer_Window = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_Window->addItem(verticalSpacer_Window);

        tabWidget->addTab(tabWindow, QString());
        tabDisplay = new QWidget();
        tabDisplay->setObjectName(QStringLiteral("tabDisplay"));
        verticalLayout_Display = new QVBoxLayout(tabDisplay);
        verticalLayout_Display->setObjectName(QStringLiteral("verticalLayout_Display"));
        horizontalLayout_1_Display = new QHBoxLayout();
        horizontalLayout_1_Display->setObjectName(QStringLiteral("horizontalLayout_1_Display"));
        langLabel = new QLabel(tabDisplay);
        langLabel->setObjectName(QStringLiteral("langLabel"));
        langLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_1_Display->addWidget(langLabel);

        lang = new QValueComboBox(tabDisplay);
        lang->setObjectName(QStringLiteral("lang"));

        horizontalLayout_1_Display->addWidget(lang);

        verticalLayout_Display->addLayout(horizontalLayout_1_Display);

        horizontalLayout_2_Display = new QHBoxLayout();
        horizontalLayout_2_Display->setObjectName(QStringLiteral("horizontalLayout_2_Display"));
        unitLabel = new QLabel(tabDisplay);
        unitLabel->setObjectName(QStringLiteral("unitLabel"));
        unitLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_2_Display->addWidget(unitLabel);

        unit = new QValueComboBox(tabDisplay);
        unit->setObjectName(QStringLiteral("unit"));

        horizontalLayout_2_Display->addWidget(unit);

        verticalLayout_Display->addLayout(horizontalLayout_2_Display);

        displayAddresses = new QCheckBox(tabDisplay);
        displayAddresses->setObjectName(QStringLiteral("displayAddresses"));

        verticalLayout_Display->addWidget(displayAddresses);

        coinControlFeatures = new QCheckBox(tabDisplay);
        coinControlFeatures->setObjectName(QStringLiteral("coinControlFeatures"));

        verticalLayout_Display->addWidget(coinControlFeatures);

        verticalSpacer_Display = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_Display->addItem(verticalSpacer_Display);

        tabWidget->addTab(tabDisplay, QString());

        verticalLayout->addWidget(tabWidget);

        horizontalLayout_Buttons = new QHBoxLayout();
        horizontalLayout_Buttons->setObjectName(QStringLiteral("horizontalLayout_Buttons"));
        horizontalSpacer_1 = new QSpacerItem(40, 48, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Buttons->addItem(horizontalSpacer_1);

        statusLabel = new QLabel(OptionsDialog);
        statusLabel->setObjectName(QStringLiteral("statusLabel"));
        QFont font;
        font.setBold(true);
        font.setWeight(75);
        statusLabel->setFont(font);
        statusLabel->setTextFormat(Qt::PlainText);
        statusLabel->setWordWrap(true);

        /////// NTP1 options tab
        tabNTPOptions = new QWidget();
        tabNTPOptions->setObjectName(QStringLiteral("tabNTPOptions"));
        tabWidget->addTab(tabNTPOptions, QString("N&TP1 Options"));
        tabNTPOptionsLayout = new QGridLayout;
        tabNTPOptions->setLayout(tabNTPOptionsLayout);
        clearNTP1DataCacheButton = new QPushButton(tabNTPOptions);
        clearNTP1DataCacheButton->setText("Clear NTP1 data cache");
        clearNTP1DataCacheButton->setToolTip(
            "The NTP1 wallet data is cached in your data directory to avoid exhausting resources with "
            "rescans. Clicking this button triggers the deletion of the cache and restarting the scan. "
            "Use this if you see wrong information about your NTP1 tokens. DO NOT click this just "
            "because you are waiting for a transaction, this does not accelerate retrieving "
            "transactions.");
        tabNTPOptionsLayout->addWidget(clearNTP1DataCacheButton);
        tabNTPOptionsLayout->setAlignment(Qt::AlignTop);
        ///////

        horizontalLayout_Buttons->addWidget(statusLabel);

        horizontalSpacer_2 = new QSpacerItem(40, 48, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Buttons->addItem(horizontalSpacer_2);

        okButton = new QPushButton(OptionsDialog);
        okButton->setObjectName(QStringLiteral("okButton"));

        horizontalLayout_Buttons->addWidget(okButton);

        cancelButton = new QPushButton(OptionsDialog);
        cancelButton->setObjectName(QStringLiteral("cancelButton"));
        cancelButton->setAutoDefault(false);

        horizontalLayout_Buttons->addWidget(cancelButton);

        applyButton = new QPushButton(OptionsDialog);
        applyButton->setObjectName(QStringLiteral("applyButton"));
        applyButton->setAutoDefault(false);

        horizontalLayout_Buttons->addWidget(applyButton);

        verticalLayout->addLayout(horizontalLayout_Buttons);

#ifndef QT_NO_SHORTCUT
        transactionFeeLabel->setBuddy(transactionFee);
        reserveBalanceLabel->setBuddy(reserveBalance);
        proxyIpLabel->setBuddy(proxyIp);
        proxyPortLabel->setBuddy(proxyPort);
        socksVersionLabel->setBuddy(socksVersion);
        langLabel->setBuddy(lang);
        unitLabel->setBuddy(unit);
#endif // QT_NO_SHORTCUT

        retranslateUi(OptionsDialog);

        tabWidget->setCurrentIndex(0);

        QMetaObject::connectSlotsByName(OptionsDialog);
    } // setupUi

    void retranslateUi(QDialog* OptionsDialog)
    {
        OptionsDialog->setWindowTitle(QApplication::translate("OptionsDialog", "Options", Q_NULLPTR));
        transactionFeeInfoLabel->setText(QApplication::translate(
            "OptionsDialog",
            "Optional transaction fee per kB that helps make sure your transactions are processed "
            "quickly. Most transactions are 1 kB. Fee 0.01 recommended.",
            Q_NULLPTR));
        transactionFeeLabel->setText(
            QApplication::translate("OptionsDialog", "Pay transaction &fee", Q_NULLPTR));
        reserveBalanceInfoLabel->setText(QApplication::translate(
            "OptionsDialog",
            "Reserved amount does not participate in staking and is therefore spendable at any time.",
            Q_NULLPTR));
        reserveBalanceLabel->setText(QApplication::translate("OptionsDialog", "Reserve", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        bitcoinAtStartup->setToolTip(QApplication::translate(
            "OptionsDialog", "Automatically start neblio after logging in to the system.", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        bitcoinAtStartup->setText(
            QApplication::translate("OptionsDialog", "&Start neblio on system login", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tabMain),
                              QApplication::translate("OptionsDialog", "&Main", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        mapPortUpnp->setToolTip(QApplication::translate("OptionsDialog",
                                                        "Automatically open the neblio client port on "
                                                        "the router. This only works when your router "
                                                        "supports UPnP and it is enabled.",
                                                        Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        mapPortUpnp->setText(
            QApplication::translate("OptionsDialog", "Map port using &UPnP", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        connectSocks->setToolTip(QApplication::translate(
            "OptionsDialog",
            "Connect to the neblio network through a SOCKS proxy (e.g. when connecting through Tor).",
            Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        connectSocks->setText(
            QApplication::translate("OptionsDialog", "&Connect through SOCKS proxy:", Q_NULLPTR));
        proxyIpLabel->setText(QApplication::translate("OptionsDialog", "Proxy &IP:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        proxyIp->setToolTip(QApplication::translate(
            "OptionsDialog", "IP address of the proxy (e.g. 127.0.0.1)", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        proxyPortLabel->setText(QApplication::translate("OptionsDialog", "&Port:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        proxyPort->setToolTip(
            QApplication::translate("OptionsDialog", "Port of the proxy (e.g. 9050)", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        socksVersionLabel->setText(
            QApplication::translate("OptionsDialog", "SOCKS &Version:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        socksVersion->setToolTip(
            QApplication::translate("OptionsDialog", "SOCKS version of the proxy (e.g. 5)", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        tabWidget->setTabText(tabWidget->indexOf(tabNetwork),
                              QApplication::translate("OptionsDialog", "&Network", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        minimizeToTray->setToolTip(QApplication::translate(
            "OptionsDialog", "Show only a tray icon after minimizing the window.", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        minimizeToTray->setText(QApplication::translate(
            "OptionsDialog", "&Minimize to the tray instead of the taskbar", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        minimizeOnClose->setToolTip(QApplication::translate(
            "OptionsDialog",
            "Minimize instead of exit the application when the window is closed. When this option is "
            "enabled, the application will be closed only after selecting Quit in the menu.",
            Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        minimizeOnClose->setText(
            QApplication::translate("OptionsDialog", "M&inimize on close", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tabWindow),
                              QApplication::translate("OptionsDialog", "&Window", Q_NULLPTR));
        langLabel->setText(
            QApplication::translate("OptionsDialog", "User Interface &language:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        lang->setToolTip(QApplication::translate("OptionsDialog",
                                                 "The user interface language can be set here. This "
                                                 "setting will take effect after restarting neblio.",
                                                 Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        unitLabel->setText(
            QApplication::translate("OptionsDialog", "&Unit to show amounts in:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        unit->setToolTip(QApplication::translate(
            "OptionsDialog",
            "Choose the default subdivision unit to show in the interface and when sending tokens.",
            Q_NULLPTR));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        displayAddresses->setToolTip(QApplication::translate(
            "OptionsDialog", "Whether to show neblio addresses in the transaction list or not.",
            Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        displayAddresses->setText(QApplication::translate(
            "OptionsDialog", "&Display addresses in transaction list", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        coinControlFeatures->setToolTip(QApplication::translate(
            "OptionsDialog", "Whether to show coin control features or not.", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        coinControlFeatures->setText(QApplication::translate(
            "OptionsDialog", "Display coin &control features (experts only!)", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tabDisplay),
                              QApplication::translate("OptionsDialog", "&Display", Q_NULLPTR));
        statusLabel->setText(QString());
        okButton->setText(QApplication::translate("OptionsDialog", "&OK", Q_NULLPTR));
        cancelButton->setText(QApplication::translate("OptionsDialog", "&Cancel", Q_NULLPTR));
        applyButton->setText(QApplication::translate("OptionsDialog", "&Apply", Q_NULLPTR));
    } // retranslateUi
};

namespace Ui {
class OptionsDialog : public Ui_OptionsDialog
{
};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OPTIONSDIALOG_H

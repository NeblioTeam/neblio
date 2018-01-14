/********************************************************************************
** Form generated from reading UI file 'overviewpage.ui'
**
** Created by: Qt User Interface Compiler version 5.7.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OVERVIEWPAGE_H
#define UI_OVERVIEWPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_OverviewPage
{
public:
    QPixmap left_logo_pix;
    QGridLayout *main_layout;
    QVBoxLayout *left_logo_layout;
    QLabel *left_logo_label;
    QVBoxLayout *logo_layout;
    QVBoxLayout *right_balance_layout;
    QFrame *wallet_contents_frame;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_2;
    QLabel *recent_tx_label;
    QLabel *labelTransactionsStatus;
    QListView *listTransactions;
    QLabel *labelWalletStatus;
    QLabel *wallet_label;
    QGridLayout *balance_layout;
    QLabel *spendable_title_label;
    QLabel *spendable_value_label;
    QLabel *stake_title_label;
    QLabel *stake_value_label;
    QLabel *unconfirmed_title_label;
    QLabel *unconfirmed_value_label;
    QLabel *immature_title_label;
    QLabel *immature_value_label;
    QLabel *labelTotalText;
    QLabel *total_value_label;

    QWidget *bottom_bar_widget;
    QLabel *bottom_bar_label;
    QGridLayout *bottom_layout;
    QPixmap bottom_logo_pix;

    void setupUi(QWidget *OverviewPage)
    {
        if (OverviewPage->objectName().isEmpty())
            OverviewPage->setObjectName(QStringLiteral("OverviewPage"));
        OverviewPage->resize(761, 452);
        main_layout = new QGridLayout(OverviewPage);
        main_layout->setObjectName(QStringLiteral("horizontalLayout"));
        left_logo_layout = new QVBoxLayout();
        left_logo_layout->setObjectName(QStringLiteral("verticalLayout_2"));
        left_logo_label = new QLabel(OverviewPage);
        left_logo_label->setObjectName(QStringLiteral("frame"));

//        logo_label->setFrameShadow(QFrame::Raised);
        left_logo_label->setLineWidth(0);
//        logo_label->setFrameStyle(QFrame::StyledPanel);

        left_logo_pix = QPixmap(":images/neblio_vertical");
        left_logo_pix = left_logo_pix.scaledToHeight(OverviewPage->height()*3./4., Qt::SmoothTransformation);
        left_logo_label->setPixmap(left_logo_pix);
        left_logo_label->setAlignment(Qt::AlignCenter);

        logo_layout = new QVBoxLayout(left_logo_label);
        logo_layout->setObjectName(QStringLiteral("verticalLayout_4"));

        left_logo_layout->addWidget(left_logo_label);

        main_layout->addLayout(left_logo_layout, 0, 0, 1, 1);

        bottom_logo_pix = QPixmap(":images/neblio_horizontal");
        bottom_bar_widget = new QWidget(OverviewPage);
        bottom_layout = new QGridLayout(bottom_bar_widget);
        bottom_bar_label = new QLabel(bottom_bar_widget);


        main_layout->addWidget(bottom_bar_widget, 1, 0, 1, 2);
        bottom_bar_widget->setLayout(bottom_layout);
        bottom_layout->addWidget(bottom_bar_label, 0, 0, 1, 1);
        bottom_bar_widget->setStyleSheet("background-color: #333333;");
        bottom_logo_pix = bottom_logo_pix.scaledToHeight(OverviewPage->height()/8, Qt::SmoothTransformation);
        bottom_bar_label->setPixmap(bottom_logo_pix);
        bottom_bar_label->setAlignment(Qt::AlignRight);


        right_balance_layout = new QVBoxLayout();
        right_balance_layout->setObjectName(QStringLiteral("verticalLayout_3"));
        wallet_contents_frame = new QFrame(OverviewPage);
        wallet_contents_frame->setObjectName(QStringLiteral("frame_2"));
        wallet_contents_frame->setFrameShape(QFrame::StyledPanel);
//        wallet_contents_frame->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(wallet_contents_frame);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        recent_tx_label = new QLabel(wallet_contents_frame);
        recent_tx_label->setObjectName(QStringLiteral("label_4"));

        horizontalLayout_2->addWidget(recent_tx_label);

        labelTransactionsStatus = new QLabel(wallet_contents_frame);
        labelTransactionsStatus->setObjectName(QStringLiteral("labelTransactionsStatus"));
        labelTransactionsStatus->setStyleSheet(QStringLiteral("QLabel { color: red; }"));
        labelTransactionsStatus->setText(QStringLiteral("(out of sync)"));
        labelTransactionsStatus->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_2->addWidget(labelTransactionsStatus);

        verticalLayout->addLayout(horizontalLayout_2);

        listTransactions = new QListView(wallet_contents_frame);
        listTransactions->setObjectName(QStringLiteral("listTransactions"));
        listTransactions->setStyleSheet(QStringLiteral("QListView { background: transparent; }"));
        listTransactions->setFrameShape(QFrame::NoFrame);
        listTransactions->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listTransactions->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listTransactions->setSelectionMode(QAbstractItemView::NoSelection);

        verticalLayout->addWidget(listTransactions);

        labelWalletStatus = new QLabel(wallet_contents_frame);
        labelWalletStatus->setObjectName(QStringLiteral("labelWalletStatus"));
        labelWalletStatus->setStyleSheet(QStringLiteral("QLabel { color: red; }"));
        labelWalletStatus->setText(QStringLiteral("(out of sync)"));
        labelWalletStatus->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        verticalLayout->addWidget(labelWalletStatus);

        wallet_label = new QLabel(wallet_contents_frame);
        wallet_label->setObjectName(QStringLiteral("label_5"));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        wallet_label->setFont(font);

        verticalLayout->addWidget(wallet_label);

        balance_layout = new QGridLayout();
        balance_layout->setObjectName(QStringLiteral("formLayout_2"));
//        balance_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
//        balance_layout->setHorizontalSpacing(12);
//        balance_layout->setVerticalSpacing(12);
        spendable_title_label = new QLabel(wallet_contents_frame);
        spendable_title_label->setObjectName(QStringLiteral("label"));
        QFont font1;
//        font1.setPointSize(10);
        spendable_title_label->setFont(font1);

        balance_layout->addWidget(spendable_title_label, 0, 0);

        spendable_value_label = new QLabel(wallet_contents_frame);
        spendable_value_label->setObjectName(QStringLiteral("labelBalance"));
        QFont font2;
        font2.setPointSize(10);
        font2.setBold(true);
        font2.setWeight(75);
        spendable_value_label->setFont(font2);
        spendable_value_label->setCursor(QCursor(Qt::IBeamCursor));
        spendable_value_label->setText(QStringLiteral("0 NEBL"));
        spendable_value_label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        balance_layout->addWidget(spendable_value_label, 0, 1);

        stake_title_label = new QLabel(wallet_contents_frame);
        stake_title_label->setObjectName(QStringLiteral("label_6"));
        stake_title_label->setFont(font1);

        balance_layout->addWidget(stake_title_label, 1, 0);

        stake_value_label = new QLabel(wallet_contents_frame);
        stake_value_label->setObjectName(QStringLiteral("labelStake"));
        stake_value_label->setFont(font2);
        stake_value_label->setCursor(QCursor(Qt::IBeamCursor));
        stake_value_label->setText(QStringLiteral("0 NEBL"));
        stake_value_label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        balance_layout->addWidget(stake_value_label, 1, 1);

        unconfirmed_title_label = new QLabel(wallet_contents_frame);
        unconfirmed_title_label->setObjectName(QStringLiteral("label_3"));
        unconfirmed_title_label->setFont(font1);

        balance_layout->addWidget(unconfirmed_title_label, 2, 0);

        unconfirmed_value_label = new QLabel(wallet_contents_frame);
        unconfirmed_value_label->setObjectName(QStringLiteral("labelUnconfirmed"));
        unconfirmed_value_label->setFont(font2);
        unconfirmed_value_label->setCursor(QCursor(Qt::IBeamCursor));
        unconfirmed_value_label->setText(QStringLiteral("0 NEBL"));
        unconfirmed_value_label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        balance_layout->addWidget(unconfirmed_value_label, 2, 1);

        immature_title_label = new QLabel(wallet_contents_frame);
        immature_title_label->setObjectName(QStringLiteral("labelImmatureText"));
        immature_title_label->setFont(font1);

        balance_layout->addWidget(immature_title_label, 3, 0);

        immature_value_label = new QLabel(wallet_contents_frame);
        immature_value_label->setObjectName(QStringLiteral("labelImmature"));
        immature_value_label->setFont(font2);
        immature_value_label->setText(QStringLiteral("0 NEBL"));
        immature_value_label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        balance_layout->addWidget(immature_value_label, 3, 1);


        verticalLayout->addLayout(balance_layout);

        labelTotalText = new QLabel(wallet_contents_frame);
        labelTotalText->setObjectName(QStringLiteral("labelTotalText"));
        QFont font3;
        font3.setFamily(QStringLiteral("Arial"));
        font3.setPointSize(16);
        labelTotalText->setFont(font3);

        verticalLayout->addWidget(labelTotalText);

        total_value_label = new QLabel(wallet_contents_frame);
        total_value_label->setObjectName(QStringLiteral("labelTotal"));
        QFont font4;
        font4.setFamily(QStringLiteral("Arial"));
        font4.setPointSize(16);
        font4.setBold(true);
        font4.setWeight(75);
        total_value_label->setFont(font4);
        total_value_label->setCursor(QCursor(Qt::IBeamCursor));
        total_value_label->setText(QStringLiteral("0 BTC"));
        total_value_label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(total_value_label);

        right_balance_layout->addWidget(wallet_contents_frame);

        main_layout->addLayout(right_balance_layout, 0, 1, 1, 1);

        retranslateUi(OverviewPage);

        QMetaObject::connectSlotsByName(OverviewPage);
    } // setupUi

    void retranslateUi(QWidget *OverviewPage)
    {
        OverviewPage->setWindowTitle(QApplication::translate("OverviewPage", "Form", Q_NULLPTR));
        recent_tx_label->setText(QApplication::translate("OverviewPage", "<b>Recent transactions</b>", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        labelTransactionsStatus->setToolTip(QApplication::translate("OverviewPage", "The displayed information may be out of date. Your wallet automatically synchronizes with the neblio network after a connection is established, but this process has not completed yet.", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        labelWalletStatus->setToolTip(QApplication::translate("OverviewPage", "The displayed information may be out of date. Your wallet automatically synchronizes with the neblio network after a connection is established, but this process has not completed yet.", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        wallet_label->setText(QApplication::translate("OverviewPage", "Wallet", Q_NULLPTR));
        spendable_title_label->setText(QApplication::translate("OverviewPage", "Spendable:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        spendable_value_label->setToolTip(QApplication::translate("OverviewPage", "Your current spendable balance", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        stake_title_label->setText(QApplication::translate("OverviewPage", "Stake:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        stake_value_label->setToolTip(QApplication::translate("OverviewPage", "Total number of tokens that were staked, and do not yet count toward the current balance", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        unconfirmed_title_label->setText(QApplication::translate("OverviewPage", "Unconfirmed:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        unconfirmed_value_label->setToolTip(QApplication::translate("OverviewPage", "Total of transactions that have yet to be confirmed, and do not yet count toward the current balance", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        immature_title_label->setText(QApplication::translate("OverviewPage", "Immature:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        immature_value_label->setToolTip(QApplication::translate("OverviewPage", "Mined balance that has not yet matured", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
        labelTotalText->setText(QApplication::translate("OverviewPage", "Total:", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        total_value_label->setToolTip(QApplication::translate("OverviewPage", "Your current total balance", Q_NULLPTR));
#endif // QT_NO_TOOLTIP
    } // retranslateUi

};

namespace Ui {
    class OverviewPage: public Ui_OverviewPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OVERVIEWPAGE_H

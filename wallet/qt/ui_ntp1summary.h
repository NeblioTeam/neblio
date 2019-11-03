#ifndef UI_NTP1SUMMARY_H
#define UI_NTP1SUMMARY_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include "ClickableLabel.h"
#include <QMovie>

#include "ntp1/issuenewntp1tokendialog.h"

QT_BEGIN_NAMESPACE

class TokensListView : public QListView
{
    Q_OBJECT
public:
    explicit TokensListView(QWidget* parent = Q_NULLPTR) : QListView(parent) {}
    virtual ~TokensListView() {}
    QModelIndexList selectedIndexesP() const { return selectedIndexes(); }
};

class Ui_NTP1Summary
{
public:
    QPixmap      left_logo_pix;
    QGridLayout* main_layout;
    QVBoxLayout* left_logo_layout;
    QLabel*      left_logo_label;
    QVBoxLayout* logo_layout;
    QVBoxLayout* right_balance_layout;
    QFrame*      wallet_contents_frame;
    QVBoxLayout* verticalLayout;
    QHBoxLayout* verticalLayoutContent;
    QHBoxLayout* horizontalLayout_2;
    QLabel*      upper_table_label;
    QPushButton* issueNewNTP1TokenButton;
    QLabel*      upper_table_loading_label;
    QLineEdit*   filter_lineEdit;
    QLabel*      labelBlockchainSyncStatus;

    TokensListView* listTokens;

    QWidget*     bottom_bar_widget;
    QLabel*      bottom_bar_logo_label;
    QGridLayout* bottom_layout;
    QPixmap      bottom_logo_pix;

    IssueNewNTP1TokenDialog* issueNewNTP1TokenDialog;
    QMovie*                  loadIssuedNTP1SpinnerMovie;
    QLabel*                  loadIssuedNTP1SpinnerLabel;

    int bottom_bar_downscale_factor;

    void setupUi(QWidget* NTP1SummaryPage)
    {
        if (NTP1SummaryPage->objectName().isEmpty())
            NTP1SummaryPage->setObjectName(QStringLiteral("NTP1SummaryPage"));
        NTP1SummaryPage->resize(761, 452);
        main_layout = new QGridLayout(NTP1SummaryPage);
        main_layout->setObjectName(QStringLiteral("horizontalLayout"));
        left_logo_layout = new QVBoxLayout();
        left_logo_layout->setObjectName(QStringLiteral("verticalLayout_2"));
        left_logo_label = new QLabel(NTP1SummaryPage);
        left_logo_label->setObjectName(QStringLiteral("frame"));

        //        logo_label->setFrameShadow(QFrame::Raised);
        left_logo_label->setLineWidth(0);
        //        logo_label->setFrameStyle(QFrame::StyledPanel);

        left_logo_pix = QPixmap(":images/neblio_vertical");
        left_logo_pix =
            left_logo_pix.scaledToHeight(NTP1SummaryPage->height() * 3. / 4., Qt::SmoothTransformation);
        left_logo_label->setPixmap(left_logo_pix);
        left_logo_label->setAlignment(Qt::AlignCenter);

        //        sendTokensWidget               = new NTP1SendDialog(tokenListModel);
        //        sendTokensWidgetGroupBoxLayout = new QGridLayout;
        //        sendTokensWidgetGroupBox       = new QGroupBox;
        //        sendTokensWidgetGroupBox->setLayout(sendTokensWidgetGroupBoxLayout);
        //        sendTokensWidgetGroupBoxLayout->addWidget(sendTokensWidget);
        //        sendTokensWidgetGroupBox->setStyleSheet(
        //            "QGroupBox { background-color: white; border: 1px solid #BBBBBB ;}");

        logo_layout = new QVBoxLayout(left_logo_label);
        logo_layout->setObjectName(QStringLiteral("verticalLayout_4"));

        left_logo_layout->addWidget(left_logo_label);

        main_layout->addLayout(left_logo_layout, 0, 0, 1, 1);

        bottom_logo_pix             = QPixmap(":images/neblio_horizontal");
        bottom_bar_widget           = new QWidget(NTP1SummaryPage);
        bottom_layout               = new QGridLayout(bottom_bar_widget);
        bottom_bar_logo_label       = new QLabel(bottom_bar_widget);
        bottom_bar_downscale_factor = 8;

        main_layout->addWidget(bottom_bar_widget, 1, 0, 1, 2);
        bottom_bar_widget->setLayout(bottom_layout);
        bottom_layout->addWidget(bottom_bar_logo_label, 0, 0, 1, 1);
        bottom_logo_pix = bottom_logo_pix.scaledToHeight(
            NTP1SummaryPage->height() / bottom_bar_downscale_factor, Qt::SmoothTransformation);
        bottom_bar_logo_label->setPixmap(bottom_logo_pix);
        bottom_bar_logo_label->setAlignment(Qt::AlignRight);

        right_balance_layout = new QVBoxLayout();
        right_balance_layout->setObjectName(QStringLiteral("verticalLayout_3"));
        wallet_contents_frame = new QFrame(NTP1SummaryPage);
        wallet_contents_frame->setObjectName(QStringLiteral("frame_2"));
        wallet_contents_frame->setFrameShape(QFrame::StyledPanel);
        //        wallet_contents_frame->setFrameShadow(QFrame::Raised);
        verticalLayout        = new QVBoxLayout(wallet_contents_frame);
        verticalLayoutContent = new QHBoxLayout;
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        //        showSendDialogButton = new QPushButton;
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        upper_table_label = new QLabel(wallet_contents_frame);
        upper_table_label->setObjectName(QStringLiteral("label_4"));
        issueNewNTP1TokenButton = new QPushButton();
        issueNewNTP1TokenButton->setIcon(QIcon(":/icons/add_new_ntp1"));
        issueNewNTP1TokenButton->setToolTip("Issue a new NTP1 token");
        loadIssuedNTP1SpinnerLabel = new QLabel(NTP1SummaryPage);
        loadIssuedNTP1SpinnerMovie = new QMovie(":images/update-spinner", QByteArray(), NTP1SummaryPage);

        upper_table_loading_label = new QLabel(wallet_contents_frame);
        upper_table_loading_label->setObjectName(QStringLiteral("upper_table_loading_label"));
        upper_table_loading_label->setText("(Updating...)");
        filter_lineEdit = new QLineEdit(wallet_contents_frame);
        filter_lineEdit->setPlaceholderText("Filter (Ctrl+F)");

        horizontalLayout_2->addWidget(upper_table_label);
        horizontalLayout_2->addWidget(upper_table_loading_label);

        labelBlockchainSyncStatus = new QLabel(wallet_contents_frame);
        labelBlockchainSyncStatus->setObjectName(QStringLiteral("labelBlockchainSyncStatus"));
        labelBlockchainSyncStatus->setStyleSheet(QStringLiteral("QLabel { color: #7F4BC8; }"));
        labelBlockchainSyncStatus->setText(QStringLiteral("(blockchain out of sync)"));
        labelBlockchainSyncStatus->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

        horizontalLayout_2->addWidget(labelBlockchainSyncStatus);

        horizontalLayout_2->addWidget(issueNewNTP1TokenButton, 0, Qt::AlignmentFlag::AlignRight);
        horizontalLayout_2->addWidget(loadIssuedNTP1SpinnerLabel, 0, Qt::AlignmentFlag::AlignRight);
        loadIssuedNTP1SpinnerLabel->setVisible(false);

        verticalLayout->addLayout(horizontalLayout_2);
        verticalLayout->addLayout(verticalLayoutContent);
        verticalLayoutContent->addWidget(filter_lineEdit);
        //        verticalLayoutContent->addWidget(showSendDialogButton);

        listTokens = new TokensListView(wallet_contents_frame);
        listTokens->setObjectName(QStringLiteral("listTokens"));
        listTokens->setStyleSheet(QStringLiteral("QListView { background: transparent; }"));
        listTokens->setFrameShape(QFrame::NoFrame);
        //        listTokens->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        //        listTokens->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        //        listTokens->setSelectionMode(QAbstractItemView::NoSelection);

        verticalLayout->addWidget(listTokens);

        right_balance_layout->addWidget(wallet_contents_frame);
        //        right_balance_layout->addWidget(sendTokensWidgetGroupBox);

        main_layout->addLayout(right_balance_layout, 0, 1, 1, 1);

        issueNewNTP1TokenDialog = new IssueNewNTP1TokenDialog(NTP1SummaryPage);

        retranslateUi(NTP1SummaryPage);

        QMetaObject::connectSlotsByName(NTP1SummaryPage);
    } // setupUi

    void retranslateUi(QWidget* NTP1SummaryPage)
    {
        NTP1SummaryPage->setWindowTitle(QApplication::translate("NTP1Summary", "Form", Q_NULLPTR));
        upper_table_label->setText(
            QApplication::translate("NTP1Summary", "<b>NTP1 Tokens</b>", Q_NULLPTR));
#ifndef QT_NO_TOOLTIP
        labelBlockchainSyncStatus->setToolTip(
            QApplication::translate("NTP1Summary",
                                    "The displayed information may be out of date. Your wallet "
                                    "automatically synchronizes with the neblio network after a "
                                    "connection is established, but this process has not completed yet.",
                                    Q_NULLPTR));
#endif // QT_NO_TOOLTIP
    }  // retranslateUi
};

QT_END_NAMESPACE

#endif // UI_NTP1SUMMARY_H

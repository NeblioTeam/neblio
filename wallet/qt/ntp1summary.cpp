#include "ntp1summary.h"
#include "ui_ntp1summary.h"

#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "ntp1/ntp1tokenlistitemdelegate.h"

#include <QKeyEvent>

NTP1Summary::NTP1Summary(QWidget *parent) :
    QWidget(parent),
    ui(new Ui_NTP1Summary),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    model(0),
    filter(0),
    tokenDelegate(new NTP1TokenListItemDelegate)
{
    ui->setupUi(this);

    ui->listTokens->setItemDelegate(tokenDelegate);
    ui->listTokens->setIconSize(QSize(NTP1TokenListItemDelegate::DECORATION_SIZE, NTP1TokenListItemDelegate::DECORATION_SIZE));
    ui->listTokens->setMinimumHeight(NTP1TokenListItemDelegate::NUM_ITEMS * (NTP1TokenListItemDelegate::DECORATION_SIZE + 2));
    ui->listTokens->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTokens, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTokenClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelBlockchainSyncStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    model = new NTP1TokenListModel;
    filter = new NTP1TokenListFilterProxy(ui->filter_lineEdit);
    setModel(model);

    connect(model, &NTP1TokenListModel::signal_walletUpdateRunning,
            ui->upper_table_loading_label, &QLabel::setVisible);
    connect(ui->filter_lineEdit, &QLineEdit::textChanged,
            filter, &NTP1TokenListFilterProxy::setFilterWildcard);
}

void NTP1Summary::handleTokenClicked(const QModelIndex &index)
{
    if(filter)
        emit tokenClicked(filter->mapToSource(index));
}

void NTP1Summary::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier)) {
        ui->filter_lineEdit->setFocus();
    }
}

NTP1Summary::~NTP1Summary()
{
    delete ui;
}

void NTP1Summary::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
}

void NTP1Summary::setModel(NTP1TokenListModel *model)
{
    if(model)
    {
        filter->setSourceModel(model);
        ui->listTokens->setModel(filter);
    }
}

void NTP1Summary::showOutOfSyncWarning(bool fShow)
{
    ui->labelBlockchainSyncStatus->setVisible(fShow);
}

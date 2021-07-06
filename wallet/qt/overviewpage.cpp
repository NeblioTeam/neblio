#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "ui_interface.h"
#include "walletmodel.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate() : QAbstractItemDelegate(), unit(BitcoinUnits::BTC) {}

    inline void paint(QPainter* painter, const QStyleOptionViewItem& option,
                      const QModelIndex& index) const
    {
        painter->save();

        QIcon icon     = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int   xspace     = DECORATION_SIZE + 8;
        int   ypad       = 6;
        int   halfheight = (mainRect.height() - 2 * ypad) / 2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top() + ypad, mainRect.width() - xspace,
                         halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top() + ypad + halfheight,
                          mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date       = index.data(TransactionTableModel::DateRole).toDateTime();
        QString   address    = index.data(Qt::DisplayRole).toString();
        qint64    amount     = index.data(TransactionTableModel::AmountRole).toLongLong();
        QString   famount    = index.data(TransactionTableModel::FormattedAmountRole).toString();
        bool      confirmed  = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant  value      = index.data(Qt::ForegroundRole);
        bool      isNTP1     = index.data(TransactionTableModel::IsNTP1Role).toBool();
        QColor    foreground = option.palette.color(QPalette::Text);
        if (value.template canConvert<QColor>()) {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft | Qt::AlignVCenter, address);

        if (amount < 0) {
            foreground = COLOR_NEGATIVE;
        } else if (!confirmed) {
            foreground = COLOR_UNCONFIRMED;
        } else {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText;
        if (isNTP1) {
            amountText = famount;
        } else {
            amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        }
        if (!confirmed) {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget* parent)
    : QWidget(parent), ui(new Ui::OverviewPage), currentBalance(-1), currentStake(0),
      currentUnconfirmedBalance(-1), currentImmatureBalance(-1), txdelegate(new TxViewDelegate()),
      filter(0)
{
    ui->setupUi(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this,
            SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    showRescanControls(false);
    walletBlockchainRescanStartedConnection = uiInterface.WalletBlockchainRescanStarted.connect(
        [this]() { QTimer::singleShot(0, this, [this]() { this->startRescan(); }); });
    walletBlockchainRescanEndedConnection = uiInterface.WalletBlockchainRescanEnded.connect(
        [this]() { QTimer::singleShot(0, this, [this]() { this->endRescan(); }); });
    walletBlockchainRescanAtHeightConnect =
        uiInterface.WalletBlockchainRescanAtHeight.connect([this](double progress) {
            QTimer::singleShot(0, this, [this, progress]() { this->setRescanProgress(progress); });
        });
}

void OverviewPage::handleTransactionClicked(const QModelIndex& index)
{
    if (filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    walletBlockchainRescanStartedConnection.disconnect();
    walletBlockchainRescanEndedConnection.disconnect();
    walletBlockchainRescanAtHeightConnect.disconnect();
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance,
                              qint64 immatureBalance)
{
    int unit                  = model->getOptionsModel()->getDisplayUnit();
    currentBalance            = balance;
    currentStake              = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance    = immatureBalance;
    ui->spendable_value_label->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->stake_value_label->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->unconfirmed_value_label->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->immature_value_label->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));
    ui->total_value_label->setText(
        BitcoinUnits::formatWithUnit(unit, balance + stake + unconfirmedBalance + immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->immature_value_label->setVisible(showImmature);
    ui->immature_title_label->setVisible(showImmature);
}

void OverviewPage::setUnknownBalance()
{
    ui->spendable_value_label->setText("Updating...");
    ui->stake_value_label->setText("Updating...");
    ui->unconfirmed_value_label->setText("Updating...");
    ui->immature_value_label->setText("Updating...");
    ui->total_value_label->setText("Updating...");
    ui->immature_value_label->setVisible(false);
    ui->immature_title_label->setVisible(false);
}

void OverviewPage::setModel(WalletModel* modelIn)
{
    this->model = modelIn;
    if (modelIn && modelIn->getOptionsModel()) {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(modelIn->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setUnknownBalance();            // we set balances to zero initially
        modelIn->checkBalanceChanged(); // then we trigger a check asynchronously
        connect(modelIn, &WalletModel::balanceChanged, this, &OverviewPage::setBalance);

        connect(modelIn->getOptionsModel(), &OptionsModel::displayUnitChanged, this,
                &OverviewPage::updateDisplayUnit);
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if (model && model->getOptionsModel()) {
        if (currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance,
                       currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = model->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::startRescan()
{
    showRescanControls(true);
    setRescanProgress(0.);
    ui->wallet_blockchain_rescan_progress->setMaximum(std::numeric_limits<int>::max());
    emit rescanStarted();
}

void OverviewPage::endRescan() { showRescanControls(false); }

void OverviewPage::setRescanProgress(double progressFromZeroToOne)
{
    if (progressFromZeroToOne >= 1.) {
        ui->wallet_blockchain_rescan_progress->setValue(std::numeric_limits<int>::max());
    } else if (progressFromZeroToOne <= 0.) {
        ui->wallet_blockchain_rescan_progress->setValue(0);
    } else {
        const int progress = static_cast<int>(static_cast<double>(std::numeric_limits<int>::max()) *
                                              static_cast<double>(progressFromZeroToOne));
        ui->wallet_blockchain_rescan_progress->setValue(progress);
    }
}

void OverviewPage::showRescanControls(bool show)
{
    ui->wallet_blockchain_rescan_label->setVisible(show);
    ui->wallet_blockchain_rescan_progress->setVisible(show);
}

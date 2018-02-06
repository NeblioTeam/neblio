#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    isUpdateRunning = false;
    updateConcluderTimer = new QTimer(this);
    updateConcluderTimeout = 3000;
    updateCheckTimer = new QTimer(this);
    updateCheckTimerTimeout = 15*60*1000; //check for updates every 15 minutes

    ui->setupUi(this);

    setupUpdateControls();

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    updateCheckTimer->start(updateCheckTimerTimeout);
    checkForNeblioUpdates();
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

void OverviewPage::updateCheckAnimation_frameChanged(int frameNumber) {
    if(frameNumber == (bottom_bar_updater_check_movie->frameCount()-1)) {
        bottom_bar_updater_check_movie->stop();
    }
}

void OverviewPage::checkForNeblioUpdates()
{
    if(!isUpdateRunning) {
        printf("Checking for updates...\n");
        ui->bottom_bar_updater_label->setToolTip("Checking for updates...");
        ui->bottom_bar_updater_label->setMovie(bottom_bar_updater_spinner_movie);
        bottom_bar_updater_spinner_movie->start();
        latestVersion.clear();
        updateAvailablePromise = boost::promise<bool>();
        updateAvailableFuture = updateAvailablePromise.get_future();
        boost::thread updaterThread(boost::bind(&NeblioUpdater::checkIfUpdateIsAvailable,
                                    &neblioUpdater,
                                    boost::ref(updateAvailablePromise),
                                    boost::ref(latestVersion)
                                    ));
        updaterThread.detach();
        updateConcluderTimer->start(updateConcluderTimeout);
        isUpdateRunning = true;
    }
}

void OverviewPage::finishCheckForNeblioUpdates()
{
    if(isUpdateRunning) {
        printf("Concluding update check...\n");
        try {
            bool updateAvailable = updateAvailableFuture.get();
            if(updateAvailable) {
                ui->bottom_bar_updater_label->setMovie(bottom_bar_updater_no_update_movie);
                bottom_bar_updater_no_update_movie->start();
                ui->bottom_bar_updater_label->setToolTip("An update exists. Please visit nebl.io and download it.");
            } else {
                ui->bottom_bar_updater_label->setMovie(bottom_bar_updater_check_movie);
                bottom_bar_updater_check_movie->start();
                ui->bottom_bar_updater_label->setToolTip("Your Neblio client is up-to-date.");
            }
        } catch (std::exception& ex) {
            ui->bottom_bar_updater_label->setMovie(bottom_bar_updater_error_movie);
            bottom_bar_updater_error_movie->start();
            ui->bottom_bar_updater_label->setToolTip(QString("Unable to retrieve update information: ") + QString(ex.what()));
        }
        updateConcluderTimer->stop();
        printf("Done with updates check.\n");
        isUpdateRunning = false;
    }
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->spendable_value_label->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->stake_value_label->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->unconfirmed_value_label->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->immature_value_label->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));
    ui->total_value_label->setText(BitcoinUnits::formatWithUnit(unit, balance + stake + unconfirmedBalance + immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->immature_value_label->setVisible(showImmature);
    ui->immature_title_label->setVisible(showImmature);
}

void OverviewPage::setupUpdateControls()
{
    // Updater animations
    QSize updaterIconSize(this->height()/ui->bottom_bar_downscale_factor,
                          this->height()/ui->bottom_bar_downscale_factor);

    bottom_bar_updater_check_movie = new QMovie(":images/update-animated-check", QByteArray(), ui->bottom_bar_widget);
    bottom_bar_updater_check_movie->setScaledSize(updaterIconSize);
    bottom_bar_updater_check_movie->start();

    bottom_bar_updater_no_update_movie = new QMovie(":images/update-update-available", QByteArray(), ui->bottom_bar_widget);
    bottom_bar_updater_no_update_movie->setScaledSize(updaterIconSize);
    bottom_bar_updater_no_update_movie->start();

    bottom_bar_updater_error_movie = new QMovie(":images/update-error", QByteArray(), ui->bottom_bar_widget);
    bottom_bar_updater_error_movie->setScaledSize(updaterIconSize);
    bottom_bar_updater_error_movie->start();

    bottom_bar_updater_spinner_movie = new QMovie(":images/update-spinner", QByteArray(), ui->bottom_bar_widget);
    bottom_bar_updater_spinner_movie->setScaledSize(updaterIconSize);
    bottom_bar_updater_spinner_movie->start();

    connect(bottom_bar_updater_check_movie, &QMovie::frameChanged,
            this, &OverviewPage::updateCheckAnimation_frameChanged);

    connect(ui->bottom_bar_updater_label, &ClickableLabel::clicked,
            this, &OverviewPage::checkForNeblioUpdates);

    connect(updateConcluderTimer, &QTimer::timeout,
            this, &OverviewPage::finishCheckForNeblioUpdates);

    connect(updateCheckTimer, &QTimer::timeout,
            this, &OverviewPage::checkForNeblioUpdates);
}

void OverviewPage::setModel(WalletModel *model)
{
    this->model = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

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

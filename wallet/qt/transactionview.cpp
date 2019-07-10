#include "transactionview.h"

#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "transactionfilterproxy.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QScrollBar>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

TransactionView::TransactionView(QWidget* parent)
    : QWidget(parent), model(0), transactionProxyModel(0), transactionView(0)
{
    // Build filter row
    setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
#ifdef Q_OS_MAC
    hlayout->setSpacing(5);
    hlayout->addSpacing(5);
#else
    hlayout->setSpacing(0);
    hlayout->addSpacing(2);
#endif

    showInactiveWidget = new QCheckBox(this);
    showInactiveWidget->setToolTip("Show Conflicted & Invalid Transactions");
    hlayout->addWidget(showInactiveWidget);

#ifdef Q_OS_MAC
    hlayout->addSpacing(5);
#else
    hlayout->addSpacing(2);
#endif

    dateWidget = new QComboBox(this);
#ifdef Q_OS_MAC
    dateWidget->setFixedWidth(121);
#else
    dateWidget->setFixedWidth(120);
#endif
    dateWidget->addItem(tr("All"), All);
    dateWidget->addItem(tr("Today"), Today);
    dateWidget->addItem(tr("This week"), ThisWeek);
    dateWidget->addItem(tr("This month"), ThisMonth);
    dateWidget->addItem(tr("Last month"), LastMonth);
    dateWidget->addItem(tr("This year"), ThisYear);
    dateWidget->addItem(tr("Range..."), Range);
    hlayout->addWidget(dateWidget);

    typeWidget = new QComboBox(this);
#ifdef Q_OS_MAC
    typeWidget->setFixedWidth(121);
#else
    typeWidget->setFixedWidth(120);
#endif

    typeWidget->addItem(tr("All"), TransactionFilterProxy::ALL_TYPES);
    typeWidget->addItem(tr("Received with"),
                        TransactionFilterProxy::TYPE(TransactionRecord::RecvWithAddress) |
                            TransactionFilterProxy::TYPE(TransactionRecord::RecvFromOther));
    typeWidget->addItem(tr("Sent to"), TransactionFilterProxy::TYPE(TransactionRecord::SendToAddress) |
                                           TransactionFilterProxy::TYPE(TransactionRecord::SendToOther));
    typeWidget->addItem(tr("To yourself"), TransactionFilterProxy::TYPE(TransactionRecord::SendToSelf));
    typeWidget->addItem(tr("Mined"), TransactionFilterProxy::TYPE(TransactionRecord::Generated));
    typeWidget->addItem(tr("Other"), TransactionFilterProxy::TYPE(TransactionRecord::Other));

    hlayout->addWidget(typeWidget);

    addressWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    addressWidget->setPlaceholderText(tr("Enter address or label to search"));
#endif
    hlayout->addWidget(addressWidget);

    amountWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    amountWidget->setPlaceholderText(tr("Min amount"));
#endif
#ifdef Q_OS_MAC
    amountWidget->setFixedWidth(97);
#else
    amountWidget->setFixedWidth(100);
#endif
    amountWidget->setValidator(new QDoubleValidator(0, 1e20, 8, this));
    hlayout->addWidget(amountWidget);

    QVBoxLayout* vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->setSpacing(0);

    QTableView* view = new QTableView(this);
    vlayout->addLayout(hlayout);
    vlayout->addWidget(createDateRangeWidget());
    vlayout->addWidget(view);
    vlayout->setSpacing(0);
    int width = view->verticalScrollBar()->sizeHint().width();
    // Cover scroll bar width with spacing
#ifdef Q_OS_MAC
    hlayout->addSpacing(width + 2);
#else
    hlayout->addSpacing(width);
#endif
    // Always show scroll bar
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);

    transactionView = view;

    // Actions
    QAction* copyAddressAction = new QAction(tr("Copy address"), this);
    QAction* copyLabelAction   = new QAction(tr("Copy label"), this);
    QAction* copyAmountAction  = new QAction(tr("Copy amount"), this);
    QAction* copyTxIDAction    = new QAction(tr("Copy transaction ID"), this);
    QAction* editLabelAction   = new QAction(tr("Edit label"), this);
    QAction* showDetailsAction = new QAction(tr("Show transaction details"), this);
    QAction* viewTxInExplorer  = new QAction(tr("View in block explorer"), this);

    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTxIDAction);
    contextMenu->addAction(editLabelAction);
    contextMenu->addAction(showDetailsAction);
    contextMenu->addAction(viewTxInExplorer);

    // Connect actions
    connect(showInactiveWidget, &QCheckBox::stateChanged, this, &TransactionView::showInactive);
    connect(dateWidget, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            &TransactionView::chooseDate);
    connect(typeWidget, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            &TransactionView::chooseType);
    connect(addressWidget, &QLineEdit::textChanged, this, &TransactionView::changedPrefix);
    connect(amountWidget, &QLineEdit::textChanged, this, &TransactionView::changedAmount);

    connect(view, &QTableView::doubleClicked, this, &TransactionView::doubleClicked);
    connect(view, &QTableView::customContextMenuRequested, this, &TransactionView::contextualMenu);

    connect(copyAddressAction, &QAction::triggered, this, &TransactionView::copyAddress);
    connect(copyLabelAction, &QAction::triggered, this, &TransactionView::copyLabel);
    connect(copyAmountAction, &QAction::triggered, this, &TransactionView::copyAmount);
    connect(copyTxIDAction, &QAction::triggered, this, &TransactionView::copyTxID);
    connect(editLabelAction, &QAction::triggered, this, &TransactionView::editLabel);
    connect(showDetailsAction, &QAction::triggered, this, &TransactionView::showDetails);
    connect(viewTxInExplorer, &QAction::triggered, this, &TransactionView::showInBlockExplorer);
}

void TransactionView::setModel(WalletModel* model)
{
    this->model = model;
    if (model) {
        transactionProxyModel = new TransactionFilterProxy(this);
        transactionProxyModel->setSourceModel(model->getTransactionTableModel());
        transactionProxyModel->setDynamicSortFilter(true);
        transactionProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        transactionProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

        transactionProxyModel->setSortRole(Qt::EditRole);

        transactionView->setModel(transactionProxyModel);
        transactionView->setAlternatingRowColors(true);
        transactionView->setSelectionBehavior(QAbstractItemView::SelectRows);
        transactionView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        transactionView->setSortingEnabled(true);
        transactionView->sortByColumn(TransactionTableModel::Date, Qt::DescendingOrder);
        transactionView->verticalHeader()->hide();

        transactionView->horizontalHeader()->resizeSection(TransactionTableModel::Status, 23);
        transactionView->horizontalHeader()->resizeSection(TransactionTableModel::Date, 120);
        transactionView->horizontalHeader()->resizeSection(TransactionTableModel::Type, 120);
        transactionView->horizontalHeader()->setResizeMode(TransactionTableModel::ToAddress,
                                                           QHeaderView::Stretch);
        transactionView->horizontalHeader()->resizeSection(TransactionTableModel::Amount, 120);
    }
}

void TransactionView::showInactive(int state)
{
    if (!transactionProxyModel)
        return;
    transactionProxyModel->setShowInactive((state == Qt::Checked));
}

void TransactionView::chooseDate(int idx)
{
    if (!transactionProxyModel)
        return;
    QDate current = QDate::currentDate();
    dateRangeWidget->setVisible(false);
    switch (dateWidget->itemData(idx).toInt()) {
    case All:
        transactionProxyModel->setDateRange(TransactionFilterProxy::MIN_DATE,
                                            TransactionFilterProxy::MAX_DATE);
        break;
    case Today:
        transactionProxyModel->setDateRange(QDateTime(current), TransactionFilterProxy::MAX_DATE);
        break;
    case ThisWeek: {
        // Find last Monday
        QDate startOfWeek = current.addDays(-(current.dayOfWeek() - 1));
        transactionProxyModel->setDateRange(QDateTime(startOfWeek), TransactionFilterProxy::MAX_DATE);

    } break;
    case ThisMonth:
        transactionProxyModel->setDateRange(QDateTime(QDate(current.year(), current.month(), 1)),
                                            TransactionFilterProxy::MAX_DATE);
        break;
    case LastMonth:
        transactionProxyModel->setDateRange(QDateTime(QDate(current.year(), current.month() - 1, 1)),
                                            QDateTime(QDate(current.year(), current.month(), 1)));
        break;
    case ThisYear:
        transactionProxyModel->setDateRange(QDateTime(QDate(current.year(), 1, 1)),
                                            TransactionFilterProxy::MAX_DATE);
        break;
    case Range:
        dateRangeWidget->setVisible(true);
        dateRangeChanged();
        break;
    }
}

void TransactionView::chooseType(int idx)
{
    if (!transactionProxyModel)
        return;
    transactionProxyModel->setTypeFilter(typeWidget->itemData(idx).toInt());
}

void TransactionView::changedPrefix(const QString& prefix)
{
    if (!transactionProxyModel)
        return;
    transactionProxyModel->setAddressPrefix(prefix);
}

void TransactionView::changedAmount(const QString& amount)
{
    if (!transactionProxyModel)
        return;
    qint64 amount_parsed = 0;
    if (BitcoinUnits::parse(model->getOptionsModel()->getDisplayUnit(), amount, &amount_parsed)) {
        transactionProxyModel->setMinAmount(amount_parsed);
    } else {
        transactionProxyModel->setMinAmount(0);
    }
}

void TransactionView::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(this, tr("Export Transaction Data"), QString(),
                                                tr("Comma separated file (*.csv)"));

    if (filename.isNull())
        return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(transactionProxyModel);
    writer.addColumn(tr("Confirmed"), 0, TransactionTableModel::ConfirmedRole);
    writer.addColumn(tr("Date"), 0, TransactionTableModel::DateRole);
    writer.addColumn(tr("Type"), TransactionTableModel::Type, Qt::EditRole);
    writer.addColumn(tr("Label"), 0, TransactionTableModel::LabelRole);
    writer.addColumn(tr("Address"), 0, TransactionTableModel::AddressRole);
    writer.addColumn(tr("Amount"), 0, TransactionTableModel::FormattedAmountRole);
    writer.addColumn(tr("ID"), 0, TransactionTableModel::TxIDRole);

    if (!writer.write()) {
        QMessageBox::critical(this, tr("Error exporting"),
                              tr("Could not write to file %1.").arg(filename), QMessageBox::Abort,
                              QMessageBox::Abort);
    }
}

void TransactionView::contextualMenu(const QPoint& point)
{
    QModelIndex index = transactionView->indexAt(point);
    if (index.isValid()) {
        contextMenu->exec(QCursor::pos());
    }
}

void TransactionView::copyAddress()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::AddressRole);
}

void TransactionView::copyLabel()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::LabelRole);
}

void TransactionView::copyAmount()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::FormattedAmountRole);
}

void TransactionView::copyTxID()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::TxIDRole);
}

void TransactionView::showInBlockExplorer()
{
    QModelIndexList selection = transactionView->selectionModel()->selectedRows(0);

    QString txId;
    if (!selection.isEmpty()) {
        txId = selection.at(0).data(TransactionTableModel::TxIDRole).toString();
    }
    if (!txId.isEmpty()) {
        QString link = QString::fromStdString(
            NTP1Tools::GetURL_ExplorerTransactionInfo(txId.toStdString(), fTestNet));
        if (!QDesktopServices::openUrl(QUrl(link))) {
            QMessageBox::warning(
                this, "Failed to open browser",
                "Could not open the web browser to view transaction information in block explorer");
        }
    } else {
        QMessageBox::warning(
            this, "Failed to retrieve transaction ID",
            "Failed to open transaction in block explorer. Failed to retrieve the transaction ID");
    }
}

void TransactionView::editLabel()
{
    if (!transactionView->selectionModel() || !model)
        return;
    QModelIndexList selection = transactionView->selectionModel()->selectedRows();
    if (!selection.isEmpty()) {
        AddressTableModel* addressBook = model->getAddressTableModel();
        if (!addressBook)
            return;
        QString address = selection.at(0).data(TransactionTableModel::AddressRole).toString();
        if (address.isEmpty()) {
            // If this transaction has no associated address, exit
            return;
        }
        // Is address in address book? Address book can miss address when a transaction is
        // sent from outside the UI.
        int idx = addressBook->lookupAddress(address);
        if (idx != -1) {
            // Edit sending / receiving address
            QModelIndex modelIdx = addressBook->index(idx, 0, QModelIndex());
            // Determine type of address, launch appropriate editor dialog type
            QString type = modelIdx.data(AddressTableModel::TypeRole).toString();

            EditAddressDialog dlg(type == AddressTableModel::Receive
                                      ? EditAddressDialog::EditReceivingAddress
                                      : EditAddressDialog::EditSendingAddress,
                                  this);
            dlg.setModel(addressBook);
            dlg.loadRow(idx);
            dlg.exec();
        } else {
            // Add sending address
            EditAddressDialog dlg(EditAddressDialog::NewSendingAddress, this);
            dlg.setModel(addressBook);
            dlg.setAddress(address);
            dlg.exec();
        }
    }
}

void TransactionView::showDetails()
{
    if (!transactionView->selectionModel())
        return;
    QModelIndexList selection = transactionView->selectionModel()->selectedRows();
    if (!selection.isEmpty()) {
        TransactionDescDialog dlg(selection.at(0));
        dlg.exec();
    }
}

QWidget* TransactionView::createDateRangeWidget()
{
    dateRangeWidget = new QFrame();
    dateRangeWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    dateRangeWidget->setContentsMargins(1, 1, 1, 1);
    QHBoxLayout* layout = new QHBoxLayout(dateRangeWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addSpacing(23);
    layout->addWidget(new QLabel(tr("Range:")));

    dateFrom = new QDateTimeEdit(this);
    dateFrom->setDisplayFormat("dd/MM/yy");
    dateFrom->setCalendarPopup(true);
    dateFrom->setMinimumWidth(100);
    dateFrom->setDate(QDate::currentDate().addDays(-7));
    layout->addWidget(dateFrom);
    layout->addWidget(new QLabel(tr("to")));

    dateTo = new QDateTimeEdit(this);
    dateTo->setDisplayFormat("dd/MM/yy");
    dateTo->setCalendarPopup(true);
    dateTo->setMinimumWidth(100);
    dateTo->setDate(QDate::currentDate());
    layout->addWidget(dateTo);
    layout->addStretch();

    // Hide by default
    dateRangeWidget->setVisible(false);

    // Notify on change
    connect(dateFrom, &QDateTimeEdit::dateChanged, this, &TransactionView::dateRangeChanged);
    connect(dateTo, &QDateTimeEdit::dateChanged, this, &TransactionView::dateRangeChanged);

    return dateRangeWidget;
}

void TransactionView::dateRangeChanged()
{
    if (!transactionProxyModel)
        return;
    transactionProxyModel->setDateRange(QDateTime(dateFrom->date()),
                                        QDateTime(dateTo->date()).addDays(1));
}

void TransactionView::focusTransaction(const QModelIndex& idx)
{
    if (!transactionProxyModel)
        return;
    QModelIndex targetIdx = transactionProxyModel->mapFromSource(idx);
    transactionView->scrollTo(targetIdx);
    transactionView->setCurrentIndex(targetIdx);
    transactionView->setFocus();
}

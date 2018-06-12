#include "ntp1senddialog.h"

#include "ntp1sendsingletokenfields.h"

NTP1SendDialog::NTP1SendDialog(NTP1TokenListModel* tokenListModel, QWidget* parent) : QWidget(parent)
{
    tokenListDataModel = tokenListModel;
    createWidgets();
}

NTP1SendDialog::~NTP1SendDialog() {}

void NTP1SendDialog::createWidgets()
{
    mainLayout             = new QGridLayout;
    addRecipientButton     = new QPushButton(this);
    sendButton             = new QPushButton(this);
    recipientWidgetsWidget = new QWidget;
    addRecipientButton->setText("Add &Recipient");
    addRecipientButton->setIcon(QIcon(":/icons/add"));
    sendButton->setText("Send");
    sendButton->setIcon(QIcon(":/icons/send"));
    this->setLayout(mainLayout);

    recipientWidgetsLayout     = new QVBoxLayout;
    recipientWidgetsScrollArea = new QScrollArea;
    recipientWidgetsWidget->setLayout(recipientWidgetsLayout);
    // The dot prevents propagating to children
    recipientWidgetsWidget->setStyleSheet(
        ".QWidget {background-color: white;border:none;font-family:'Open Sans,sans-serif'; }");
    recipientWidgetsScrollArea->setWidget(recipientWidgetsWidget);

    recipientWidgetsScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    recipientWidgetsScrollArea->setWidgetResizable(true);
    recipientWidgetsScrollArea->setFrameStyle(QFrame::NoFrame);
    recipientWidgetsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recipientWidgetsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    mainLayout->addWidget(recipientWidgetsScrollArea, 0, 0, 1, 3);
    mainLayout->addWidget(addRecipientButton, 1, 0, 1, 1);
    mainLayout->addWidget(sendButton, 1, 2, 1, 1);

    connect(addRecipientButton, &QPushButton::clicked, this, &NTP1SendDialog::slot_addRecipientWidget);

    // add one widget
    slot_addRecipientWidget();
}

boost::shared_ptr<NTP1Wallet> NTP1SendDialog::getLatestNTP1Wallet() const
{
    if (tokenListDataModel != Q_NULLPTR) {
        return tokenListDataModel->getCurrentWallet();
    } else {
        printf("Error while retrieving latest wallet. Token list data model is nullptr.");
        return Q_NULLPTR;
    }
}

void NTP1SendDialog::slot_addRecipientWidget()
{
    NTP1SendSingleTokenFields* widget =
        new NTP1SendSingleTokenFields(boost::bind(&NTP1SendDialog::getLatestNTP1Wallet, this));
    connect(widget, &NTP1SendSingleTokenFields::signal_closeThis, this,
            &NTP1SendDialog::slot_removeRecipientWidget);
    connect(this, &NTP1SendDialog::signal_updateAllRecipientDialogsTokens, widget,
            &NTP1SendSingleTokenFields::slot_updateTokenList);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    recipientWidgets.push_back(widget);
    recipientWidgetsLayout->addWidget(widget, 0, Qt::AlignTop);
    slot_actToShowOrHideCloseButtons();
}

void NTP1SendDialog::slot_removeRecipientWidget(QWidget* widget)
{
    if (recipientWidgets.size() <= 1) {
        return;
    }
    recipientWidgetsLayout->removeWidget(widget);
    auto it = std::find(recipientWidgets.begin(), recipientWidgets.end(), widget);
    delete widget;
    if (it != recipientWidgets.end()) {
        recipientWidgets.erase(it);
    } else {
        printf("Unable to find recipient widget to erase.");
    }
    slot_actToShowOrHideCloseButtons();
}

void NTP1SendDialog::slot_actToShowOrHideCloseButtons()
{
    if (recipientWidgets.size() > 1) {
        showAllCloseButtons();
    } else {
        hideAllCloseButtons();
    }
}

void NTP1SendDialog::slot_updateAllRecipientDialogsTokens()
{
    emit signal_updateAllRecipientDialogsTokens();
}

void NTP1SendDialog::showAllCloseButtons()
{
    for (NTP1SendSingleTokenFields* w : recipientWidgets) {
        w->slot_showClose();
    }
}

void NTP1SendDialog::hideAllCloseButtons()
{
    for (NTP1SendSingleTokenFields* w : recipientWidgets) {
        w->slot_hideClose();
    }
}

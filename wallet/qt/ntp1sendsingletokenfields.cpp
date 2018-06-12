#include "ntp1sendsingletokenfields.h"

NTP1SendSingleTokenFields::NTP1SendSingleTokenFields(
    std::function<boost::shared_ptr<NTP1Wallet>(void)> WalletRetriever, QWidget* parent)
    : QWidget(parent)
{
    retrieveLatestWallet = WalletRetriever;
    createWidgets();
}

NTP1SendSingleTokenFields::~NTP1SendSingleTokenFields() {}

void NTP1SendSingleTokenFields::slot_closeThis() { emit signal_closeThis(this); }

void NTP1SendSingleTokenFields::slot_hideClose() { closeButton->hide(); }

void NTP1SendSingleTokenFields::slot_showClose() { closeButton->show(); }

void NTP1SendSingleTokenFields::slot_updateTokenList()
{
    fillCurrentTokens();

    destination->clear();
    amount->clear();

    tokenType->clear();
    for (unsigned i = 0; i < currentTokens.size(); i++) {
        tokenType->addItem(currentTokens[i].icon, currentTokens[i].name);
    }
}

void NTP1SendSingleTokenFields::createWidgets()
{
    mainLayout  = new QGridLayout(this);
    amount      = new QLineEdit(this);
    destination = new QLineEdit(this);
    tokenType   = new QComboBox(this);
    closeButton = new QPushButton(this);

    {
        QFontMetrics fm    = amount->fontMetrics();
        int          width = fm.width(QString("1000000000"));
        amount->setFixedWidth(width);
    }

    this->setLayout(mainLayout);

    amount->setPlaceholderText("Amount");
    amount->setValidator(new QIntValidator(0, 1000000000, this));
    destination->setPlaceholderText("Destination address");

    mainLayout->addWidget(destination, 0, 0, 1, 1);
    mainLayout->addWidget(amount, 0, 1, 1, 1);
    mainLayout->addWidget(tokenType, 0, 2, 1, 1);
    mainLayout->addWidget(closeButton, 0, 3, 1, 1);

    closeButton->setIcon(QIcon(":/icons/remove"));

    connect(closeButton, &QPushButton::clicked, this, &NTP1SendSingleTokenFields::slot_closeThis);

    slot_updateTokenList();
}

void NTP1SendSingleTokenFields::fillCurrentTokens()
{
    boost::shared_ptr<NTP1Wallet> wallet = retrieveLatestWallet();

    if (!wallet)
        return;

    currentTokens.clear();
    for (long i = 0; i < wallet->getNumberOfTokens(); i++) {
        TokenData d;
        d.fill(i, wallet);
        currentTokens.push_back(d);
    }
}

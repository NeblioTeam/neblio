#include "ntp1sendsingletokenfields.h"

NTP1SendSingleTokenFields::NTP1SendSingleTokenFields(
    std::function<boost::shared_ptr<NTP1Wallet>(void)> WalletRetriever, QWidget* parent)
    : QWidget(parent)
{
    retrieveLatestWallet = WalletRetriever;
    createWidgets();
}

NTP1SendSingleTokenFields::~NTP1SendSingleTokenFields() {}

NTP1SendTokensOneRecipientData NTP1SendSingleTokenFields::createRecipientData() const
{
    NTP1SendTokensOneRecipientData result;
    auto                           selectedTokenIndex = tokenTypeComboBox->currentIndex();
    std::string                    selectedTokenName  = tokenTypeComboBox->currentText().toStdString();
    if (selectedTokenIndex >= static_cast<long>(currentTokens.size())) {
        std::string errorMsg = "While constructing recipient data for token number " +
                               ToString(selectedTokenIndex) + " (name: " + selectedTokenName +
                               "), an error occurred: "
                               "The selected token index exceeds the available tokens in the wallet.\n";
        printf("%s\n", errorMsg.c_str());
        throw std::runtime_error(errorMsg.c_str());
    }
    if (currentTokens[selectedTokenIndex].name.toStdString() != selectedTokenName) {
        std::string errorMsg = "While constructing recipient data for token number " +
                               ToString(selectedTokenIndex) + " (name: " + selectedTokenName +
                               "), an error occurred: "
                               "The selected token name does not match the expected index.\n";
        printf("%s\n", errorMsg.c_str());
        throw std::runtime_error(errorMsg.c_str());
    }
    if (amount->text().isEmpty() || destination->text().isEmpty()) {
        std::string errorMsg = "Please fill all the fields before attempting to send.\n";
        throw std::runtime_error(errorMsg.c_str());
    }
    {
        CBitcoinAddress address;
        address.SetString(destination->text().toStdString());
        if (!address.IsValid()) {
            std::string errorMsg =
                "The address \"" + destination->text().toStdString() + "\" is not valid.\n";
            printf("%s\n", errorMsg.c_str());
            throw std::runtime_error(errorMsg.c_str());
        }
    }
    result.amount      = FromString<uint64_t>(amount->text().toStdString());
    result.tokenId     = currentTokens[selectedTokenIndex].tokenId.toStdString();
    result.destination = destination->text().toStdString();
    return result;
}

void NTP1SendSingleTokenFields::resetAllFields()
{
    amount->clear();
    destination->clear();
}

void NTP1SendSingleTokenFields::slot_closeThis() { emit signal_closeThis(this); }

void NTP1SendSingleTokenFields::slot_hideClose() { closeButton->hide(); }

void NTP1SendSingleTokenFields::slot_showClose() { closeButton->show(); }

void NTP1SendSingleTokenFields::slot_updateTokenList()
{
    fillCurrentTokens();

    destination->clear();
    amount->clear();

    tokenTypeComboBox->clear();
    for (unsigned i = 0; i < currentTokens.size(); i++) {
        tokenTypeComboBox->addItem(currentTokens[i].icon, currentTokens[i].name);
    }
}

void NTP1SendSingleTokenFields::createWidgets()
{
    mainLayout        = new QGridLayout(this);
    destination       = new QLineEdit(this);
    amount            = new QLineEdit(this);
    tokenTypeComboBox = new QComboBox(this);
    closeButton       = new QPushButton(this);

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
    mainLayout->addWidget(tokenTypeComboBox, 0, 2, 1, 1);
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
        NTP1ListElementTokenData d;
        d.fill(i, wallet);
        currentTokens.push_back(d);
    }
}

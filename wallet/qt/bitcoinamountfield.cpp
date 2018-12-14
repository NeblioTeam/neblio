#include "bitcoinamountfield.h"
#include "bitcoinunits.h"
#include "ntp1/ntp1listelementtokendata.h"
#include "qvaluecombobox.h"

#include "guiconstants.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRegExpValidator>
#include <qmath.h>

BitcoinAmountField::BitcoinAmountField(bool EnableNTP1Tokens, QWidget* parent)
    : QWidget(parent), amount(0), currentUnit(-1)
{
    enableNTP1Tokens = EnableNTP1Tokens;

    amount = new QDoubleSpinBox(this);
    amount->setLocale(QLocale::c());
    amount->setDecimals(8);
    amount->installEventFilter(this);
    amount->setMaximumWidth(170);
    amount->setSingleStep(0.001);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(amount);
    unit = new QValueComboBox(this);
    unit->setModel(new BitcoinUnits(this));
    layout->addWidget(unit);
    //    layout->addStretch(1);
    layout->setContentsMargins(0, 0, 0, 0);

    if (enableNTP1Tokens) {
        tokenKindsComboBox = new QComboBox;
        tokenKindsComboBox->addItem(QIcon(QStringLiteral(":/icons/bitcoin")), "NEBL");
        layout->addWidget(tokenKindsComboBox);

        refreshTokensButton = new QToolButton;
        QIcon icon;
        icon.addFile(QStringLiteral(":/icons/refresh"), QSize(), QIcon::Normal, QIcon::Off);
        refreshTokensButton->setIcon(icon);
        refreshTokensButton->setToolTip("Refresh tokens list");
        layout->addWidget(refreshTokensButton);

        connect(refreshTokensButton, &QToolButton::clicked, this,
                &BitcoinAmountField::slot_updateTokensList);
        connect(tokenKindsComboBox,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                &BitcoinAmountField::slot_tokenChanged);
    }

    setLayout(layout);

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(amount);

    // If one if the widgets changes, the combined content changes as well
    connect(amount, SIGNAL(valueChanged(QString)), this, SIGNAL(textChanged()));
    connect(unit, SIGNAL(currentIndexChanged(int)), this, SLOT(unitChanged(int)));
    if (enableNTP1Tokens) {
        connect(tokenKindsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(unitChanged(int)));
    }

    // Set default based on configuration
    unitChanged(unit->currentIndex());
}

void BitcoinAmountField::setText(const QString& text)
{
    if (text.isEmpty())
        amount->clear();
    else
        amount->setValue(text.toDouble());
}

void BitcoinAmountField::clear()
{
    amount->clear();
    unit->setCurrentIndex(0);
    if (enableNTP1Tokens) {
        tokenKindsComboBox->setCurrentIndex(0);
        slot_updateTokensList();
    }
}

bool BitcoinAmountField::validate()
{
    bool valid = true;
    if (amount->value() == 0.0)
        valid = false;
    if (valid && !BitcoinUnits::parse(currentUnit, text(), 0))
        valid = false;

    setValid(valid);

    return valid;
}

void BitcoinAmountField::setValid(bool valid)
{
    if (valid)
        amount->setStyleSheet("");
    else
        amount->setStyleSheet(STYLE_INVALID);
}

QString BitcoinAmountField::text() const
{
    if (amount->text().isEmpty())
        return QString();
    else
        return amount->text();
}

void BitcoinAmountField::slot_updateTokensList()
{
    if (enableNTP1Tokens) {
        boost::shared_ptr<NTP1Wallet> wallet;
        if (ntp1TokenListModelInstance.load()) {
            wallet = ntp1TokenListModelInstance.load()->getCurrentWallet();
        }

        // update the internal list
        tokenKindsList.clear();
        for (long i = 0; i < wallet->getNumberOfTokens(); i++) {
            NTP1ListElementTokenData d;
            d.fill(i, wallet);
            tokenKindsList.push_back(d);
        }

        std::sort(tokenKindsList.begin(), tokenKindsList.end(),
                  [](const NTP1ListElementTokenData& k1, const NTP1ListElementTokenData& k2) {
                      return k1.name < k2.name;
                  });

        // update the combobox
        tokenKindsComboBox->clear();
        tokenKindsComboBox->addItem(QIcon(QStringLiteral(":/icons/bitcoin")), "NEBL");
        for (unsigned i = 0; i < tokenKindsList.size(); i++) {
            tokenKindsComboBox->addItem(tokenKindsList[i].icon,
                                        tokenKindsList[i].name + " (" +
                                            QString::number(tokenKindsList[i].amount) + ") (" +
                                            tokenKindsList[i].tokenId.left(10) + "...)");
        }
    }
}

void BitcoinAmountField::slot_tokenChanged()
{
    if (enableNTP1Tokens && tokenKindsComboBox->count() > 0) {
        unit->setEnabled(tokenKindsComboBox->currentIndex() == 0);
        if (isNTP1TokenSelected()) {
            amount->setSingleStep(1);
        } else {
            amount->setSingleStep(0.001);
        }
    }
}

bool BitcoinAmountField::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Comma) {
            // Translate a comma into a period
            QKeyEvent periodKeyEvent(event->type(), Qt::Key_Period, keyEvent->modifiers(), ".",
                                     keyEvent->isAutoRepeat(), keyEvent->count());
            qApp->sendEvent(object, &periodKeyEvent);
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

void BitcoinAmountField::focusInEvent(QFocusEvent*)
{
    // Clear invalid flag on focus
    setValid(true);
}

QWidget* BitcoinAmountField::setupTabChain(QWidget* prev)
{
    QWidget::setTabOrder(prev, amount);
    return amount;
}

qint64 BitcoinAmountField::value(bool* valid_out) const
{
    qint64 val_out = 0;
    bool   valid   = BitcoinUnits::parse(currentUnit, text(), &val_out);
    if (valid_out) {
        *valid_out = valid;
    }
    return val_out;
}

void BitcoinAmountField::setValue(qint64 value) { setText(BitcoinUnits::format(currentUnit, value)); }

QString BitcoinAmountField::getSelectedTokenId() const
{
    if (enableNTP1Tokens && tokenKindsComboBox->count() > 0) {
        int selectedIndex = tokenKindsComboBox->currentIndex();
        if (selectedIndex == 0) {
            return QString::fromStdString(NTP1SendTxData::NEBL_TOKEN_ID);
        } else {
            return tokenKindsList.at(selectedIndex - 1).tokenId; // element 0 is NEBL
        }
    }
    return "";
}

bool BitcoinAmountField::isNTP1TokenSelected() const
{
    return (enableNTP1Tokens && tokenKindsComboBox->currentIndex() != 0);
}

void BitcoinAmountField::unitChanged(int idx)
{
    // Use description tooltip for current unit for the combobox
    unit->setToolTip(unit->itemData(idx, Qt::ToolTipRole).toString());

    // Determine new unit ID
    int newUnit = 0;
    if (!isNTP1TokenSelected()) {
        newUnit = unit->itemData(idx, BitcoinUnits::UnitRole).toInt();
    } else {
        newUnit = static_cast<int>(BitcoinUnit::NTP1);
    }

    // Parse current value and convert to new unit
    bool   valid        = false;
    qint64 currentValue = value(&valid);

    currentUnit = newUnit;

    // Set max length after retrieving the value, to prevent truncation
    amount->setDecimals(BitcoinUnits::decimals(currentUnit));
    amount->setMaximum(qPow(10, BitcoinUnits::amountDigits(currentUnit)) -
                       qPow(10, -amount->decimals()));

    if (valid) {
        // If value was valid, re-place it in the widget with the new unit
        if (!isNTP1TokenSelected()) {
            setValue(currentValue);
        } else {
            setText("");
        }
    } else {
        // If current value is invalid, just clear field
        setText("");
    }
    setValid(true);
}

void BitcoinAmountField::setDisplayUnit(int newUnit) { unit->setValue(newUnit); }

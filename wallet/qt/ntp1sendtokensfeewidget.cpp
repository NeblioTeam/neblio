#include "ntp1sendtokensfeewidget.h"

#include "util.h"
#include <QDoubleValidator>

void NTP1SendTokensFeeWidget::createWidgets()
{
    mainLayout               = new QGridLayout;
    autoCalculateFeeCheckbox = new QCheckBox(this);
    feeAmountLineEdit        = new QLineEdit(this);

    mainLayout->addWidget(autoCalculateFeeCheckbox, 0, 0, 1, 1);
    mainLayout->addWidget(feeAmountLineEdit, 0, 1, 1, 1);

    feeAmountLineEdit->setPlaceholderText("Choose your fee in nebl, e.g., 0.0001");

    feeAmountLineEdit->setValidator(new QDoubleValidator(0, 100000000, 8));
    autoCalculateFeeCheckbox->setText("Automatically calculate the fee");
    setLayout(mainLayout);

    connect(autoCalculateFeeCheckbox, &QCheckBox::toggled, this,
            &NTP1SendTokensFeeWidget::slot_autoCalculateFeeStatusChanged);

    autoCalculateFeeCheckbox->setChecked(true);
}

NTP1SendTokensFeeWidget::NTP1SendTokensFeeWidget(QWidget* parent) : QWidget(parent) { createWidgets(); }

bool NTP1SendTokensFeeWidget::isAutoCalcFeeSelected() const
{
    return autoCalculateFeeCheckbox->isChecked();
}

std::string NTP1SendTokensFeeWidget::getEnteredFee() const
{
    return feeAmountLineEdit->text().toStdString();
}

void NTP1SendTokensFeeWidget::resetAllFields()
{
    autoCalculateFeeCheckbox->setChecked(true);
    feeAmountLineEdit->clear();
}

void NTP1SendTokensFeeWidget::slot_autoCalculateFeeStatusChanged(bool selected)
{
    feeAmountLineEdit->setEnabled(!selected);
}

#include "ntp1createmetadatadialog.h"

#include <QDesktopWidget>
#include <QMessageBox>

NTP1CreateMetadataDialog::NTP1CreateMetadataDialog(QWidget* parent) : QDialog(parent)
{
    mainLayout = new QGridLayout(this);
    setLayout(mainLayout);
    setWindowTitle("Edit NTP1 metadata");

    resize(QDesktopWidget().availableGeometry(this).size() * 0.5);

    editModeCheckbox    = new QCheckBox("Input custom json data", this);
    encryptDataCheckbox = new QCheckBox("Encrypt data (possible for only one recipient)", this);
    metadataPairsWidget = new NTP1MetadataPairsWidget(this);
    customDataWidget    = new NTP1CustomMetadataWidget(this);
    customDataWidget->setVisible(false);

    mainLayout->addWidget(editModeCheckbox, 0, 0, 1, 1);
    mainLayout->addWidget(encryptDataCheckbox, 1, 0, 1, 1);
    mainLayout->addWidget(metadataPairsWidget, 2, 0, 1, 1);
    mainLayout->addWidget(customDataWidget, 2, 0, 1, 1);

    connect(metadataPairsWidget, &NTP1MetadataPairsWidget::sig_okPressed, this, &QWidget::hide);
    connect(customDataWidget, &NTP1CustomMetadataWidget::sig_okPressed, this, &QWidget::hide);
    connect(this->editModeCheckbox, &QCheckBox::toggled, this,
            &NTP1CreateMetadataDialog::slot_customJsonDataSwitched);
}

json_spirit::Object NTP1CreateMetadataDialog::getJsonData() const
{
    try {
        if (editModeCheckbox->isChecked()) {
            return customDataWidget->getJsonObject();
        } else {
            return metadataPairsWidget->getJsonObject();
        }
    } catch (...) {
        QMessageBox::warning(
            this->customDataWidget, "Error retrieving json data",
            "An error happened while attempting to retrieve json data. This should not happen.");
        return json_spirit::Object({json_spirit::Pair("Error", "")});
    }
}

bool NTP1CreateMetadataDialog::encryptData() const { return encryptDataCheckbox->isChecked(); }

bool NTP1CreateMetadataDialog::jsonDataExists() const
{
    if (editModeCheckbox->isChecked()) {
        return !customDataWidget->isJsonEmpty();
    } else {
        return !metadataPairsWidget->isJsonEmpty();
    }
}

bool NTP1CreateMetadataDialog::jsonDataValid() const
{
    if (editModeCheckbox->isChecked()) {
        return !customDataWidget->isJsonEmpty() && customDataWidget->isJsonValid();
    } else {
        return !metadataPairsWidget->isJsonEmpty();
    }
}

void NTP1CreateMetadataDialog::clearData()
{
    customDataWidget->clearData();
    metadataPairsWidget->clearData();
    editModeCheckbox->setChecked(false);
}

void NTP1CreateMetadataDialog::slot_customJsonDataSwitched()
{
    metadataPairsWidget->setVisible(!editModeCheckbox->isChecked());
    customDataWidget->setVisible(editModeCheckbox->isChecked());
}

void NTP1CreateMetadataDialog::closeEvent(QCloseEvent* /*event*/)
{
    QMessageBox::information(this, "Changes saved", "Your changes (if any) have been saved");
    hide();
}

void NTP1CreateMetadataDialog::reject()
{
    QMessageBox::information(this, "Changes saved", "Your changes (if any) have been saved");
    hide();
}

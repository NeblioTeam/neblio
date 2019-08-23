#include "ntp1metadatapairwidget.h"

NTP1MetadataPairWidget::NTP1MetadataPairWidget(QWidget* parent) : QWidget(parent)
{
    mainLayout = new QGridLayout(this);

    this->setLayout(mainLayout);

    keyLineEdit = new QLineEdit(this);
    valLineEdit = new QLineEdit(this);
    closeButton = new QPushButton("", this);

    QIcon removeIcon;
    removeIcon.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    closeButton->setIcon(removeIcon);

    keyLineEdit->setPlaceholderText("Key");
    valLineEdit->setPlaceholderText("Value");

    mainLayout->addWidget(keyLineEdit, 0, 0, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(valLineEdit, 0, 1, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(closeButton, 0, 2, 1, 1, Qt::AlignTop);

    mainLayout->setMargin(0);

    QSizePolicy keySp = keyLineEdit->sizePolicy();
    keySp.setHorizontalStretch(1);
    keySp.setVerticalPolicy(QSizePolicy::Expanding);
    keyLineEdit->setSizePolicy(keySp);

    QSizePolicy valSp = valLineEdit->sizePolicy();
    valSp.setHorizontalStretch(1);
    valSp.setVerticalPolicy(QSizePolicy::Expanding);
    valLineEdit->setSizePolicy(valSp);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    connect(closeButton, &QPushButton::clicked, this, &NTP1MetadataPairWidget::slot_closeThis);
}

QPair<QString, QString> NTP1MetadataPairWidget::getKeyValuePair() const
{
    return qMakePair(keyLineEdit->text(), valLineEdit->text());
}

void NTP1MetadataPairWidget::clearData()
{
    keyLineEdit->setText("");
    valLineEdit->setText("");
}

void NTP1MetadataPairWidget::slot_hideClose() { closeButton->hide(); }

void NTP1MetadataPairWidget::slot_showClose() { closeButton->show(); }

bool NTP1MetadataPairWidget::isEmpty() const
{
    return keyLineEdit->text().trimmed().isEmpty() && valLineEdit->text().trimmed().isEmpty();
}

json_spirit::Object NTP1MetadataPairWidget::getAsJsonObject() const
{
    json_spirit::Object result;
    if (!keyLineEdit->text().trimmed().isEmpty()) {
        std::string key = keyLineEdit->text().toUtf8().toStdString();
        std::string val = valLineEdit->text().toUtf8().toStdString();
        result.push_back(json_spirit::Pair(key, val));
    }
    return result;
}

void NTP1MetadataPairWidget::slot_closeThis() { emit signal_closeThis(this); }

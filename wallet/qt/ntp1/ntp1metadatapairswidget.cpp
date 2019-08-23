#include "ntp1metadatapairswidget.h"

NTP1MetadataPairsWidget::NTP1MetadataPairsWidget(QWidget* parent) : QWidget(parent)
{
    mainLayout              = new QGridLayout(this);
    addFieldPairButton      = new QPushButton("Add value pair", this);
    metadataPairsScrollArea = new QScrollArea(this);
    metadataPairsLayout     = new QVBoxLayout(metadataPairsScrollArea);
    pairsWidgetsWidget      = new QWidget(this);

    okButton    = new QPushButton("OK", this);
    clearButton = new QPushButton("Clear all", this);

    metadataPairsScrollArea->setLayout(metadataPairsLayout);
    metadataPairsScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    metadataPairsScrollArea->setWidgetResizable(true);
    metadataPairsScrollArea->setFrameStyle(QFrame::NoFrame);
    metadataPairsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    metadataPairsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    pairsWidgetsWidget->setLayout(metadataPairsLayout);
    metadataPairsScrollArea->setWidget(pairsWidgetsWidget);

    pairsWidgetsWidget->setStyleSheet("background-color: white;font-family:'Open "
                                      "Sans,sans-serif';");

    mainLayout->addWidget(okButton, 0, 0, 1, 1);
    mainLayout->addWidget(clearButton, 0, 1, 1, 1);
    mainLayout->addWidget(addFieldPairButton, 0, 2, 1, 1);
    mainLayout->addWidget(metadataPairsScrollArea, 1, 0, 1, 3);

    connect(addFieldPairButton, &QPushButton::clicked, this,
            &NTP1MetadataPairsWidget::slot_addKeyValuePair);
    connect(clearButton, &QPushButton::clicked, this, &NTP1MetadataPairsWidget::slot_clearPressed);
    connect(okButton, &QPushButton::clicked, this, &NTP1MetadataPairsWidget::slot_okPressed);

    metadataPairsLayout->setContentsMargins(0, 0, 0, 0);
    metadataPairsLayout->setSpacing(0);
    metadataPairsLayout->setMargin(5);
    metadataPairsLayout->setAlignment(Qt::AlignTop);

    slot_addKeyValuePair();
}

void NTP1MetadataPairsWidget::clearData()
{
    while (metadataPairsWidgets.size() > 1) {
        slot_removePairWidget(metadataPairsWidgets.front());
    }
    Q_ASSERT(metadataPairsWidgets.size() == 1);
    metadataPairsWidgets.front()->clearData();
}

void NTP1MetadataPairsWidget::slot_addKeyValuePair()
{
    NTP1MetadataPairWidget* widget = new NTP1MetadataPairWidget;
    metadataPairsWidgets.push_back(widget);
    metadataPairsLayout->addWidget(widget, 0, Qt::AlignTop);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    connect(widget, &NTP1MetadataPairWidget::signal_closeThis, this,
            &NTP1MetadataPairsWidget::slot_removePairWidget);
    slot_actToShowOrHideCloseButtons();
}

void NTP1MetadataPairsWidget::slot_actToShowOrHideCloseButtons()
{
    if (metadataPairsWidgets.size() > 1) {
        showAllCloseButtons();
    } else {
        hideAllCloseButtons();
    }
}

void NTP1MetadataPairsWidget::showAllCloseButtons()
{
    for (NTP1MetadataPairWidget* w : metadataPairsWidgets) {
        w->slot_showClose();
    }
}

void NTP1MetadataPairsWidget::hideAllCloseButtons()
{
    for (NTP1MetadataPairWidget* w : metadataPairsWidgets) {
        w->slot_hideClose();
    }
}

void NTP1MetadataPairsWidget::slot_removePairWidget(QWidget* widget)
{
    if (metadataPairsWidgets.size() <= 1) {
        return;
    }
    metadataPairsLayout->removeWidget(widget);
    delete widget;
    auto it = std::find(metadataPairsWidgets.begin(), metadataPairsWidgets.end(), widget);
    if (it != metadataPairsWidgets.end()) {
        metadataPairsWidgets.erase(it);
    } else {
        printf("Unable to find the metadata pair widget to erase");
    }
    slot_actToShowOrHideCloseButtons();
}

void NTP1MetadataPairsWidget::slot_okPressed() { emit sig_okPressed(); }

void NTP1MetadataPairsWidget::slot_clearPressed() { clearData(); }

json_spirit::Object NTP1MetadataPairsWidget::getJsonObject() const
{
    // {"userData":{"meta":[{"Hi":"there"},{"I am":"here!"}]}}
    json_spirit::Object resultObj;
    json_spirit::Array  metaArray;
    for (const auto& w : metadataPairsWidgets) {
        json_spirit::Object obj = w->getAsJsonObject();
        if (!obj.empty()) {
            metaArray.push_back(obj);
        }
    }
    resultObj.push_back(json_spirit::Pair("meta", metaArray));
    json_spirit::Pair p("userData", resultObj);
    return json_spirit::Object({p});
}

bool NTP1MetadataPairsWidget::isJsonEmpty() const
{
    bool allEmpty = true;
    for (const auto& w : metadataPairsWidgets) {
        allEmpty = allEmpty && w->isEmpty();
    }
    return allEmpty;
}

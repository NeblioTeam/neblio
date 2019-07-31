#ifndef NTP1METADATAPAIRWIDGET_H
#define NTP1METADATAPAIRWIDGET_H

#include "json_spirit.h"
#include <QGridLayout>
#include <QLineEdit>
#include <QPair>
#include <QPushButton>
#include <QWidget>

class NTP1MetadataPairWidget : public QWidget
{
    Q_OBJECT

    QGridLayout* mainLayout;
    QLineEdit*   keyLineEdit;
    QLineEdit*   valLineEdit;
    QPushButton* closeButton;

public:
    NTP1MetadataPairWidget(QWidget* parent = Q_NULLPTR);

    QPair<QString, QString> getKeyValuePair() const;

    void clearData();

signals:
    void signal_closeThis(QWidget* theWidget);

public slots:
    void slot_closeThis();
    void slot_hideClose();
    void slot_showClose();
    bool isEmpty() const;

    json_spirit::Object getAsJsonObject() const;
};

#endif // NTP1METADATAPAIRWIDGET_H

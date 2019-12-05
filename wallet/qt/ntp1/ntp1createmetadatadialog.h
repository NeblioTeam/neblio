#ifndef NTP1CREATEMETADATADIALOG_H
#define NTP1CREATEMETADATADIALOG_H

#include "ntp1custommetadatawidget.h"
#include "ntp1metadatapairswidget.h"
#include <QCheckBox>
#include <QDialog>
#include <QGridLayout>
#include <QPushButton>

class NTP1CreateMetadataDialog : public QDialog
{
    Q_OBJECT

    QGridLayout* mainLayout;
    QCheckBox*   editModeCheckbox;
    QCheckBox*   encryptDataCheckbox;

    NTP1MetadataPairsWidget*  metadataPairsWidget;
    NTP1CustomMetadataWidget* customDataWidget;

public:
    NTP1CreateMetadataDialog(QWidget* parent = Q_NULLPTR);

    json_spirit::Object getJsonData() const;
    bool                encryptData() const;
    bool                jsonDataExists() const;
    bool                jsonDataValid() const;
    void                clearData();

public slots:
    void slot_customJsonDataSwitched();

    // QWidget interface
protected:
    // effect on clicking the X button of the window
    void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;

    // QDialog interface
public slots:
    // effect on Esc keyboard press
    void reject() Q_DECL_OVERRIDE;
};

#endif // NTP1CREATEMETADATADIALOG_H

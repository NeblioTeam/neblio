#ifndef NTP1SENDDIALOG_H
#define NTP1SENDDIALOG_H

#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include "ntp1/ntp1sendtokensdata.h"
#include "ntp1/ntp1tokenlistmodel.h"
#include "ntp1sendtokensfeewidget.h"

class NTP1SendSingleTokenFields;
class NTP1TokenListModel;

class NTP1SendDialog Q_DECL_FINAL : public QWidget
{
    Q_OBJECT
    QGridLayout* mainLayout;
    QPushButton* addRecipientButton;
    QPushButton* sendButton;

    QVBoxLayout*                        recipientWidgetsLayout;
    QWidget*                            recipientWidgetsWidget;
    QVector<NTP1SendSingleTokenFields*> recipientWidgets;
    QScrollArea*                        recipientWidgetsScrollArea;
    NTP1TokenListModel*                 tokenListDataModel;
    NTP1SendTokensFeeWidget*            feeWidget;

public:
    explicit NTP1SendDialog(NTP1TokenListModel* tokenListModel, QWidget* parent = Q_NULLPTR);
    ~NTP1SendDialog();
    void                          createWidgets();
    boost::shared_ptr<NTP1Wallet> getLatestNTP1Wallet() const;
    NTP1SendTokensData            createTransactionData() const;

public slots:
    void slot_addRecipientWidget();
    void slot_removeRecipientWidget(QWidget* widget);
    void slot_actToShowOrHideCloseButtons();
    void slot_updateAllRecipientDialogsTokens();
    void slot_sendClicked();
    void resetAllFields();

protected:
    void showAllCloseButtons();
    void hideAllCloseButtons();

signals:
    void signal_updateAllRecipientDialogsTokens();
};

#endif // NTP1SENDDIALOG_H

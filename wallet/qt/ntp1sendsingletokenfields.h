#ifndef NTP1SENDSINGLETOKENFIELDS_H
#define NTP1SENDSINGLETOKENFIELDS_H

#include <QComboBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

#include "ntp1/ntp1listelementtokendata.h"
#include "ntp1/ntp1sendtokensonerecipientdata.h"
#include "ntp1senddialog.h"

class NTP1SendSingleTokenFields Q_DECL_FINAL : public QWidget
{
    Q_OBJECT

    QGridLayout*                                       mainLayout;
    QLineEdit*                                         amount;
    QLineEdit*                                         destination;
    QComboBox*                                         tokenTypeComboBox;
    QPushButton*                                       closeButton;
    void                                               createWidgets();
    std::function<boost::shared_ptr<NTP1Wallet>(void)> retrieveLatestWallet;
    std::deque<NTP1ListElementTokenData>               currentTokens;
    void                                               fillCurrentTokens();

public:
    explicit NTP1SendSingleTokenFields(
        std::function<boost::shared_ptr<NTP1Wallet>(void)> WalletRetriever, QWidget* parent = Q_NULLPTR);
    virtual ~NTP1SendSingleTokenFields();
    NTP1SendTokensOneRecipientData createRecipientData() const;
    void                           resetAllFields();

signals:
    void signal_closeThis(QWidget* theWidget);

public slots:
    void slot_closeThis();
    void slot_hideClose();
    void slot_showClose();
    void slot_updateTokenList();
};

#endif // NTP1SENDSINGLETOKENFIELDS_H

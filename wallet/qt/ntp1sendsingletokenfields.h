#ifndef NTP1SENDSINGLETOKENFIELDS_H
#define NTP1SENDSINGLETOKENFIELDS_H

#include <QComboBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <functional>

#include "ntp1senddialog.h"

struct TokenData
{
    QString name;
    QString tokenId;
    qint64  amount;
    QIcon   icon;
    void    fill(int index, boost::shared_ptr<NTP1Wallet> wallet)
    {
        name = NTP1TokenListModel::__getTokenName(index, wallet);
        icon = NTP1TokenListModel::__getTokenIcon(index, wallet);
        if (icon.isNull()) {
            icon = QIcon(":/images/orion");
        }
        tokenId = NTP1TokenListModel::__getTokenId(index, wallet);
        amount  = FromString<qint64>(NTP1TokenListModel::__getTokenBalance(index, wallet).toStdString());
    }
};

class NTP1SendSingleTokenFields Q_DECL_FINAL : public QWidget
{
    Q_OBJECT

    QGridLayout*                                       mainLayout;
    QLineEdit*                                         amount;
    QLineEdit*                                         destination;
    QComboBox*                                         tokenType;
    QPushButton*                                       closeButton;
    void                                               createWidgets();
    std::function<boost::shared_ptr<NTP1Wallet>(void)> retrieveLatestWallet;
    std::deque<TokenData>                              currentTokens;
    void                                               fillCurrentTokens();

public:
    explicit NTP1SendSingleTokenFields(
        std::function<boost::shared_ptr<NTP1Wallet>(void)> WalletRetriever, QWidget* parent = Q_NULLPTR);
    virtual ~NTP1SendSingleTokenFields();

signals:
    void signal_closeThis(QWidget* theWidget);

public slots:
    void slot_closeThis();
    void slot_hideClose();
    void slot_showClose();
    void slot_updateTokenList();
};

#endif // NTP1SENDSINGLETOKENFIELDS_H

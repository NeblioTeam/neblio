#ifndef BITCOINFIELD_H
#define BITCOINFIELD_H

#include "qt/ntp1/ntp1listelementtokendata.h"
#include "qt/ntp1/ntp1tokenlistmodel.h"
#include <QComboBox>
#include <QToolButton>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QValueComboBox;
QT_END_NAMESPACE

/** Widget for entering bitcoin amounts.
 */
class BitcoinAmountField : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qint64 value READ value WRITE setValue NOTIFY textChanged USER true)
public:
    explicit BitcoinAmountField(bool EnableNTP1Tokens, QWidget* parent);

    qint64  value(bool* valid = 0) const;
    void    setValue(qint64 value);
    QString getSelectedTokenId() const;

    bool isNTP1TokenSelected() const;

    /** Mark current value as invalid in UI. */
    void setValid(bool valid);
    /** Perform input validation, mark field as invalid if entered value is not valid. */
    bool validate();

    /** Change unit used to display amount. */
    void setDisplayUnit(int unit);

    /** Make field empty and ready for new input. */
    void clear();

    /** Qt messes up the tab chain by default in some cases (issue
       https://bugreports.qt-project.org/browse/QTBUG-10907), in these cases we have to set it up
       manually.
    */
    QWidget* setupTabChain(QWidget* prev);

signals:
    void textChanged();

public slots:
    void slot_updateTokensList();
    void slot_tokenChanged();

protected:
    /** Intercept focus-in event and ',' key presses */
    bool eventFilter(QObject* object, QEvent* event);
    void focusInEvent(QFocusEvent*);

private:
    QDoubleSpinBox* amount;
    QValueComboBox* unit;
    QComboBox*      tokenKindsComboBox;
    QToolButton*    refreshTokensButton;
    int             currentUnit;

    NTP1TokenListModel*                  tokenListDataModel;
    std::deque<NTP1ListElementTokenData> tokenKindsList;

    void    setText(const QString& text);
    QString text() const;

    bool enableNTP1Tokens = false;

private slots:
    void unitChanged(int idx);
};

#endif // BITCOINFIELD_H

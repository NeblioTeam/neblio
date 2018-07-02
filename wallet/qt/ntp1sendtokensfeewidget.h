#ifndef NTP1SENDTOKENSFEEWIDGET_H
#define NTP1SENDTOKENSFEEWIDGET_H

#include <QCheckBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QWidget>

class NTP1SendTokensFeeWidget : public QWidget
{
    Q_OBJECT

    QGridLayout* mainLayout;
    QCheckBox*   autoCalculateFeeCheckbox;
    QLineEdit*   feeAmountLineEdit;

    void createWidgets();

public:
    explicit NTP1SendTokensFeeWidget(QWidget* parent = nullptr);
    bool        isAutoCalcFeeSelected() const;
    std::string getEnteredFee() const;
    void        resetAllFields();

signals:

public slots:
    void slot_autoCalculateFeeStatusChanged(bool selected);
};

#endif // NTP1SENDTOKENSFEEWIDGET_H

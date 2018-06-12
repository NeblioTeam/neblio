#ifndef NTP1SENDSINGLETOKENFIELDS_H
#define NTP1SENDSINGLETOKENFIELDS_H

#include <QComboBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>

class NTP1SendSingleTokenFields final : public QWidget
{
    Q_OBJECT

    QGridLayout* mainLayout;
    QLineEdit*   amount;
    QLineEdit*   destination;
    QComboBox*   tokenType;
    QPushButton* closeButton;
    void         createWidgets();

public:
    explicit NTP1SendSingleTokenFields(QWidget* parent = Q_NULLPTR);
    virtual ~NTP1SendSingleTokenFields();

signals:
    void signal_closeThis(QWidget* theWidget);

public slots:
    void slot_closeThis();
    void slot_hideClose();
    void slot_showClose();
};

#endif // NTP1SENDSINGLETOKENFIELDS_H

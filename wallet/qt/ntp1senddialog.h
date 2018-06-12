#ifndef NTP1SENDDIALOG_H
#define NTP1SENDDIALOG_H

#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class NTP1SendSingleTokenFields;

class NTP1SendDialog : public QWidget
{
    Q_OBJECT
    QGridLayout* mainLayout;
    QPushButton* addRecipientButton;
    QPushButton* sendButton;

    QVBoxLayout*                        recipientWidgetsLayout;
    QWidget*                            recipientWidgetsWidget;
    QVector<NTP1SendSingleTokenFields*> recipientWidgets;
    QScrollArea*                        recipientWidgetsScrollArea;

public:
    explicit NTP1SendDialog(QWidget* parent = Q_NULLPTR);
    ~NTP1SendDialog();
    void createWidgets();

public slots:
    void slot_addRecipientWidget();
    void slot_removeRecipientWidget(QWidget* widget);
    void slot_actToShowOrHideCloseButtons();

protected:
    void showAllCloseButtons();
    void hideAllCloseButtons();
};

#endif // NTP1SENDDIALOG_H

#ifndef MESSAGEBOXWITHTIMER_H
#define MESSAGEBOXWITHTIMER_H

#include <QMessageBox>
#include <QTimer>
#include <QPushButton>
#include <QVector>


/**
 * @brief The MessageBoxWithTimer class
 * This class is like QMessageBox, but provides the option to have
 * a button disabled for some time and then enabled,
 * to prevent the user from rash clicking
 * Any number of buttons can be disabled.
 * To use this:
 * Instantiate an object in your QWidget derivative and make it parent
 * use addButton() to add buttons to your message box
 * use addButtonToWaitOn() to add buttons that should be disabled for some time
 * use exec() instead of show() or open()
 */
class MessageBoxWithTimer : public QMessageBox
{
    QVector<QPushButton*> buttonsToEnable;
    QVector<QString> originalButtonLabels;
    long disablePeriod;
    bool isDialogTimerRunning;
    void messageShown();

public:
    MessageBoxWithTimer(QWidget *parent = 0);

    QMessageBox *NTPWarning_messageBox;
    QTimer* buttonsEnableTimer;
    QTimer* buttonLabelUpdateTimer;
    void addButtonToWaitOn(QPushButton *button);
    void setDisablePeriod(long period_in_ms = 10000);
    void clearButtonsToEnable();
    /**
     * @brief exec
     * This is the alternative to the normal QWidget::show() in this class
     * which is supposed to activate the disabled buttons
     */
    int exec() Q_DECL_OVERRIDE;
private slots:
    void enableDisabledButtons();
    void updateButtonsLabels();
};

#endif // MESSAGEBOXWITHTIMER_H

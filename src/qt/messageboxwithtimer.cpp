#include "messageboxwithtimer.h"

MessageBoxWithTimer::MessageBoxWithTimer(QWidget *parent) : QMessageBox(parent)
{
    buttonsEnableTimer     = new QTimer(this);
    buttonLabelUpdateTimer = new QTimer(this);
    isDialogTimerRunning = false;
    setDisablePeriod();

    connect(buttonsEnableTimer, &QTimer::timeout,
            this, &MessageBoxWithTimer::enableDisabledButtons);
    connect(buttonLabelUpdateTimer, &QTimer::timeout,
            this, &MessageBoxWithTimer::updateButtonsLabels);
}

void MessageBoxWithTimer::addButtonToWaitOn(QPushButton *button)
{
    buttonsToEnable.push_back(button);
}

void MessageBoxWithTimer::setDisablePeriod(long period_in_ms)
{
    disablePeriod = period_in_ms;
}

void MessageBoxWithTimer::clearButtonsToEnable()
{
    buttonsToEnable.clear();
}

int MessageBoxWithTimer::exec()
{
    messageShown();
    return QMessageBox::exec();
}

void MessageBoxWithTimer::enableDisabledButtons()
{
    for(long i = 0; i < buttonsToEnable.size(); i++) {
        buttonsToEnable[i]->setEnabled(true);
    }
    isDialogTimerRunning = false;
    buttonsEnableTimer->stop();
}

void MessageBoxWithTimer::updateButtonsLabels()
{
    // protect from the case if this is called before initializing labels
    if(buttonsToEnable.size() != originalButtonLabels.size()) {
        return;
    }

    long remainingTime = buttonsEnableTimer->remainingTime();
    QString timerText = QString::number(remainingTime/1000 + 1);
    if(buttonsEnableTimer->isActive()) {
        for(long i = 0; i < buttonsToEnable.size(); i++) {
            buttonsToEnable[i]->setText(originalButtonLabels[i] + " (" + timerText + ")");
        }
    } else {
        for(long i = 0; i < buttonsToEnable.size(); i++) {
            buttonsToEnable[i]->setText(originalButtonLabels[i]);
        }

        // stop the timer
        buttonLabelUpdateTimer->stop();
    }
}

void MessageBoxWithTimer::messageShown()
{
    // avoid double labeling with timer period
    if(!isDialogTimerRunning) {
        isDialogTimerRunning = true;
        originalButtonLabels.resize(buttonsToEnable.size());
        for(long i = 0; i < buttonsToEnable.size(); i++) {
            originalButtonLabels[i] = buttonsToEnable[i]->text();
        }
    }

    // disable buttons to be enabled after the timer
    for(long i = 0; i < buttonsToEnable.size(); i++) {
        buttonsToEnable[i]->setEnabled(false);
    }
    buttonsEnableTimer->stop();
    buttonLabelUpdateTimer->stop();
    buttonsEnableTimer->start(disablePeriod);
    buttonLabelUpdateTimer->start(100); // labels will be updated this often
}

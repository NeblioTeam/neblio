#include "messagebox.h"

#include <QThread>
#include <QSharedPointer>
#include <QMessageBox>

namespace ledger
{
    MessageBox::MessageBox(QWidget *parent, QSharedPointer<QObject> worker) : worker_(worker), msgBox_(parent)
    {
        worker_->moveToThread(&thread_);
        connect(&thread_, SIGNAL(finished()), &thread_, SLOT(deleteLater()));
        thread_.start();

        msgBox_.setIcon(QMessageBox::Icon::Information);
        msgBox_.setWindowTitle(parent->windowTitle());
        msgBox_.setText("Please confirm or cancel the action on your Ledger device.");
        msgBox_.setStandardButtons(QMessageBox::StandardButton::NoButton);
    }

    void MessageBox::exec()
    {
        msgBox_.exec();
        thread_.wait(); // to make sure that the thread is finished
    }

    void MessageBox::quit()
    {
        msgBox_.accept();
        thread_.quit();
    }
}

#ifndef __LEDGER_MESSAGEBOX
#define __LEDGER_MESSAGEBOX 1

#include <QObject>
#include <QWidget>
#include <QThread>
#include <QSharedPointer>
#include <QMessageBox>

namespace ledger
{
    class MessageBox : public QObject
    {
    Q_OBJECT

    public:
        MessageBox(QWidget *parent, QSharedPointer<QObject> worker);
        void exec();

    public slots:
        void quit();

    private:
        QThread thread_;
        QSharedPointer<QObject> worker_;
        QMessageBox msgBox_;
    };
}

#endif

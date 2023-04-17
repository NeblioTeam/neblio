#ifndef LEDGER_UI_LEDGERMESSAGEBOX_H
#define LEDGER_UI_LEDGERMESSAGEBOX_H

#include <QObject>
#include <QWidget>
#include <QThread>
#include <QSharedPointer>
#include <QMessageBox>

namespace ledger_ui
{
    class LedgerMessageBox : public QObject
    {
    Q_OBJECT

    public:
        LedgerMessageBox(QWidget *parent, QSharedPointer<QObject> worker, const QString &text = QString());
        void exec();

    public slots:
        void quit();

    private:
        QThread thread_;
        QSharedPointer<QObject> worker_;
        QMessageBox msgBox_;
    };
}

#endif // LEDGER_UI_LEDGERMESSAGEBOX_H

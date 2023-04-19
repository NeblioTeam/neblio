#ifndef LEDGER_UI_LEDGERMESSAGEBOX_H
#define LEDGER_UI_LEDGERMESSAGEBOX_H

#include <QMessageBox>
#include <QObject>
#include <QSharedPointer>
#include <QThread>
#include <QWidget>

namespace ledger_ui {
class LedgerMessageBox : public QObject
{
    Q_OBJECT

public:
    LedgerMessageBox(QWidget* parent, QSharedPointer<QObject> worker, const QString& text = QString());
    void exec();

public slots:
    void quit();

private:
    QThread                 thread_;
    QSharedPointer<QObject> worker_;
    QMessageBox             msgBox_;
};
} // namespace ledger_ui

#endif // LEDGER_UI_LEDGERMESSAGEBOX_H

#include "ledger_ui/ledgeruiutils.h"

#include <QObject>
#include <QString>

namespace ledger_ui {
QString GetQtErrorMessage(const ledger::LedgerException& e)
{
    return QObject::
        tr("A Ledger error occured: %1\n\nIf you did not cancel the operation intentionally, make sure "
           "that your device is connected to the computer and the Neblio app is opened on the device.")
            .arg(QString::fromStdString(e.GetMessage()));
}
} // namespace ledger_ui

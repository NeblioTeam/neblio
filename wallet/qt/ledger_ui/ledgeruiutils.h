#ifndef LEDGER_UI_LEDGERUIUTILS_H
#define LEDGER_UI_LEDGERUIUTILS_H

#include <string>
#include <QString>

#include "ledger/error.h"

namespace ledger_ui {
    QString GetQtErrorMessage(const ledger::LedgerException &e);
}

#endif // LEDGER_UI_LEDGERUIUTILS_H

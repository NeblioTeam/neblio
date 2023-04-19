#ifndef LEDGER_UI_LEDGERUIUTILS_H
#define LEDGER_UI_LEDGERUIUTILS_H

#include <QString>
#include <string>

#include "ledger/error.h"

namespace ledger_ui {
QString GetQtErrorMessage(const ledger::LedgerException& e);
}

#endif // LEDGER_UI_LEDGERUIUTILS_H

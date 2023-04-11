#pragma once

#include <string>
#include <QString>

#include "ledger/error.h"

namespace ledger_ui {
    QString GetQtErrorMessage(const ledger::LedgerException &e);
}

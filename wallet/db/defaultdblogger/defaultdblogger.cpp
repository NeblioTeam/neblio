#include "defaultdblogger.h"

#include "logging/logger.h"

void DefaultDBLogger::logWrite(const StringViewT message)
{
    NLog.write(b_sev::info, "{}", message.to_string());
}

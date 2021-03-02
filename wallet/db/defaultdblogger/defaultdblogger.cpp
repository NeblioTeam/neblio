#include "defaultdblogger.h"

#include "util.h"

void DefaultDBLogger::logWrite(const StringViewT message)
{
    OutputDebugStringF("%s\n", message.to_string().c_str());
}

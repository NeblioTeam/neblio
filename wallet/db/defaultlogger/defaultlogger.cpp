#include "defaultlogger.h"

#include "util.h"

void DefaultLogger::logWrite(const StringViewT message)
{
    OutputDebugStringF("%s\n", message.to_string().c_str());
}

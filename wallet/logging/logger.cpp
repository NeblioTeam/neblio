#include "logger.h"

boost::filesystem::path GetLoggingDir(const boost::filesystem::path& datadir)
{
    return datadir / "logs";
}

boost::filesystem::path GetLogFileName() { return "debug.log"; }

boost::filesystem::path GetLogFileFullPath(const boost::filesystem::path& datadir)
{
    return GetLoggingDir(datadir) / GetLogFileName();
}

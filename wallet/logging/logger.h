#ifndef LOGGER_H
#define LOGGER_H

#include "defaultlogger.h"

#include <boost/filesystem/path.hpp>

boost::filesystem::path GetLoggingDir(const boost::filesystem::path& datadir);
boost::filesystem::path GetLogFileName();
boost::filesystem::path GetLogFileFullPath(const boost::filesystem::path& datadir);

#endif // LOGGER_H

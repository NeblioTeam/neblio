#ifndef LOGGER_INCLUDES_H
#define LOGGER_INCLUDES_H

_Pragma(NEBLIO_DIAGNOSTIC_PUSH);
_Pragma(NEBLIO_HIDE_SHADOW_WARNING);

#include "spdlog/async.h"
#include "spdlog/async_logger.h"
#include "spdlog/details/thread_pool.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

_Pragma(NEBLIO_DIAGNOSTIC_POP);

#endif // LOGGER_INCLUDES_H

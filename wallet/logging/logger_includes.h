#ifndef LOGGER_INCLUDES_H
#define LOGGER_INCLUDES_H

#if !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#include "spdlog/async.h"
#include "spdlog/async_logger.h"
#include "spdlog/details/thread_pool.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // LOGGER_INCLUDES_H

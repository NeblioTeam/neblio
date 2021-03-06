#ifndef DEFAULTLOGGER_H
#define DEFAULTLOGGER_H

#include <boost/optional.hpp>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

// without this, it won't compile
#define SPDLOG_DISABLE_DEFAULT_LOGGER

#include "logger_includes.h"

#ifndef FUNCTIONSIG
#if defined(__GNUC__)
#define FUNCTIONSIG __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define FUNCTIONSIG __FUNCSIG__
#else
#define FUNCTIONSIG __func__
#endif
#endif

#define LOG_PRE                                                                                         \
    "[" + boost::filesystem::path(__FILE__).filename().string() + ":" + std::to_string(__LINE__) +      \
        "] [" + std::string(FUNCTIONSIG) + "]"

//#define NLog LoggerSingleton::get()
#define NLog LogSourceForwarder(std::string(LOG_PRE))

using b_sev = spdlog::level::level_enum;

class DefaultLogger
{
    std::shared_ptr<spdlog::sinks::dist_sink_mt> dist_sink =
        std::make_shared<spdlog::sinks::dist_sink_mt>();
    std::shared_ptr<spdlog::details::thread_pool> tp =
        std::make_shared<spdlog::details::thread_pool>(4096, std::thread::hardware_concurrency());
    std::shared_ptr<spdlog::async_logger> logger =
        std::make_shared<spdlog::async_logger>("", dist_sink, tp);

public:
    DefaultLogger()
    {
        spdlog::register_logger(logger);
        spdlog::flush_every(std::chrono::seconds(5));
    }

    static std::string severity_as_string(b_sev severity)
    {
        switch (severity) {
        case b_sev::trace:
            return "Trace";
        case b_sev::debug:
            return "Debug";
        case b_sev::info:
            return "Info";
        case b_sev::warn:
            return "Warning";
        case b_sev::err:
            return "Error";
        default:
            return "Unknown";
        }
    }

    bool add_file(const std::string& filename, const spdlog::level::level_enum& minimum_severity)
    {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, false);
            file_sink->set_level(minimum_severity);
            dist_sink->add_sink(file_sink);
            return true;
        } catch (const std::exception& ex) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
            return false;
        }
    }

    bool add_rotating_file(const std::string& filename, std::size_t max_size, std::size_t max_files,
                           bool rotate_name, const spdlog::level::level_enum& minimum_severity)
    {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                filename, max_size, max_files, rotate_name);
            file_sink->set_level(minimum_severity);
            dist_sink->add_sink(file_sink);
            return true;
        } catch (const std::exception& ex) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
            return false;
        }
    }

    template <typename FormatString, typename... Args>
    void write(b_sev severity, const FormatString& fmt, Args&&... args)
    {
        logger->log(severity, fmt, std::forward<Args>(args)...);
    }

    template <typename FormatString, typename... Args>
    bool error(const FormatString& fmt, Args&&... args)
    {
        logger->log(b_sev::err, fmt, std::forward<Args>(args)...);
        return false;
    }

    template <typename FormatString, typename... Args>
    bool critical(const FormatString& fmt, Args&&... args)
    {
        logger->log(b_sev::critical, fmt, std::forward<Args>(args)...);
        return false;
    }

    template <typename T>
    bool add_stream(std::shared_ptr<T> sink, const b_sev& minimum_severity)
    {
        try {
            sink->set_level(minimum_severity);
            dist_sink->add_sink(sink);
            return true;
        } catch (std::exception& ex) {
            std::cerr << "Failed to add sink" << std::endl;
            return false;
        }
    }

    void set_level(const b_sev& minimum_severity) { dist_sink->set_level(minimum_severity); }

    void flush() { logger->flush(); }

    spdlog::async_logger* getInternalLogger() { return logger.get(); }
};

class LoggerSingleton
{
public:
    static DefaultLogger& get()
    {
        static DefaultLogger logger;
        return logger;
    }
};

/**
 * @brief The LogSourceForwarder class
 * This class wraps the singleton and adds to it the source of the log information
 */
class LogSourceForwarder
{
    std::string sourceInfo;

public:
    LogSourceForwarder(const std::string& SourceInfo) : sourceInfo(SourceInfo) {}

    LogSourceForwarder(std::string&& SourceInfo) : sourceInfo(std::move(SourceInfo)) {}

    bool add_rotating_file(const std::string& filename, std::size_t max_size, std::size_t max_files,
                           bool rotate_name, const spdlog::level::level_enum& minimum_severity)
    {
        return LoggerSingleton::get().add_rotating_file(filename, max_size, max_files, rotate_name,
                                                        minimum_severity);
    }

    template <typename T>
    bool add_stream(std::shared_ptr<T> sink, const b_sev& minimum_severity)
    {
        return LoggerSingleton::get().add_stream(sink, minimum_severity);
    }

    void set_level(const b_sev& minimum_severity) { LoggerSingleton::get().set_level(minimum_severity); }

    template <typename FormatString, typename... Args>
    void write(b_sev severity, const FormatString& fmtStr, Args&&... args)
    {
        LoggerSingleton::get().write(severity, "{}: {}", std::move(sourceInfo),
                                     fmt::format(fmtStr, std::forward<Args>(args)...));
    }

    template <typename FormatString, typename... Args>
    bool error(const FormatString& fmtStr, Args&&... args)
    {
        LoggerSingleton::get().write(b_sev::err, "{}: {}", std::move(sourceInfo),
                                     fmt::format(fmtStr, std::forward<Args>(args)...));
        return false;
    }

    template <typename FormatString, typename... Args>
    bool critical(const FormatString& fmtStr, Args&&... args)
    {
        LoggerSingleton::get().write(b_sev::critical, "{}: {}", std::move(sourceInfo),
                                     fmt::format(fmtStr, std::forward<Args>(args)...));
        return false;
    }

    template <typename FormatString, typename... Args>
    boost::none_t errorn(const FormatString& fmtStr, Args&&... args)
    {
        LoggerSingleton::get().write(b_sev::err, "{}: {}", std::move(sourceInfo),
                                     fmt::format(fmtStr, std::forward<Args>(args)...));
        return boost::none;
    }

    void flush() { LoggerSingleton::get().flush(); }
};

#endif // DEFAULTLOGGER_H

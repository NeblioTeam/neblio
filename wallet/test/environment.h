#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "gtest/gtest.h"

#include "logging/logger.h"

#include "chainparams.h"
#include "logging/logger.h"
#include "stringmanip.h"
#include <boost/core/ignore_unused.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

static std::once_flag createDirOnce;

class Environment : public ::testing::Environment
{
public:
    virtual ~Environment() = default;
    virtual void SetUp()
    {
        // ::testing::GTEST_FLAG(catch_exceptions) = false;
        srand(time(nullptr));
        SelectParams(NetworkType::Mainnet);
        InitLogging();
        std::call_once(createDirOnce, []() {
            boost::filesystem::path dir = GetTestsDataDir();
            if (!boost::filesystem::is_directory(dir)) {
                boost::filesystem::create_directories(dir);
            }
            std::clog << "Tests data dir: " << PossiblyWideStringToString(dir.native()) << std::endl;
        });
    }

    static boost::filesystem::path GetTestsDataDir()
    {
        return boost::filesystem::temp_directory_path() / "neblio_tests";
    }

    static void InitLogging()
    {

        const boost::filesystem::path LogFilesDir = GetTestsDataDir() / "logs";
        const boost::filesystem::path LogFileName = "neblio_tests.log";
        const boost::filesystem::path LogFilePath = LogFilesDir / LogFileName;

        const bool rotateFile = false;

        // max rotated files
        const int64_t maxFiles = 50;

        // max file size
        const int64_t maxSize = 1 << 30;

        const b_sev minSeverity = b_sev::trace;

        NLog.set_level(minSeverity);

        const bool logFileAddingResult =
            NLog.add_rotating_file(PossiblyWideStringToString(LogFilePath.native()), maxSize, maxFiles,
                                   rotateFile, minSeverity);
        if (!logFileAddingResult) {
            throw std::runtime_error("Failed to open log file for writing: " +
                                     PossiblyWideStringToString(LogFilePath.native()));
        }

        NLog.write(b_sev::info, "\n\n\n\n\n\n\n\n\n\n---------------------------------");

        NLog.write(b_sev::info, "Initialized logging for unit tests successfully!");
    }

    virtual void TearDown() {}
};

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new Environment);

/**
 * @brief The SwitchNetworkTemporarily class
 * In tests, we need a mechanism to switch the network parameters back and forth without altering the
 * global state after the test. This class uses RAII to achieve that. Just construct an instance of this
 * class and hold it until the end of the test, and it'll switch back to the original state
 */
struct SwitchNetworkTypeTemporarily
{
    NetworkType prevNetType;
    SwitchNetworkTypeTemporarily(NetworkType NewNetworkType) : prevNetType(Params().NetType())
    {
        SelectParams(NewNetworkType);
    }
    ~SwitchNetworkTypeTemporarily() { SelectParams(prevNetType); }
};

template <typename T>
void ignore_it(T)
{
    boost::ignore_unused(env);
}

#endif // ENVIRONMENT_H

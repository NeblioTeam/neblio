#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "gtest/gtest.h"

#include "chainparams.h"
#include <boost/core/ignore_unused.hpp>
#include <fstream>
#include <sstream>
#include <thread>

class Environment : public ::testing::Environment
{
public:
    virtual ~Environment() = default;
    virtual void SetUp()
    {
        // ::testing::GTEST_FLAG(catch_exceptions) = false;
        srand(time(nullptr));
        SelectParams(NetworkType::Mainnet);
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

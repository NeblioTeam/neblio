#ifndef SERIALIZATIONTESTER_H
#define SERIALIZATIONTESTER_H

#include "addrman.h"
#include "alert.h"
#include "bloom.h"
#include "checkpoints.h"
#include "crypter.h"
#include "main.h"
#include "ntp1/ntp1outpoint.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1tokentxdata.h"
#include "ntp1/ntp1transaction.h"
#include "ntp1/ntp1txin.h"
#include "ntp1/ntp1txout.h"
#include "protocol.h"
#include "wallet.h"
#include "zerocoin/Accumulator.h"
#include "zerocoin/AccumulatorProofOfKnowledge.h"

#include <type_traits>

#if BOOST_VERSION >= 106100
#include <boost/utility/string_view.hpp>
using StringViewT = boost::string_view;
#else
#include <boost/utility/string_ref.hpp>
using StringViewT = boost::string_ref;
#endif

template <typename T>
typename std::enable_if<!std::is_same<T, std::string>::value && !std::is_same<T, const char*>::value,
                        void>::type
TEST_EQUALITY(T a, T b, unsigned line)
{
#ifdef EXPECT_EQ
    EXPECT_EQ(a, b) << " failure at line number: " << line;
#else
    if (a != b) {
        std::stringstream ss;
        ss << "Binary format check failed for pair \"" << a << "\" and \"" << b << "\" from line "
           << line;
        throw std::runtime_error(ss.str());
    }
#endif
}

template <typename T>
typename std::enable_if<std::is_same<T, std::string>::value || std::is_same<T, const char*>::value,
                        void>::type
TEST_EQUALITY(const T& a, StringViewT b, unsigned line)
{
#ifdef EXPECT_EQ
    EXPECT_EQ(a, b) << " failure at line number: " << line;
#else
    if (a != b) {
        std::stringstream ss;
        ss << "Binary format check failed for pair \"" << a << "\" and \"" << b << "\" from line "
           << line;
        throw std::runtime_error(ss.str());
    }
#endif
}

void RunCrossPlatformTests();

#endif // SERIALIZATIONTESTER_H

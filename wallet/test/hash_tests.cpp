// Copyright (c) 2013-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "key.h"
#include "base58.h"
#include "main.h"

#include <vector>

#include "googletest/googletest/include/gtest/gtest.h"

TEST(hash_tests, murmurhash3)
{

#define T(expected, seed, data) EXPECT_EQ(MurmurHash3(seed, ParseHex(data)), expected)

    // Test MurmurHash3 with various inputs. Of course this is retested in the
    // bloom filter tests - they would fail if MurmurHash3() had any problems -
    // but is useful for those trying to implement Bitcoin libraries as a
    // source of test data for their MurmurHash3() primitive during
    // development.
    //
    // The magic number 0xFBA4C795 comes from CBloomFilter::Hash()

    T(0x00000000U, 0x00000000, "");
    T(0x6a396f08U, 0xFBA4C795, "");
    T(0x81f16f39U, 0xffffffff, "");

    T(0x514e28b7U, 0x00000000, "00");
    T(0xea3f0b17U, 0xFBA4C795, "00");
    T(0xfd6cf10dU, 0x00000000, "ff");

    T(0x16c6b7abU, 0x00000000, "0011");
    T(0x8eb51c3dU, 0x00000000, "001122");
    T(0xb4471bf8U, 0x00000000, "00112233");
    T(0xe2301fa8U, 0x00000000, "0011223344");
    T(0xfc2e4a15U, 0x00000000, "001122334455");
    T(0xb074502cU, 0x00000000, "00112233445566");
    T(0x8034d2a0U, 0x00000000, "0011223344556677");
    T(0xb4698defU, 0x00000000, "001122334455667788");

#undef T
}

#include "googletest/googletest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "main.h"

// amounts 0.00000001 .. 0.00100000
#define NUM_MULTIPLES_UNIT 100000

// amounts 0.01 .. 100.00
#define NUM_MULTIPLES_CENT 10000

// amounts 1 .. 10000
#define NUM_MULTIPLES_1BTC 10000

// amounts 50 .. 21000000
#define NUM_MULTIPLES_50BTC 420000

using namespace std;

bool static TestEncode(uint64_t in) {
    return in == CTxOutCompressor::DecompressAmount(CTxOutCompressor::CompressAmount(in));
}

bool static TestDecode(uint64_t in) {
    return in == CTxOutCompressor::CompressAmount(CTxOutCompressor::DecompressAmount(in));
}

bool static TestPair(uint64_t dec, uint64_t enc) {
    return CTxOutCompressor::CompressAmount(dec) == enc &&
           CTxOutCompressor::DecompressAmount(enc) == dec;
}

TEST(compress_tests, compress_amounts)
{
    EXPECT_TRUE(TestPair(            0,       0x0));
    EXPECT_TRUE(TestPair(            1,       0x1));
    EXPECT_TRUE(TestPair(         CENT,       0x7));
    EXPECT_TRUE(TestPair(         COIN,       0x9));
    EXPECT_TRUE(TestPair(      50*COIN,      0x32));
    EXPECT_TRUE(TestPair(21000000*COIN, 0x1406f40));

    for (uint64_t i = 1; i <= NUM_MULTIPLES_UNIT; i++)
        EXPECT_TRUE(TestEncode(i));

    for (uint64_t i = 1; i <= NUM_MULTIPLES_CENT; i++)
        EXPECT_TRUE(TestEncode(i * CENT));

    for (uint64_t i = 1; i <= NUM_MULTIPLES_1BTC; i++)
        EXPECT_TRUE(TestEncode(i * COIN));

    for (uint64_t i = 1; i <= NUM_MULTIPLES_50BTC; i++)
        EXPECT_TRUE(TestEncode(i * 50 * COIN));

    for (uint64_t i = 0; i < 100000; i++)
        EXPECT_TRUE(TestDecode(i));
}

#include "googletest/googletest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "serialize.h"

using namespace std;

TEST(serialize_tests, varints)
{
    // encode

    CDataStream ss(SER_DISK, 0);
    CDataStream::size_type size = 0;
    for (int i = 0; i < 100000; i++) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        EXPECT_TRUE(size == ss.size());
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        EXPECT_TRUE(size == ss.size());
    }

    // decode
    for (int i = 0; i < 100000; i++) {
        int j;
        ss >> VARINT(j);
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        uint64_t j;
        ss >> VARINT(j);
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }

}

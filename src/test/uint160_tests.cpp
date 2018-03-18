#include "googletest/googletest/include/gtest/gtest.h"

#include "uint256.h"

TEST(uint160_tests, uint160_equality)
{
    uint160 num1 = 10;
    uint160 num2 = 11;
    EXPECT_TRUE(num1+1 == num2);

    uint64_t num3 = 10;
    EXPECT_TRUE(num1 == num3);
    EXPECT_TRUE(num1+num2 == num3+num2);
}

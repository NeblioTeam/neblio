#include "googletest/googletest/include/gtest/gtest.h"

#include "result.h"

TEST(result_tests, basic)
{
    {
        Result<uint64_t, uint32_t> r1 = Ok(UINT64_C(3));

        auto val = r1.expect("Failed to retrieve the value");
        EXPECT_EQ(val, 3);
    }
    {
        Result<std::string, uint32_t> r1 = Ok(std::string("Success!"));

        auto val = r1.expect("Failed to retrieve the value");
        EXPECT_EQ(val, "Success!");
    }
    {
        const std::string str("Success!");

        Result<std::string, uint32_t> r1 = Ok(str);

        auto val = r1.expect("Failed to retrieve the value");
        EXPECT_EQ(val, "Success!");
    }
    {
        Result<std::string, uint32_t> r1 = Err(15u);

        EXPECT_TRUE(r1.isErr());
        EXPECT_EQ(r1.unwrapErr(), 15);
    }
    {
        Result<uint32_t, std::string> r1 = Err(std::string("Error!"));

        EXPECT_TRUE(r1.isErr());
        EXPECT_EQ(r1.unwrapErr(), "Error!");
    }
    {
        const std::string str("Success!");

        Result<uint32_t, std::string> r1 = Err(str);

        EXPECT_TRUE(r1.isErr());
        EXPECT_EQ(r1.unwrapErr(), str);
    }
}

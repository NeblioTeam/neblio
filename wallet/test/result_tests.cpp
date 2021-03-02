#include "googletest/googletest/include/gtest/gtest.h"

#include "result.h"

TEST(result_tests, basic_result)
{
    Result<uint64_t, uint32_t> r1 = Ok(UINT64_C(3));

    auto val = r1.expect("Failed to retrieve the value");
    EXPECT_EQ(val, 3u);
}

TEST(result_tests, rvalue_result)
{
    Result<std::string, uint32_t> r1 = Ok(std::string("Success!"));

    auto val = r1.expect("Failed to retrieve the value");
    EXPECT_EQ(val, "Success!");
}

TEST(result_tests, rvalue_result_forced)

{
    std::string                   str("Success!");
    Result<std::string, uint32_t> r1 = Ok(std::move(str));

    EXPECT_TRUE(str.empty());

    auto val = r1.expect("Failed to retrieve the value");
    EXPECT_EQ(val, "Success!");
}

TEST(result_tests, rvalue_error_forced)
{
    std::string                   str("Success!");
    Result<uint32_t, std::string> r1 = Err(std::move(str));

    EXPECT_TRUE(str.empty());
    EXPECT_TRUE(r1.isErr());

    EXPECT_EQ(r1.unwrapErr(), "Success!");
}

TEST(result_tests, lvalue_result)
{
    const std::string str("Success!");

    Result<std::string, uint32_t> r1 = Ok(str);

    auto val = r1.expect("Failed to retrieve the value");
    EXPECT_EQ(val, "Success!");
}

TEST(result_tests, basic_error)
{
    Result<std::string, uint32_t> r1 = Err(15u);

    EXPECT_TRUE(r1.isErr());
    EXPECT_EQ(r1.unwrapErr(), 15u);
}

TEST(result_tests, rvalue_error)
{
    Result<uint32_t, std::string> r1 = Err(std::string("Error!"));

    EXPECT_TRUE(r1.isErr());
    EXPECT_EQ(r1.unwrapErr(), "Error!");
}

TEST(result_tests, lvalue_error)
{
    const std::string str("Success!");

    Result<uint32_t, std::string> r1 = Err(str);

    EXPECT_TRUE(r1.isErr());
    EXPECT_EQ(r1.unwrapErr(), str);
}

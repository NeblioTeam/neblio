#include "googletest/googletest/include/gtest/gtest.h"

#include <boost/algorithm/hex.hpp>
#include <string>
#include <vector>

#include "SerializationTester.h"
#include "serialize.h"

TEST(serialize_tests, varints)
{
    // encode

    CDataStream            ss(SER_DISK, 0);
    CDataStream::size_type size = 0;
    for (int i = 0; i < 100000; i++) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        EXPECT_TRUE(size == ss.size());
    }

    for (uint64_t i = 0; i < 100000000000ULL; i += 999999937) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        EXPECT_TRUE(size == ss.size());
    }

    // decode
    for (int i = 0; i < 100000; i++) {
        int j = 0;
        ss >> VARINT(j);
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }

    for (uint64_t i = 0; i < 100000000000ULL; i += 999999937) {
        uint64_t j = 0;
        ss >> VARINT(j);
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }
}

TEST(serialize_tests, cross_platform_consistency) { RunCrossPlatformSerializationTests(); }

TEST(serialize_tests, ser_compact_size)
{
    {
        std::string s = boost::algorithm::unhex(std::string("81"));
        CDataStream ssValue(s.c_str(), s.c_str() + s.size(), SER_NETWORK, PROTOCOL_VERSION);
        auto        val = ReadCompactSize(ssValue);
        EXPECT_EQ(val, static_cast<decltype(val)>(129));
    }
    {
        std::string s = boost::algorithm::unhex(std::string("fd3401"));
        CDataStream ssValue(s.c_str(), s.c_str() + s.size(), SER_NETWORK, PROTOCOL_VERSION);
        auto        val = ReadCompactSize(ssValue);
        EXPECT_EQ(val, static_cast<decltype(val)>(308));
    }
    {
        std::string s = boost::algorithm::unhex(std::string("fd0490"));
        CDataStream ssValue(s.c_str(), s.c_str() + s.size(), SER_NETWORK, PROTOCOL_VERSION);
        auto        val = ReadCompactSize(ssValue);
        EXPECT_EQ(val, static_cast<decltype(val)>(36868));
    }
    {
        std::string s = boost::algorithm::unhex(std::string("fefd0f0100"));
        CDataStream ssValue(s.c_str(), s.c_str() + s.size(), SER_NETWORK, PROTOCOL_VERSION);
        auto        val = ReadCompactSize(ssValue);
        EXPECT_EQ(val, static_cast<decltype(val)>(69629));
    }
}

// TEST(serialize_tests,vector_bool)
//{
//    std::vector<uint8_t> vec1{1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1,
//    0, 0, 1}; std::vector<bool> vec2{1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1,
//    0, 1, 0, 0, 1};

//    EXPECT_TRUE(vec1 == std::vector<uint8_t>(vec2.begin(), vec2.end()));
//    EXPECT_TRUE(SerializeHash(vec1) == SerializeHash(vec2));
//}

TEST(serialize_tests, sizes)
{
    EXPECT_EQ(sizeof(char), GetSerializeSize(char(0), 0));
    EXPECT_EQ(sizeof(int8_t), GetSerializeSize(int8_t(0), 0));
    EXPECT_EQ(sizeof(uint8_t), GetSerializeSize(uint8_t(0), 0));
    EXPECT_EQ(sizeof(int16_t), GetSerializeSize(int16_t(0), 0));
    EXPECT_EQ(sizeof(uint16_t), GetSerializeSize(uint16_t(0), 0));
    EXPECT_EQ(sizeof(int32_t), GetSerializeSize(int32_t(0), 0));
    EXPECT_EQ(sizeof(uint32_t), GetSerializeSize(uint32_t(0), 0));
    EXPECT_EQ(sizeof(int64_t), GetSerializeSize(int64_t(0), 0));
    EXPECT_EQ(sizeof(uint64_t), GetSerializeSize(uint64_t(0), 0));
    EXPECT_EQ(sizeof(float), GetSerializeSize(float(0), 0));
    EXPECT_EQ(sizeof(double), GetSerializeSize(double(0), 0));
    // Bool is serialized as char
    EXPECT_EQ(sizeof(char), GetSerializeSize(bool(0), 0));

    // Sanity-check GetSerializeSize and c++ type matching
    EXPECT_EQ(GetSerializeSize(char(0), 0), 1U);
    EXPECT_EQ(GetSerializeSize(int8_t(0), 0), 1U);
    EXPECT_EQ(GetSerializeSize(uint8_t(0), 0), 1U);
    EXPECT_EQ(GetSerializeSize(int16_t(0), 0), 2U);
    EXPECT_EQ(GetSerializeSize(uint16_t(0), 0), 2U);
    EXPECT_EQ(GetSerializeSize(int32_t(0), 0), 4U);
    EXPECT_EQ(GetSerializeSize(uint32_t(0), 0), 4U);
    EXPECT_EQ(GetSerializeSize(int64_t(0), 0), 8U);
    EXPECT_EQ(GetSerializeSize(uint64_t(0), 0), 8U);
    EXPECT_EQ(GetSerializeSize(float(0), 0), 4U);
    EXPECT_EQ(GetSerializeSize(double(0), 0), 8U);
    EXPECT_EQ(GetSerializeSize(bool(0), 0), 1U);
}

/*
Python code to generate the below hashes:

    def reversed_hex(x):
        return binascii.hexlify(''.join(reversed(x)))
    def dsha256(x):
        return hashlib.sha256(hashlib.sha256(x).digest()).digest()

    reversed_hex(dsha256(''.join(struct.pack('<f', x) for x in range(0,1000)))) ==
'8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c'
    reversed_hex(dsha256(''.join(struct.pack('<d', x) for x in range(0,1000)))) ==
'43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96'
*/
TEST(serialize_tests, floats)
{
    CDataStream ss(SER_DISK, 0);
    // encode
    for (int i = 0; i < 1000; i++) {
        ss << float(i);
    }
    EXPECT_TRUE(Hash(ss.begin(), ss.end()) ==
                uint256("8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c"));

    // decode
    for (int i = 0; i < 1000; i++) {
        float j;
        ss >> j;
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }
}

TEST(serialize_tests, doubles)
{
    CDataStream ss(SER_DISK, 0);
    // encode
    for (int i = 0; i < 1000; i++) {
        ss << double(i);
    }
    EXPECT_TRUE(Hash(ss.begin(), ss.end()) ==
                uint256("43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96"));

    // decode
    for (int i = 0; i < 1000; i++) {
        double j;
        ss >> j;
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }
}

TEST(serialize_tests, compactsize)
{
    CDataStream                  ss(SER_DISK, 0);
    std::vector<char>::size_type i, j;

    for (i = 1; i <= MAX_SIZE; i *= 2) {
        WriteCompactSize(ss, i - 1);
        WriteCompactSize(ss, i);
    }
    for (i = 1; i <= MAX_SIZE; i *= 2) {
        j = ReadCompactSize(ss);
        EXPECT_TRUE((i - 1) == j) << "decoded:" << j << " expected:" << (i - 1);
        j = ReadCompactSize(ss);
        EXPECT_TRUE(i == j) << "decoded:" << j << " expected:" << i;
    }
}

TEST(serialize_tests, noncanonical)
{
    // Write some non-canonical CompactSize encodings, and
    // make sure an exception is thrown when read back.
    CDataStream                  ss(SER_DISK, 0);
    std::vector<char>::size_type n;

    // zero encoded with three bytes:
    ss.write("\xfd\x00\x00", 3);
    EXPECT_THROW(ReadCompactSize(ss), std::ios_base::failure);

    // 0xfc encoded with three bytes:
    ss.write("\xfd\xfc\x00", 3);
    EXPECT_THROW(ReadCompactSize(ss), std::ios_base::failure);

    // 0xfd encoded with three bytes is OK:
    ss.write("\xfd\xfd\x00", 3);
    n = ReadCompactSize(ss);
    EXPECT_TRUE(n == 0xfd);

    // zero encoded with five bytes:
    ss.write("\xfe\x00\x00\x00\x00", 5);
    EXPECT_THROW(ReadCompactSize(ss), std::ios_base::failure);

    // 0xffff encoded with five bytes:
    ss.write("\xfe\xff\xff\x00\x00", 5);
    EXPECT_THROW(ReadCompactSize(ss), std::ios_base::failure);

    // zero encoded with nine bytes:
    ss.write("\xff\x00\x00\x00\x00\x00\x00\x00\x00", 9);
    EXPECT_THROW(ReadCompactSize(ss), std::ios_base::failure);

    // 0x01ffffff encoded with nine bytes:
    ss.write("\xff\xff\xff\xff\x01\x00\x00\x00\x00", 9);
    EXPECT_THROW(ReadCompactSize(ss), std::ios_base::failure);
}

TEST(serialize_tests, insert_delete)
{
    // Test inserting/deleting bytes.
    CDataStream ss(SER_DISK, 0);
    EXPECT_EQ(ss.size(), 0U);

    ss.write("\x00\x01\x02\xff", 4);
    EXPECT_EQ(ss.size(), 4U);

    char c = (char)11;

    // Inserting at beginning/end/middle:
    ss.insert(ss.begin(), c);
    EXPECT_EQ(ss.size(), 5U);
    EXPECT_EQ(ss[0], c);
    EXPECT_EQ(ss[1], 0);

    ss.insert(ss.end(), c);
    EXPECT_EQ(ss.size(), 6U);
    EXPECT_EQ(ss[4], (char)0xff);
    EXPECT_EQ(ss[5], c);

    ss.insert(ss.begin() + 2, c);
    EXPECT_EQ(ss.size(), 7U);
    EXPECT_EQ(ss[2], c);

    // Delete at beginning/end/middle
    ss.erase(ss.begin());
    EXPECT_EQ(ss.size(), 6U);
    EXPECT_EQ(ss[0], 0);

    ss.erase(ss.begin() + ss.size() - 1);
    EXPECT_EQ(ss.size(), 5U);
    EXPECT_EQ(ss[4], (char)0xff);

    ss.erase(ss.begin() + 1);
    EXPECT_EQ(ss.size(), 4U);
    EXPECT_EQ(ss[0], 0);
    EXPECT_EQ(ss[1], 1);
    EXPECT_EQ(ss[2], 2);
    EXPECT_EQ(ss[3], (char)0xff);

    // Make sure GetAndClear does the right thing:
    CSerializeData d;
    ss.GetAndClear(d);
    EXPECT_EQ(ss.size(), 0U);
}

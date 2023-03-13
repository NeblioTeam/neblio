#include "bytes.h"

#include <vector>
#include <random>
#include <string>
#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace ledger;

inline static constexpr const uint8_t Base58Map[] = {
    '1', '2', '3', '4', '5', '6', '7', '8',
    '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q',
    'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y',
    'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'm', 'n', 'o', 'p',
    'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z'};
inline static constexpr const uint8_t AlphaMap[] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0xff, 0x11, 0x12, 0x13, 0x14, 0x15, 0xff,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0xff, 0x2c, 0x2d, 0x2e,
    0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xff, 0xff, 0xff, 0xff, 0xff};

std::string Base58Encode(const bytes &data)
{
    bytes digits((data.size() * 138 / 100) + 1);
    size_t digitslen = 1;
    for (size_t i = 0; i < data.size(); i++)
    {
        uint32_t carry = static_cast<uint32_t>(data[i]);
        for (size_t j = 0; j < digitslen; j++)
        {
            carry = carry + static_cast<uint32_t>(digits[j] << 8);
            digits[j] = static_cast<uint8_t>(carry % 58);
            carry /= 58;
        }
        for (; carry; carry /= 58)
            digits[digitslen++] = static_cast<uint8_t>(carry % 58);
    }
    std::string result;
    for (size_t i = 0; i < (data.size() - 1) && !data[i]; i++)
        result.push_back(Base58Map[0]);
    for (size_t i = 0; i < digitslen; i++)
        result.push_back(Base58Map[digits[digitslen - 1 - i]]);
    return result;
}

bytes Base58Decode(const std::string &data)
{
    bytes result((data.size() * 138 / 100) + 1);
    size_t resultlen = 1;
    for (size_t i = 0; i < data.size(); i++)
    {
        uint32_t carry = static_cast<uint32_t>(AlphaMap[data[i] & 0x7f]);
        for (size_t j = 0; j < resultlen; j++, carry >>= 8)
        {
            carry += static_cast<uint32_t>(result[j] * 58);
            result[j] = static_cast<uint8_t>(carry);
        }
        for (; carry; carry >>= 8)
            result[resultlen++] = static_cast<uint8_t>(carry);
    }
    result.resize(resultlen);
    for (size_t i = 0; i < (data.size() - 1) && data[i] == AlphaMap[0]; i++)
        result.push_back(0);
    std::reverse(result.begin(), result.end());
    return result;
}
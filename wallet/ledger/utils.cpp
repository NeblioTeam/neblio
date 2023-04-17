#include "ledger/utils.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace ledger {
std::tuple<uint32_t, uint8_t> DeserializeVarint(const bytes& data, uint32_t offset)
{
    if (data[offset] < 0xfd) {
        return std::make_tuple(data[offset], 1);
    }

    if (data[offset] == 0xfd) {
        return std::make_tuple((data[offset + 2] << 8) + data[offset + 1], 3);
    }

    if (data[offset] == 0xfe) {
        return std::make_tuple((data[offset + 4] << 24) + (data[offset + 3] << 16) +
                                   (data[offset + 2] << 8) + data[offset + 1],
                               5);
    }
}

bytes CreateVarint(uint32_t value)
{
    bytes data;
    if (value < 0xfd) {
        data.push_back(value);
    } else if (value <= 0xffff) {
        data.push_back(0xfd);
        data.push_back(value & 0xff);
        data.push_back((value >> 8) & 0xff);
    } else {
        data.push_back(0xfd);
        data.push_back(value & 0xff);
        data.push_back((value >> 8) & 0xff);
        data.push_back((value >> 16) & 0xff);
        data.push_back((value >> 24) & 0xff);
    }

    return data;
}

std::string BytesToHex(const bytes& vec)
{
    std::stringstream ss;
    for (int i = 0; i < vec.size(); i++) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)vec[i];
    }

    return ss.str();
}

bytes HexToBytes(const std::string& data)
{
    std::stringstream ss;
    ss << data;

    bytes      resBytes;
    size_t     count = 0;
    const auto len   = data.size();
    while (ss.good() && count < len) {
        unsigned short num;
        char           hexNum[2];
        ss.read(hexNum, 2);
        sscanf(hexNum, "%2hX", &num);
        resBytes.push_back(num);
        count += 2;
    }
    return resBytes;
}

uint64_t BytesToUint64(const bytes& _bytes, bool littleEndian)
{
    auto bytesToConvert = _bytes;
    if (littleEndian) {
        bytesToConvert = bytes(bytesToConvert.rbegin(), bytesToConvert.rend());
    }

    uint64_t value = 0;
    for (const uint8_t& byte : bytesToConvert) {
        value = (value << 8) + byte;
    }
    return value;
}

int BytesToInt(const bytes& _bytes, bool littleEndian)
{
    auto bytesToConvert = _bytes;
    if (littleEndian) {
        bytesToConvert = bytes(bytesToConvert.rbegin(), bytesToConvert.rend());
    }

    int value = 0;
    for (const uint8_t& byte : bytesToConvert) {
        value = (value << 8) + byte;
    }
    return value;
}

bytes IntToBytes(uint32_t n, uint32_t length, bool littleEndian)
{
    bytes bytes;
    bytes.reserve(length);
    for (auto i = 0; i < length; i++) {
        bytes.emplace_back((n >> 8 * (length - 1 - i)) & 0xFF);
    }

    if (littleEndian) {
        std::reverse(bytes.begin(), bytes.end());
    }

    return bytes;
}

void AppendUint32(bytes& vector, uint32_t n, bool littleEndian)
{
    AppendVector(vector, IntToBytes(n, 4, littleEndian));
}

bytes Uint64ToBytes(uint64_t n, uint32_t length, bool littleEndian)
{
    bytes bytes;
    bytes.reserve(length);
    for (auto i = 0; i < length; i++) {
        bytes.emplace_back((n >> 8 * (length - 1 - i)) & 0xFF);
    }

    if (littleEndian) {
        std::reverse(bytes.begin(), bytes.end());
    }

    return bytes;
}

void AppendUint64(bytes& vector, uint64_t n, bool littleEndian)
{
    AppendVector(vector, Uint64ToBytes(n, 8, littleEndian));
}

bytes Splice(const bytes& vec, int start, int length)
{
    bytes result(length);
    copy(vec.begin() + start, vec.begin() + start + length, result.begin());

    return result;
}

bytes CompressPubKey(const bytes& pubKey)
{
    if (pubKey.size() != 65) {
        throw std::runtime_error("Invalid public key length");
    }

    if (pubKey[0] != 0x04) {
        throw std::runtime_error("Invalid public key format");
    }

    bytes compressedPubKey(33);
    compressedPubKey[0] = pubKey[64] & 1 ? 0x03 : 0x02;
    copy(pubKey.begin() + 1, pubKey.begin() + 33, compressedPubKey.begin() + 1);

    return compressedPubKey;
}
} // namespace ledger

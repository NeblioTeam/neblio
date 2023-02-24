#include "utils.h"

namespace ledger::utils {
int bytes_to_int(const std::vector<uint8_t>& bytes)
{
    int value = 0;
    for (uint8_t byte : bytes) {
        value = (value << 8) + byte;
    }
    return value;
}

std::vector<uint8_t> int_to_bytes(uint32_t n, uint32_t length)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(length);
    for (auto i = 0; i < length; i++) {
        bytes.emplace_back((n >> 8 * (length - 1 - i)) & 0xFF);
    }
    return bytes;
}

uint32_t hardened(uint32_t n)
{
	return n | 0x80000000;
}
} // namespace ledger::utils

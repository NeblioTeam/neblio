#ifndef _LEDGER_UTILS
#define _LEDGER_UTILS 1

#include <cstdint>
#include <vector>

namespace ledger::utils {
int                  bytes_to_int(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> int_to_bytes(unsigned int n, unsigned int length);
template <typename T>
void append_vector(std::vector<T>& destination, std::vector<T> source)
{
    destination.insert(destination.end(), source.begin(), source.end());
}
uint32_t hardened(uint32_t n);

const uint32_t MAX_RECOMMENDED_ACCOUNT = 100;
const uint32_t MAX_RECOMMENDED_INDEX = 50000;
} // namespace ledger::utils

#endif

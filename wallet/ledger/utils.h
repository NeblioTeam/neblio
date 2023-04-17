#ifndef LEDGER_UTILS_H
#define LEDGER_UTILS_H

#include "ledger/bytes.h"

#include <cstdint>
#include <vector>
#include <string>

namespace ledger
{
	std::tuple<uint32_t, uint8_t> DeserializeVarint(const bytes &data, uint32_t offset);
	bytes CreateVarint(uint32_t value);
	std::string BytesToHex(const bytes &vec);
	bytes HexToBytes(const std::string &data);
	uint64_t BytesToUint64(const bytes &bytes, bool littleEndian = false);
	int BytesToInt(const bytes &bytes, bool littleEndian = false);
	bytes IntToBytes(unsigned int n, unsigned int length, bool littleEndian = false);
	bytes Uint64ToBytes(uint64_t n, unsigned int length, bool littleEndian = false);
	template <typename T>
	void AppendVector(std::vector<T> &destination, const std::vector<T> &source)
	{
		destination.insert(destination.end(), source.begin(), source.end());
	}
	void AppendUint32(bytes &vector, uint32_t n, bool littleEndian = false);
	void AppendUint64(bytes &vector, uint64_t n, bool littleEndian = false);
	bytes Splice(const bytes &vec, int start, int length);
	bytes CompressPubKey(const bytes &pubKey);

const uint32_t MAX_RECOMMENDED_ACCOUNT = 100;
const uint32_t MAX_RECOMMENDED_INDEX = 50000;
} // namespace ledger

#endif // LEDGER_UTILS_H

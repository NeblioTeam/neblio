#ifndef NTP1COMMON_H
#define NTP1COMMON_H

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/exception/to_string.hpp>
#include <string>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

using NTP1Int = boost::multiprecision::cpp_int;

// You should NEVER change these without changing the database version
// These go to the database for verifying issuance transactions duplication
typedef uint32_t          NTP1TransactionType;
const NTP1TransactionType NTP1TxType_UNKNOWN  = 0;
const NTP1TransactionType NTP1TxType_NOT_NTP1 = 1;
const NTP1TransactionType NTP1TxType_ISSUANCE = 2;
const NTP1TransactionType NTP1TxType_TRANSFER = 3;
const NTP1TransactionType NTP1TxType_BURN     = 4;

constexpr const char* METADATA_SER_FIELD__VERSION               = "SerializationVersion";
constexpr const char* METADATA_SER_FIELD__TARGET_PUBLIC_KEY_HEX = "TargetPubKeyHex";
constexpr const char* METADATA_SER_FIELD__SOURCE_PUBLIC_KEY_HEX = "SourcePubKeyHex";
constexpr const char* METADATA_SER_FIELD__CIPHER_BASE64         = "Cipher64";

const std::string  HexBytesRegexStr("^([0-9a-fA-F][0-9a-fA-F])+$");
const boost::regex HexBytexRegex(HexBytesRegexStr);

const NTP1Int NTP1MaxAmount = std::numeric_limits<int64_t>::max();

#endif // NTP1COMMON_H

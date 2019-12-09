// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_HASH_H
#define BITCOIN_HASH_H

#include "serialize.h"
#include "uint256.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <openssl/md5.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>
#include <vector>

template <typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256              hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]),
           (unsigned char*)&hash1);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

class CHashWriter
{
private:
    SHA256_CTX ctx;

public:
    int nType;
    int nVersion;

    void Init() { SHA256_Init(&ctx); }

    CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) { Init(); }

    CHashWriter& write(const char* pch, size_t size)
    {
        SHA256_Update(&ctx, pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 GetHash()
    {
        uint256 hash1;
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template <typename T>
    CHashWriter& operator<<(const T& obj)
    {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

template <typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end)
{
    static unsigned char pblank[1];
    uint256              hash1;
    SHA256_CTX           ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]),
                  (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]),
                  (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template <typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end, const T3 p3begin,
                    const T3 p3end)
{
    static unsigned char pblank[1];
    uint256              hash1;
    SHA256_CTX           ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]),
                  (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]),
                  (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]),
                  (p3end - p3begin) * sizeof(p3begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template <typename T>
uint256 SerializeHash(const T& obj, int nType = SER_GETHASH, int nVersion = PROTOCOL_VERSION)
{
    CHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
    uint256 hash1;
    SHA256(&vch[0], vch.size(), (unsigned char*)&hash1);
    uint160 hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash);

template <typename CTXType, int (*InitFunc)(CTXType*), int (*UpdateFunc)(CTXType*, const void*, size_t),
          int (*FinalFunc)(unsigned char*, CTXType*), unsigned DigestSize>
class HashCalculator
{
    CTXType ctx;

public:
    HashCalculator() { reset(); }
    void push_data(const std::string& data)
    {
        UpdateFunc(&ctx, reinterpret_cast<const void*>(&data.front()), data.size());
    }
    void push_data(const std::vector<char>& data)
    {
        UpdateFunc(&ctx, reinterpret_cast<const void*>(&data.front()), data.size());
    }
    void push_data(const std::vector<unsigned char>& data)
    {
        UpdateFunc(&ctx, reinterpret_cast<const void*>(&data.front()), data.size());
    }
    void reset()
    {
        ctx = CTXType();
        InitFunc(&ctx);
    }
    std::string getHashAndReset()
    {
        std::string res;
        res.resize(DigestSize);
        FinalFunc(reinterpret_cast<unsigned char*>(&res.front()), &ctx);
        reset();
        return res;
    }
};

using Sha1Calculator = HashCalculator<SHA_CTX, SHA1_Init, SHA1_Update, SHA1_Final, SHA_DIGEST_LENGTH>;
using Sha224Calculator =
    HashCalculator<SHA256_CTX, SHA224_Init, SHA224_Update, SHA224_Final, SHA224_DIGEST_LENGTH>;
using Sha256Calculator =
    HashCalculator<SHA256_CTX, SHA256_Init, SHA256_Update, SHA256_Final, SHA256_DIGEST_LENGTH>;
using Sha384Calculator =
    HashCalculator<SHA512_CTX, SHA384_Init, SHA384_Update, SHA384_Final, SHA384_DIGEST_LENGTH>;
using Sha512Calculator =
    HashCalculator<SHA512_CTX, SHA512_Init, SHA512_Update, SHA512_Final, SHA512_DIGEST_LENGTH>;
using Md5Calculator = HashCalculator<MD5_CTX, MD5_Init, MD5_Update, MD5_Final, MD5_DIGEST_LENGTH>;
using Ripemd160HashCalculator = HashCalculator<RIPEMD160_CTX, RIPEMD160_Init, RIPEMD160_Update,
                                               RIPEMD160_Final, RIPEMD160_DIGEST_LENGTH>;

template <typename HashCalculatorClass>
std::string CalculateHashOfFile(const boost::filesystem::path& PathToFile,
                                const std::uintmax_t           ChunkSize = (1 << 20))
{
    if (!boost::filesystem::exists(PathToFile)) {
        throw std::runtime_error("While attempting to calculate hash of file, it does not exist: " +
                                 PathToFile.string());
    }
    boost::filesystem::ifstream fileToRead(PathToFile, std::ios::binary);
    std::string                 chunk;
    chunk.resize(ChunkSize);
    if (!fileToRead.good()) {
        throw std::runtime_error("Unable to open file: " + PathToFile.string() +
                                 "; in order to calculate hash");
    }
    HashCalculatorClass calculator;
    while (!fileToRead.eof()) {
        fileToRead.read(&chunk.front(), ChunkSize);
        std::size_t sz = fileToRead.gcount();
        chunk.resize(sz);
        calculator.push_data(chunk);
    }
    return calculator.getHashAndReset();
}

void* KDF_SHA256(const void* in, size_t inlen, void* out, size_t* outlen);
void* KDF_SHA512(const void* in, size_t inlen, void* out, size_t* outlen);

#endif

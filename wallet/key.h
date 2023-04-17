// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KEY_H
#define BITCOIN_KEY_H

#include <stdexcept>
#include <vector>

#include "allocators.h"
#include "hash.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"

#include <openssl/ec.h> // for EC_KEY definition

// secp160k1
// const unsigned int PRIVATE_KEY_SIZE = 192;
// const unsigned int PUBLIC_KEY_SIZE  = 41;
// const unsigned int SIGNATURE_SIZE   = 48;
//
// secp192k1
// const unsigned int PRIVATE_KEY_SIZE = 222;
// const unsigned int PUBLIC_KEY_SIZE  = 49;
// const unsigned int SIGNATURE_SIZE   = 57;
//
// secp224k1
// const unsigned int PRIVATE_KEY_SIZE = 250;
// const unsigned int PUBLIC_KEY_SIZE  = 57;
// const unsigned int SIGNATURE_SIZE   = 66;
//
// secp256k1:
// const unsigned int PRIVATE_KEY_SIZE = 279;
// const unsigned int PUBLIC_KEY_SIZE  = 65;
// const unsigned int SIGNATURE_SIZE   = 72;
//
// see www.keylength.com
// script supports up to 75 for single byte push

class key_error : public std::runtime_error
{
public:
    explicit key_error(const std::string& str) : std::runtime_error(str) {}
};

/** A reference to a CKey: the Hash160 of its serialized public key */
class CKeyID : public uint160
{
public:
    CKeyID() : uint160(0) {}
    CKeyID(const uint160& in) : uint160(in) {}
};

/** A reference to a CScript: the Hash160 of its serialization (see script.h) */
class CScriptID : public uint160
{
public:
    CScriptID() : uint160(0) {}
    CScriptID(const uint160& in) : uint160(in) {}
};

/** An encapsulated public key. */
class CPubKey
{
private:
    std::vector<unsigned char> vchPubKey;
    friend class CKey;

public:
    CPubKey() {}
    CPubKey(const std::vector<unsigned char>& vchPubKeyIn) : vchPubKey(vchPubKeyIn) {}

    template <typename T>
    CPubKey(T&& pbegin, const T&& pend)
    {
        vchPubKey.assign(pbegin, pend);
    }

    friend bool operator==(const CPubKey& a, const CPubKey& b) { return a.vchPubKey == b.vchPubKey; }
    friend bool operator!=(const CPubKey& a, const CPubKey& b) { return a.vchPubKey != b.vchPubKey; }
    friend bool operator<(const CPubKey& a, const CPubKey& b) { return a.vchPubKey < b.vchPubKey; }

    IMPLEMENT_SERIALIZE(READWRITE(vchPubKey);)

    void SetRaw(std::vector<unsigned char> rawKey) {vchPubKey = rawKey;}

    CKeyID GetID() const { return CKeyID(Hash160(vchPubKey)); }

    uint256 GetHash() const { return Hash(vchPubKey.begin(), vchPubKey.end()); }

    bool IsValid() const { return vchPubKey.size() == 33 || vchPubKey.size() == 65; }

    bool IsCompressed() const { return vchPubKey.size() == 33; }

    std::vector<unsigned char> Raw() const { return vchPubKey; }
};

// secure_allocator is defined in allocators.h
// CPrivKey is a serialized private key, with all parameters included (279 bytes)
typedef std::vector<unsigned char, secure_allocator<unsigned char>> CPrivKey;
// CSecret is a serialization of just the secret parameter (32 bytes)
typedef std::vector<unsigned char, secure_allocator<unsigned char>> CSecret;

/** An encapsulated OpenSSL Elliptic Curve key (public and/or private) */
class CKey
{
protected:
    EC_KEY* pkey;
    bool    fSet;
    bool    fCompressedPubKey;

public:
    void SetCompressedPubKey(bool fCompressed = true);

    void Reset();

    CKey();
    CKey(const CKey& b);

    CKey& operator=(const CKey& b);

    ~CKey();

    bool IsNull() const;
    bool IsCompressed() const;

    void     MakeNewKey(bool fCompressed);
    bool     SetPrivKey(const CPrivKey& vchPrivKey);
    bool     SetSecret(const CSecret& vchSecret, bool fCompressed = false);
    CSecret  GetSecret(bool& fCompressed) const;
    CPrivKey GetPrivKey() const;
    bool     SetPubKey(const CPubKey& vchPubKey);
    CPubKey  GetPubKey() const;

    bool Sign(uint256 hash, std::vector<unsigned char>& vchSig) const;

    // create a compact signature (65 bytes), which allows reconstructing the used public key
    // The format is one header byte, followed by two times 32 bytes for the serialized r and s values.
    // The header byte: 0x1B = first key with even y, 0x1C = first key with odd y,
    //                  0x1D = second key with even y, 0x1E = second key with odd y
    bool SignCompact(uint256 hash, std::vector<unsigned char>& vchSig);

    // reconstruct public key from a compact signature
    // This is only slightly more CPU intensive than just verifying it.
    // If this function succeeds, the recovered public key is guaranteed to be valid
    // (the signature is a valid signature of the given data for that key)
    bool SetCompactSignature(uint256 hash, const std::vector<unsigned char>& vchSig);

    bool Verify(uint256 hash, const std::vector<unsigned char>& vchSig);

    bool IsValid();

    // Check whether an element of a signature (r or s) is valid.
    static bool CheckSignatureElement(const unsigned char* vch, int len, bool half);

    const EC_KEY* getRawKey() const { return pkey; }

    std::function<void(EC_KEY*)> EcKeyDeleter = [](EC_KEY* eckey) {
        if (!eckey)
            return;
        EC_KEY_free(eckey);
    };

    using EcKeyPtr = std::unique_ptr<EC_KEY, decltype(EcKeyDeleter)>;

    EcKeyPtr GetLowLevelPublicKey() const;
    EcKeyPtr GetLowLevelPrivateKey() const;

    [[nodiscard]] static std::array<uint8_t, 32>
    GenerateSharedSecretFromPrivateAndPublicKey(const CKey& privateKey, const CKey& publicKey);
    [[nodiscard]] std::pair<CKey, std::array<uint8_t, 32>>
    GenerateEphemeralSharedSecretFromThisPublicKey() const;
    [[nodiscard]] std::array<uint8_t, 32>
    GenerateSharedSecretFromThisPrivateKey(const CKey& publicKey) const;
};

/** Check that required EC support is available at runtime */
bool ECC_InitSanityCheck(void);

class CLedgerKey
{
public:
    CPubKey vchPubKey;
    CKeyID accountPubKeyID;
    uint32_t account;
    bool isChange;
    uint32_t index;

    CLedgerKey() {}

    CLedgerKey(const CPubKey& vchPubKeyIn, const CKeyID& accountPubKeyIDIn, uint32_t accountIn, bool isChangeIn, uint32_t indexIn) :
        vchPubKey(vchPubKeyIn), accountPubKeyID(accountPubKeyIDIn), account(accountIn), isChange(isChangeIn), index(indexIn) {};

    IMPLEMENT_SERIALIZE(READWRITE(vchPubKey); READWRITE(accountPubKeyID); READWRITE(account); READWRITE(isChange); READWRITE(index);)
};

#endif

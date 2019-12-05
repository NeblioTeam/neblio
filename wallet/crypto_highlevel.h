#ifndef CRYPTO_HIGHLEVEL_H
#define CRYPTO_HIGHLEVEL_H

#include "CustomTypes.h"
#include "allocators.h"
#include "json_spirit.h"
#include <boost/optional.hpp>
#include <sodium.h>
#include <sodium/crypto_secretbox_xsalsa20poly1305.h>
#include <vector>

class Crypto_HighLevel
{
public:
    typedef std::vector<uint8_t, secure_allocator<uint8_t>> SecureBytes;
    typedef std::vector<uint8_t>                            Bytes;
    typedef std::string                                     String;

    // MAX_MSG_SIZE based on: https://nacl.cr.yp.to/valid.html
    static const int MAX_CRYPTOSECRETBOX_MSG_SIZE             = 4096;
    static const int MAX_CRYPTOSECRETBOX_ENCRYPTABLE_MSG_SIZE = 4096 - crypto_secretbox_ZEROBYTES;

    static constexpr const char* XSalsa20Poly1305AlgoName = "XSalsa20Poly1305";

    static constexpr const char* Poly1305AlgoName = "XSalsa20Poly1305";

    static constexpr const char* Sha256RatchetName = "SHA256";
    static constexpr const char* Sha384RatchetName = "SHA384";
    static constexpr const char* Sha512RatchetName = "SHA512";

    struct EncryptMessageOutput
    {
        static constexpr const char* SER_FIELD__SER_VERSION           = "SerializationVersion";
        static constexpr const char* SER_FIELD__ENC_ALGO              = "EncAlgo";
        static constexpr const char* SER_FIELD__AUTH_ALGO             = "AuthAlgo";
        static constexpr const char* SER_FIELD__AUTH_KEY_RATCHET_ALGO = "AuthKeyRatchetAlgo";
        static constexpr const char* SER_FIELD__IV_POSITION           = "IVPos";
        static constexpr const char* SER_FIELD__IV_LENGTH             = "IVLen";
        static constexpr const char* SER_FIELD__CIPHER_LENGTH         = "CipherLen";
        static constexpr const char* SER_FIELD__CIPHER_POSITION       = "CipherPos";
        static constexpr const char* SER_FIELD__AUTH_DATA_LENGTH      = "AuthDataLen";
        static constexpr const char* SER_FIELD__AUTH_DATA_POSITION    = "AuthDataPos";

        Bytes                                     cipher;
        Bytes                                     nonce;
        Bytes                                     authData;
        std::string                               encryptionAlgo;
        std::string                               keyRatchetAlgo;
        std::string                               authAlgo;
        void                                      assertNoneIsEmpty() const;
        [[nodiscard]] static Bytes                Serialize(const EncryptMessageOutput& cipherData);
        [[nodiscard]] static EncryptMessageOutput Deserialize(const Bytes& data);
    };

    enum EncryptionAlgorithm : uint16_t
    {
        Enc_XSalsa20Poly1305 = 0,

        Enc_Size
    };

    enum AuthenticationAlgorithm : uint16_t
    {
        Auth_Poly1305 = 0,

        Auth_Size
    };

    enum AuthKeyRatchetAlgorithm : uint16_t
    {
        Ratchet_Sha256 = 0,
        Ratchet_Sha384,
        Ratchet_Sha512,

        Ratchet_Size
    };

    static String GetOpenSSLErrorMsg();

    [[nodiscard]] static Bytes XSalsa20poly1305_EncryptBlock(
        const Bytes&                                                                   msg,
        const std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_NONCEBYTES>& nonce,
        const std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>&   key);

    [[nodiscard]] static Bytes XSalsa20poly1305_DecryptBlock(
        const Bytes&                                                                   cipher,
        const std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_NONCEBYTES>& nonce,
        const std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>&   key);

    static std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_NONCEBYTES>
    GenSalsa20poly1305RandomNonce();

    static std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>
    GetCanonicalSalsa20poly1305Key(const Bytes& key, const std::string& algoName);

    [[nodiscard]] static Bytes XSalsa20poly1305_EncryptLongMsg_CTR(
        const Crypto_HighLevel::Bytes&                                               msg,
        std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_NONCEBYTES>      nonce,
        const std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>& key);

    [[nodiscard]] static std::array<uint8_t, crypto_onetimeauth_poly1305_BYTES>
    Poly1305AuthenticateMessage(
        const Crypto_HighLevel::Bytes&                                         msg,
        const std::array<unsigned char, crypto_onetimeauth_poly1305_KEYBYTES>& key);

    [[nodiscard]] static bool
    Poly1305VerifyMessage(const Crypto_HighLevel::Bytes&                                         msg,
                          const std::array<uint8_t, crypto_onetimeauth_poly1305_BYTES>&          tag,
                          const std::array<unsigned char, crypto_onetimeauth_poly1305_KEYBYTES>& key);

    [[nodiscard]] static Bytes XSalsa20poly1305_DecryptLongMsg_CTR(
        const Bytes&                                                                 cipher,
        const std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>& key);

    [[nodiscard]] static std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>
    GenXSalsa20poly1305RandomKey();

    [[nodiscard]] static Bytes RandomBytes(uint64_t length);
    template <typename T>
    static typename std::enable_if<std::is_trivial<T>::value, T>::type RandomBytesAs()
    {
        static_assert(std::is_trivial<T>::value, "T must be a trivial type");
        Bytes b = RandomBytes(sizeof(T));
        T     res{};
        memcpy(&res, b.data(), sizeof(T));
        return res;
    }

    template <std::size_t N>
    static void IncrementNonce(std::array<uint8_t, N>& nonce);

    template <typename T>
    [[nodiscard]] static std::array<uint8_t, sizeof(T)> SerializeSimple(T val);

    template <typename T>
    [[nodiscard]] static T DeserializeSimple(std::array<uint8_t, sizeof(T)> data);

    [[nodiscard]] static boost::optional<std::string> GetEncryptionAlgoName(EncryptionAlgorithm algo);
    [[nodiscard]] static EncryptionAlgorithm          GetEncryptionAlgoFromName(const StringViewT name);
    [[nodiscard]] static boost::optional<uint64_t> GetEncryptionAlgoKeyLength(EncryptionAlgorithm algo);
    [[nodiscard]] static boost::optional<std::string> GetRatchetAlgoName(AuthKeyRatchetAlgorithm algo);
    [[nodiscard]] static AuthKeyRatchetAlgorithm      GetRatchetAlgoFromName(const StringViewT name);
    [[nodiscard]] static boost::optional<uint64_t>
                                                      GetRatchetAlgoOutputLength(AuthKeyRatchetAlgorithm algo);
    [[nodiscard]] static boost::optional<uint64_t>    GetAuthAlgoKeyLength(AuthenticationAlgorithm algo);
    [[nodiscard]] static boost::optional<std::string> GetAuthAlgoName(AuthenticationAlgorithm algo);
    [[nodiscard]] static AuthenticationAlgorithm      GetAuthAlgoFromName(StringViewT name);

    [[nodiscard]] static EncryptMessageOutput EncryptMessage(const Bytes& message, const Bytes& key,
                                                             EncryptionAlgorithm     encAlgo,
                                                             AuthKeyRatchetAlgorithm keyRatchetAlgo,
                                                             AuthenticationAlgorithm authAlgo);
    [[nodiscard]] static Bytes                DecryptMessage(const EncryptMessageOutput& encryptedData,
                                                             const Bytes&                key);

    [[nodiscard]] static Bytes
    CalculateKeyRatchet(AuthKeyRatchetAlgorithm keyRatchetAlgo, const Bytes& key,
                        boost::optional<uint64_t> maxAuthenticationAlgoKeyLen = boost::none);

    template <typename Container>
    [[nodiscard]] static Bytes ToBytes(Container&& input);

    template <typename Container>
    [[nodiscard]] static std::string ToString(Container&& input);
};

template <typename Container>
Crypto_HighLevel::Bytes Crypto_HighLevel::ToBytes(Container&& input)
{
    return Bytes(input.begin(), input.end());
}

template <typename Container>
std::string Crypto_HighLevel::ToString(Container&& input)
{
    return std::string(input.begin(), input.end());
}

template <std::size_t N>
void Crypto_HighLevel::IncrementNonce(std::array<uint8_t, N>& nonce)
{
    static_assert(N % sizeof(uint64_t) == 0, "The nonce is expected to be integer multiples of 8");
    // we have two cases, either all bytes are 0xFF, which means the next nonce is 0, or other wise, we
    // loop over all bytes, add one, and move the carry to the next byte

    // cover the case of 0xFFFFFFFFF...
    if (std::all_of(nonce.cbegin(), nonce.cend(), [](uint8_t c) { return c == UINT8_C(0xFF); })) {
        std::memset(nonce.data(), 0, nonce.size());
        return;
    }

    for (unsigned i = 0; i < nonce.size(); i++) {
        unsigned idx = nonce.size() - i - 1;
        if (nonce[idx] != 0xFF) {
            nonce[idx] += 1;
            break;
        } else {
            nonce[idx] = UINT8_C(0);
        }
    }
}

template <typename T>
std::array<uint8_t, sizeof(T)> Crypto_HighLevel::SerializeSimple(T val)
{
    static_assert(std::is_trivial<T>::value, "You can only serialize trivial types");
    static_assert(std::is_integral<T>::value, "This serialization is only for ints");
    std::array<uint8_t, sizeof(T)> res;
    for (unsigned i = 0; i < res.size(); i++) {
        res[i] = 0xFF & (val >> 8 * i);
    }
    return res;
}

template <typename T>
T Crypto_HighLevel::DeserializeSimple(std::array<uint8_t, sizeof(T)> data)
{
    static_assert(std::is_trivial<T>::value, "You can only serialize trivial types");
    static_assert(std::is_integral<T>::value, "This serialization is only for ints");
    T res = 0;
    for (unsigned i = 0; i < data.size(); i++) {
        res = res << 8;
        res |= data[data.size() - i - 1];
    }
    return res;
}

using CHL = Crypto_HighLevel;

#endif // CRYPTO_HIGHLEVEL_H

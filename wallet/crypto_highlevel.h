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

    template <typename T, std::size_t Size>
    struct SecureArray : public std::array<T, Size>
    {
        static_assert(std::is_pod<T>::value, "Only pod types are allowed");
        static_assert(sizeof(T) == 1, "The size of secure array elements mut be one");
        virtual ~SecureArray()
        {
            Bytes d = RandomBytes(Size);
            std::memcpy(this->data(), d.data(), Size);
        }
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
        const SecureArray<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>&  key);

    [[nodiscard]] static Bytes XSalsa20poly1305_DecryptBlock(
        const Bytes&                                                                   cipher,
        const std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_NONCEBYTES>& nonce,
        const SecureArray<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>&  key);

    static std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_NONCEBYTES>
    GenSalsa20poly1305RandomNonce();

    static SecureArray<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>
    GetCanonicalSalsa20poly1305Key(const SecureBytes& key, const std::string& algoName);

    [[nodiscard]] static Bytes XSalsa20poly1305_EncryptLongMsg_CTR(
        const Crypto_HighLevel::Bytes&                                                msg,
        std::array<unsigned char, crypto_secretbox_xsalsa20poly1305_NONCEBYTES>       nonce,
        const SecureArray<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>& key);

    [[nodiscard]] static std::array<uint8_t, crypto_onetimeauth_poly1305_BYTES>
    Poly1305AuthenticateMessage(
        const Crypto_HighLevel::Bytes&                                          msg,
        const SecureArray<unsigned char, crypto_onetimeauth_poly1305_KEYBYTES>& key);

    [[nodiscard]] static bool
    Poly1305VerifyMessage(const Crypto_HighLevel::Bytes&                                          msg,
                          const std::array<uint8_t, crypto_onetimeauth_poly1305_BYTES>&           tag,
                          const SecureArray<unsigned char, crypto_onetimeauth_poly1305_KEYBYTES>& key);

    [[nodiscard]] static Bytes XSalsa20poly1305_DecryptLongMsg_CTR(
        const Bytes&                                                                  cipher,
        const SecureArray<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>& key);

    [[nodiscard]] static SecureArray<unsigned char, crypto_secretbox_xsalsa20poly1305_KEYBYTES>
    GenXSalsa20poly1305RandomKey();

    [[nodiscard]] static Bytes       RandomBytes(uint64_t length);
    [[nodiscard]] static SecureBytes RandomBytes_Secure(uint64_t length);
    template <typename T>
    static T RandomBytesAs()
    {
        static_assert(std::is_pod<T>::value, "T must be a pod type");
        Bytes b = RandomBytes(sizeof(T));
        T     res{};
        memcpy(&res, b.data(), sizeof(T));
        return res;
    }

    template <std::size_t Size>
    static SecureArray<uint8_t, Size> RandomBytesAsSecureArray()
    {
        SecureBytes                b = RandomBytes_Secure(Size);
        SecureArray<uint8_t, Size> res{};
        memcpy(res.data(), b.data(), Size);
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

    [[nodiscard]] static EncryptMessageOutput
                               EncryptMessage(const Bytes& message, const SecureBytes& key, EncryptionAlgorithm encAlgo,
                                              AuthKeyRatchetAlgorithm keyRatchetAlgo, AuthenticationAlgorithm authAlgo);
    [[nodiscard]] static Bytes DecryptMessage(const EncryptMessageOutput& encryptedData,
                                              const SecureBytes&          key);

    [[nodiscard]] static SecureBytes
    CalculateKeyRatchet(AuthKeyRatchetAlgorithm keyRatchetAlgo, const SecureBytes& key,
                        boost::optional<uint64_t> maxAuthenticationAlgoKeyLen = boost::none);

    template <typename Container>
    [[nodiscard]] static Bytes ToBytes(Container&& input);

    template <typename Container>
    [[nodiscard]] static SecureBytes ToSecureBytes(Container&& input);

    template <typename Container>
    [[nodiscard]] static std::string ToString(Container&& input);
};

template <typename Container>
Crypto_HighLevel::Bytes Crypto_HighLevel::ToBytes(Container&& input)
{
    return Bytes(input.begin(), input.end());
}

template <typename Container>
Crypto_HighLevel::SecureBytes Crypto_HighLevel::ToSecureBytes(Container&& input)
{
    return SecureBytes(input.begin(), input.end());
}

template <typename Container>
std::string Crypto_HighLevel::ToString(Container&& input)
{
    return std::string(input.begin(), input.end());
}

template <std::size_t N>
void Crypto_HighLevel::IncrementNonce(std::array<uint8_t, N>& nonce)
{
    uint_fast16_t current = 1;
    for (int i = 0; i < nonce.size(); i++) {
        const unsigned idx = nonce.size() - i - 1;
        current += static_cast<uint_fast16_t>(nonce[idx]);
        nonce[idx] = static_cast<unsigned char>(current);
        current >>= 8;
    }
}

template <typename T>
std::array<uint8_t, sizeof(T)> Crypto_HighLevel::SerializeSimple(T val)
{
    static_assert(std::is_pod<T>::value, "You can only serialize pod types");
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
    static_assert(std::is_pod<T>::value, "You can only serialize pod types");
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

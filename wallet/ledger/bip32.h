#ifndef LEDGER_BIP32_H
#define LEDGER_BIP32_H

#include "ledger/bytes.h"

#include <cstdint>
#include <vector>
#include <string>

namespace ledger
{
    class Bip32Path
    {
        public:
            const int PURPOSE_INDEX = 0;
            const int COIN_TYPE_INDEX = 1;
            const int ACCOUNT_INDEX = 2;
            const int CHANGE_INDEX = 3;
            const int ADDRESS_INDEX_INDEX = 4;

            const int BIP32_PURPOSE = 44;
            const int BIP32_COIN_TYPE = 146;

            Bip32Path(const std::string &keyPath);
            Bip32Path(uint32_t account);
            Bip32Path(const std::string &account, bool isChange, const std::string &index);
            Bip32Path(uint32_t account, bool isChange, uint32_t index);

            uint32_t Harden(uint32_t n) const;
            uint32_t Unharden(uint32_t n) const;
            bool IsHardened(uint32_t n) const;

            Bip32Path ToChangePath() const;

            bytes Serialize() const;
            std::string ToString() const;

        private:
            enum class Bip32PathType
            {
                Account,
                Address
            };

            Bip32PathType type;
            std::vector<uint32_t> components;
    };
}

#endif // LEDGER_BIP32_H

#ifndef __LEDGER_BIP32
#define __LEDGER_BIP32 1

#include "bytes.h"

#include <cstdint>
#include <vector>
#include <string>

namespace ledger
{
    class Bip32Path 
    {
        public:
            const int BIP32_PURPOSE = 44;
            const int BIP32_COIN_TYPE = 146;

            Bip32Path(const std::string &keyPath);
            Bip32Path(const std::string &account, bool isChange, const std::string &index);
            Bip32Path(uint32_t account, bool isChange, uint32_t index);
    
            uint32_t Harden(uint32_t n) const;
            uint32_t Unharden(uint32_t n) const;
            bool IsHardened(uint32_t n) const;

            bytes Serialize() const;
            std::string ToString() const;

        private:
            uint32_t purpose;
            uint32_t coinType;
            uint32_t account;
            bool isChange;
            uint32_t index;            
    };
}

#endif
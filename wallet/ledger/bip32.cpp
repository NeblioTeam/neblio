#include "bip32.h"
#include "utils.h"

#include <sstream>

namespace ledger
{
    uint32_t Bip32Path::Harden(uint32_t n) const
    {
        return n | 0x80000000;
    }

    uint32_t Bip32Path::Unharden(uint32_t n) const
    {
        return n & ~(0x80000000);
    }

    bool Bip32Path::IsHardened(uint32_t n) const
    {
        return n == Harden(n);
    }

    Bip32Path::Bip32Path(const std::string &keyPathStr) {
        std::vector<uint32_t> components;
        std::stringstream ss(keyPathStr);
        std::string item;
        bool first = true;
        while (std::getline(ss, item, '/'))
        {
            if (item.compare("m") == 0)
            {
                if (first)
                {
                    first = false;
                    continue;
                }
                throw std::runtime_error("Invalid keypath");
            }
            // Finds whether it is hardened
            uint32_t path = 0;
            size_t pos = item.find("'");
            if (pos != std::string::npos)
            {
                // The hardened tick can only be in the last index of the string
                if (pos != item.size() - 1)
                {
                    throw std::runtime_error("Invalid keypath");
                }
                path |= 0x80000000;
                item = item.substr(0, item.size() - 1); // Drop the last character which is the hardened tick
            }

            // Ensure this is only numbers
            if (item.find_first_not_of("0123456789") != std::string::npos)
            {
                throw std::runtime_error("Invalid keypath");
            }

            // utils::AppendUint32(keypath, std::stoul(item) | path);
            components.push_back(std::stoul(item) | path);

            first = false;
        }

        if (components.size() != 5)
        {
            throw std::runtime_error("Invalid keypath size");
        }

        if (components[0] != Harden(BIP32_PURPOSE))
        {
            throw std::runtime_error("Invalid keypath purpose");
        }

        if (components[1] != Harden(BIP32_COIN_TYPE))
        {
            throw std::runtime_error("Invalid keypath coin type");
        }

        if (!IsHardened(components[2]))
        {
            throw std::runtime_error("Invalid keypath account");
        }

        if (IsHardened(components[3]))
        {
            throw std::runtime_error("Invalid keypath change");
        }

        if (IsHardened(components[4]))
        {
            throw std::runtime_error("Invalid keypath index");
        }

        account = Unharden(components[2]);
        isChange = components[3] == 1;
        index = components[4];
    }

	Bip32Path::Bip32Path(const std::string &account, bool isChange, const std::string &index)
        : Bip32Path(std::stoul(account), isChange, std::stoul(index)) {};

    Bip32Path::Bip32Path(uint32_t account, bool isChange, uint32_t index)
        : purpose(BIP32_PURPOSE), coinType(BIP32_COIN_TYPE), account(account), isChange(isChange), index(index) {};

    bytes Bip32Path::Serialize() const
    {
        bytes serializedKeyPath;
        
        utils::AppendUint32(serializedKeyPath, Harden(purpose));
        utils::AppendUint32(serializedKeyPath, Harden(coinType));
        utils::AppendUint32(serializedKeyPath, Harden(account));
        utils::AppendUint32(serializedKeyPath, isChange ? 1 : 0);
        utils::AppendUint32(serializedKeyPath, index);
        
        return serializedKeyPath;
    }

    std::string Bip32Path::ToString() const 
    { 
        std::stringstream ss;
        ss << "m/" << purpose << "'/" << coinType << "'/" << account << "'/" << (isChange ? 1 : 0) << "/" << index;
        return ss.str();
     }
} // namespace ledger

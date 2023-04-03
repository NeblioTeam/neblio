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
        std::vector<uint32_t> _components;
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
            _components.push_back(std::stoul(item) | path);

            first = false;
        }

        if (_components.size() != 3 || _components.size() != 5)
        {
            throw std::runtime_error("Invalid keypath size");
        }

        if (_components[0] != Harden(BIP32_PURPOSE))
        {
            throw std::runtime_error("Invalid keypath purpose");
        }

        if (_components[1] != Harden(BIP32_COIN_TYPE))
        {
            throw std::runtime_error("Invalid keypath coin type");
        }

        if (!IsHardened(_components[2]))
        {
            throw std::runtime_error("Invalid keypath account");
        }

        if (_components.size() == 5 && IsHardened(_components[3]))
        {
            throw std::runtime_error("Invalid keypath change");
        }

        if (_components.size() == 5 && IsHardened(_components[4]))
        {
            throw std::runtime_error("Invalid keypath index");
        }
        
        components.push_back(BIP32_PURPOSE);
        components.push_back(BIP32_COIN_TYPE);
        components.push_back(_components[ACCOUNT_INDEX]);
        components.push_back(_components[CHANGE_INDEX]);
        components.push_back(_components[ADDRESS_INDEX_INDEX]);
    }
    
    Bip32Path::Bip32Path(uint32_t account) {
        components.push_back(BIP32_PURPOSE);
        components.push_back(BIP32_COIN_TYPE);
        components.push_back(account);
    }

	Bip32Path::Bip32Path(const std::string &account, bool isChange, const std::string &index)
        : Bip32Path(std::stoul(account), isChange, std::stoul(index)) {};

    Bip32Path::Bip32Path(uint32_t account, bool isChange, uint32_t index)
        : Bip32Path(account) {
        components.push_back(isChange ? 1 : 0);
        components.push_back(index);
    };

    bytes Bip32Path::Serialize() const
    {
        bytes serializedKeyPath;
        
        utils::AppendUint32(serializedKeyPath, Harden(components[PURPOSE_INDEX]));
        utils::AppendUint32(serializedKeyPath, Harden(components[COIN_TYPE_INDEX]));
        utils::AppendUint32(serializedKeyPath, Harden(components[ACCOUNT_INDEX]));
        utils::AppendUint32(serializedKeyPath, components[CHANGE_INDEX]);
        utils::AppendUint32(serializedKeyPath, components[ADDRESS_INDEX_INDEX]);
        
        return serializedKeyPath;
    }

    std::string Bip32Path::ToString() const 
    { 
        std::stringstream ss;
        
        ss << "m/" << components[PURPOSE_INDEX] << "'/" << components[COIN_TYPE_INDEX] << "'/" << components[ACCOUNT_INDEX];
        
        if (components.size() > 3) {
            ss << "/" << components[CHANGE_INDEX];
        }
        
        if (components.size() > 4) {
            ss << "/" << components[ADDRESS_INDEX_INDEX];
        }
        
        return ss.str();
     }
} // namespace ledger

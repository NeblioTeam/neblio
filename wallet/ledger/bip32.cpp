#include "bip32.h"
#include "utils.h"

#include <sstream>

namespace ledger::bip32
{
    uint32_t Harden(uint32_t n)
    {
        return n | 0x80000000;
    }

    // copied from https://github.com/bitcoin/bitcoin/blob/master/src/util/bip32.cpp#L13
    // and adjusted for uint8_t instead of uint32_t vector
    bytes ParseHDKeypath(const std::string &keypath_str)
    {
        bytes keypath;
        std::stringstream ss(keypath_str);
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

            utils::AppendUint32(keypath, std::stoul(item) | path);

            first = false;
        }
        return keypath;
    }

	std::string GetBip32Path(const std::string &account, const std::string &index)
	{
		std::stringstream ss;
        ss << "m/44'/146'/" << account << "'/0/" << index;
		return ss.str();
	}

	std::string GetBip32Path(uint32_t account, uint32_t index)
	{
		return GetBip32Path(std::to_string(account), std::to_string(index));
	}
} // namespace ledger::utils

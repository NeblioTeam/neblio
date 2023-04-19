#include "ledger/bip32.h"
#include "ledger/utils.h"

#include <sstream>

namespace ledger {
uint32_t Bip32Path::Harden(uint32_t n) const { return n | 0x80000000; }

uint32_t Bip32Path::Unharden(uint32_t n) const { return n & ~(0x80000000); }

bool Bip32Path::IsHardened(uint32_t n) const { return n == Harden(n); }

Bip32Path Bip32Path::ToChangePath() const
{
    if (type == Bip32PathType::Account) {
        throw std::runtime_error("Account keypath cannot be converted to change path!");
    }

    return Bip32Path(components[ACCOUNT_INDEX], true, components[ADDRESS_INDEX_INDEX]);
}

Bip32Path::Bip32Path(uint32_t account)
{
    type = Bip32PathType::Account;

    components.push_back(BIP32_PURPOSE);
    components.push_back(BIP32_COIN_TYPE);
    components.push_back(account);
}

Bip32Path::Bip32Path(const std::string& account, bool isChange, const std::string& index)
    : Bip32Path(std::stoul(account), isChange, std::stoul(index)){};

Bip32Path::Bip32Path(uint32_t account, bool isChange, uint32_t index) : Bip32Path(account)
{
    type = Bip32PathType::Address;

    components.push_back(isChange ? 1 : 0);
    components.push_back(index);
};

bytes Bip32Path::Serialize() const
{
    bytes serializedKeyPath;

    AppendUint32(serializedKeyPath, Harden(components[PURPOSE_INDEX]));
    AppendUint32(serializedKeyPath, Harden(components[COIN_TYPE_INDEX]));
    AppendUint32(serializedKeyPath, Harden(components[ACCOUNT_INDEX]));

    if (type == Bip32PathType::Address) {
        AppendUint32(serializedKeyPath, components[CHANGE_INDEX]);
        AppendUint32(serializedKeyPath, components[ADDRESS_INDEX_INDEX]);
    }

    return serializedKeyPath;
}

std::string Bip32Path::ToString() const
{
    std::stringstream ss;

    ss << "m/" << components[PURPOSE_INDEX] << "'/" << components[COIN_TYPE_INDEX] << "'/"
       << components[ACCOUNT_INDEX] << "'";

    if (type == Bip32PathType::Address) {
        ss << "/" << components[CHANGE_INDEX];
        ss << "/" << components[ADDRESS_INDEX_INDEX];
    }

    return ss.str();
}
} // namespace ledger

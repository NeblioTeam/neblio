#ifndef _LEDGER_LEDGER
#define _LEDGER_LEDGER 1

#include "transport.h"

namespace ledger {
class Ledger
{
    enum APDU : uint8_t
    {
        CLA                       = 0xe0,
        INS_GET_APP_CONFIGURATION = 0x01,
        INS_GET_PUBLIC_KEY        = 0x40,
        INS_SIGN                  = 0x03,
    };

public:
    Ledger();
    ~Ledger();

    Error open();

    std::tuple<Error, std::vector<uint8_t>> get_public_key(uint32_t account, bool confirm = false);
    std::tuple<Error, std::vector<uint8_t>> sign(uint32_t account, const std::vector<uint8_t>& msg);

    void close();

private:
    std::unique_ptr<Transport> transport_;
};
} // namespace ledger

#endif

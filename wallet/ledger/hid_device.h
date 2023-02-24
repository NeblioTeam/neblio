#ifndef _LEDGER_HID_DEVICE
#define _LEDGER_HID_DEVICE 1

#include "comm.h"

#include <hidapi/hidapi_libusb.h>

namespace ledger {
class HID final : public Comm
{
public:
    Error              open() override;
    int                send(const std::vector<uint8_t>& data) override;
    int                recv(std::vector<uint8_t>& rdata) override;
    void               close() noexcept override;
    [[nodiscard]] bool is_open() const override;

private:
    static std::vector<std::string> enumerate_devices(unsigned short vendor_id = 0x2c97) noexcept;

    hid_device*    device_     = nullptr;
    std::string    path_       = {};
    bool           opened_     = false;
    const int      timeout_ms_ = 60 * 1000;
    unsigned short vendor_id_  = 0x2c97; // Ledger Vendor ID
};
} // namespace ledger

#endif

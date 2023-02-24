#ifndef _LEDGER_TRANSPORT
#define _LEDGER_TRANSPORT 1

#include "comm.h"

#include <memory>

namespace ledger {
class Transport
{
public:
    enum class TransportType : int
    {
        HID = 0,
    };

    Transport(TransportType type);
    Error open();
    std::tuple<ledger::Error, std::vector<uint8_t>>
         exchange(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const std::vector<uint8_t>& cdata);
    void close() noexcept;

private:
    int send(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const std::vector<uint8_t>& cdata);
    int recv(std::vector<uint8_t>& rdata);
    static std::vector<uint8_t> apdu_header(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                                            uint8_t lc);

    std::unique_ptr<Comm> comm_;
};
} // namespace ledger

#endif

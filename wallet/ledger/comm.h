#ifndef _LEDGER_COMM
#define _LEDGER_COMM 1

#include <string>
#include <vector>

#include "error.h"

namespace ledger {
class Comm
{
public:
    virtual ~Comm() = default;

    virtual Error              open()                                 = 0;
    virtual int                send(const std::vector<uint8_t>& data) = 0;
    virtual int                recv(std::vector<uint8_t>& rdata)      = 0;
    virtual void               close()                                = 0;
    [[nodiscard]] virtual bool is_open() const                        = 0;
};
} // namespace ledger

#endif

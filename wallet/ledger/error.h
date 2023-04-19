#ifndef LEDGER_ERROR_H
#define LEDGER_ERROR_H

#include <string>

namespace ledger {
enum class ErrorCode
{
    DEVICE_NOT_FOUND,
    DEVICE_OPEN_FAIL,
    DEVICE_DATA_SEND_FAIL,
    DEVICE_DATA_RECV_FAIL,
    APDU_INVALID_CMD,
    INVALID_TRUSTED_INPUT,
    UNRECOGNIZED_ERROR = 999,
};

class LedgerException : public std::exception
{
public:
    LedgerException(ErrorCode errorCodeIn);
    ~LedgerException() noexcept override = default;

    ErrorCode   GetErrorCode() const;
    std::string GetMessage() const;

    const char* what() const noexcept override;

private:
    ErrorCode   errorCode;
    std::string message;
};
} // namespace ledger

#endif // LEDGER_ERROR_H

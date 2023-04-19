#include "ledger/error.h"

namespace ledger {
std::string GetLedgerErrorCodeMessage(ErrorCode errorCode)
{
    switch (errorCode) {
    case ErrorCode::DEVICE_NOT_FOUND:
        return "Ledger Not Found";
    case ErrorCode::DEVICE_OPEN_FAIL:
        return "Failed to open Ledger";
    case ErrorCode::DEVICE_DATA_SEND_FAIL:
        return "Failed to send data to Ledger";
    case ErrorCode::DEVICE_DATA_RECV_FAIL:
        return "Failed to receive data from Ledger";
    case ErrorCode::APDU_INVALID_CMD:
        return "Invalid Ledger data";
    case ErrorCode::INVALID_TRUSTED_INPUT:
        return "Invalid trusted input";
    case ErrorCode::UNRECOGNIZED_ERROR:
    default:
        return "Unrecognized error";
    }
}

LedgerException::LedgerException(ErrorCode errorCodeIn)
    : errorCode(errorCodeIn), message(GetLedgerErrorCodeMessage(errorCodeIn))
{
}

ErrorCode   LedgerException::GetErrorCode() const { return errorCode; }
std::string LedgerException::GetMessage() const { return message; }

const char* LedgerException::what() const noexcept { return message.c_str(); }
} // namespace ledger

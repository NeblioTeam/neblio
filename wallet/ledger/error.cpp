#include "ledger/error.h"

namespace ledger
{
	LedgerException::LedgerException(ErrorCode errorCodeIn) : errorCode(errorCodeIn), message(GetMessage(errorCodeIn)) {}

	ErrorCode LedgerException::GetErrorCode() const { return errorCode; }
	std::string LedgerException::GetMessage() const { return message; }

	const char *LedgerException::what() const noexcept { return message.c_str(); }
}

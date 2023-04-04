#include "error.h"

namespace ledger
{	
	LedgerException::LedgerException(ErrorCode errorCodeIn) : errorCode(errorCodeIn) {}

	ErrorCode LedgerException::GetErrorCode() const { return errorCode; }
	
	const char *LedgerException::what() const noexcept {
		return GetMessage().c_str();
	}

	std::string LedgerException::GetMessage() const
	{
		return LedgerException::GetMessage(errorCode);
	}
}

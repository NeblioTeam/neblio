#pragma once

#include <string>

namespace ledger {
	enum class ErrorCode {
		DEVICE_NOT_FOUND,
		DEVICE_OPEN_FAIL,
		DEVICE_DATA_SEND_FAIL,
		DEVICE_DATA_RECV_FAIL,
		APDU_INVALID_CMD,
		INVALID_TRUSTED_INPUT,
		UNRECOGNIZED_ERROR = 999,
	};

	class LedgerException : public std::exception {
	public:
		static std::string GetMessage(ErrorCode errorCode)
		{
			switch (errorCode)
			{
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

		LedgerException(ErrorCode errorCodeIn);
		~LedgerException() noexcept override = default;

		ErrorCode GetErrorCode() const;
		std::string GetMessage() const;
		
		const char *what() const noexcept override;
	private:
		ErrorCode errorCode;
	};
} // namespace ledger

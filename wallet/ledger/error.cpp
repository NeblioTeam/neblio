#include "error.h"

namespace ledger
{
	std::string error_message(Error code)
	{
		switch (code)
		{
		case Error::SUCCESS:
			return "Ok";
		case Error::DEVICE_NOT_FOUND:
			return "Ledger Not Found";
		case Error::DEVICE_OPEN_FAIL:
			return "Failed to open Ledger";
		case Error::DEVICE_DATA_SEND_FAIL:
			return "Failed to send data to Ledger";
		case Error::DEVICE_DATA_RECV_FAIL:
			return "Failed to receive data from Ledger";
		case Error::APDU_INVALID_CMD:
			return "Invalid Ledger data";
		default:
			return "Unrecognized error";
		}
	}
}

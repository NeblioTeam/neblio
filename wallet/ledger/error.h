#pragma once

#include <string>

namespace ledger {
	enum class Error {
		SUCCESS	= 0,
		DEVICE_NOT_FOUND,
		DEVICE_OPEN_FAIL,
		DEVICE_DATA_SEND_FAIL,
		DEVICE_DATA_RECV_FAIL,
		APDU_INVALID_CMD,
	};

	std::string error_message(Error code);

} // namespace ledger

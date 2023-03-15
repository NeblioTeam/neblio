#pragma once

#include "bytes.h"
#include "comm.h"

#include <memory>

namespace ledger
{
	class Transport
	{
	public:
		enum class TransportType : int
		{
			HID = 0,
			SPECULOS = 1,
		};

		Transport(TransportType type);
		Error open();
		std::tuple<ledger::Error, bytes> exchange(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const bytes &cdata);
		void close() noexcept;

	private:
		int send(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const bytes &cdata);
		int recv(bytes &rdata);
		static bytes apdu_header(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, uint8_t lc);

		std::unique_ptr<Comm> comm_;
	};
} // namespace ledger

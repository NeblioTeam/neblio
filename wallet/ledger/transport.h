#pragma once

#include "ledger/bytes.h"
#include "ledger/comm.h"

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
		void open();
		bytes exchange(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const bytes &cdata);
		void close() noexcept;

	private:
		int send(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const bytes &cdata);
		int receive(bytes &rdata);
		static bytes apduHeader(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, uint8_t lc);

		std::unique_ptr<Comm> comm;
	};
} // namespace ledger

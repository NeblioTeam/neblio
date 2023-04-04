#include "error.h"
#include "hid.h"
#include "speculos.h"
#include "transport.h"
#include "utils.h"

namespace ledger
{
	Transport::Transport(TransportType type)
	{
		switch (type)
		{
		case TransportType::HID:
            comm = std::unique_ptr<HID>(new HID());
            break;
		case TransportType::SPECULOS:
            comm = std::unique_ptr<Speculos>(new Speculos());
			break;
		}
	}

	void Transport::open()
	{
        return comm->open();
	}

	bytes Transport::exchange(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const bytes &cdata)
	{
		int length = this->send(cla, ins, p1, p2, cdata);
		if (length < 0)
			throw LedgerException(ErrorCode::DEVICE_DATA_SEND_FAIL);

		bytes buffer;
		int sw = this->receive(buffer);
		if (sw < 0)
			throw LedgerException(ErrorCode::DEVICE_DATA_RECV_FAIL);

		if (sw != 0x9000)
			throw LedgerException(ErrorCode::APDU_INVALID_CMD);

		return buffer;
	}

	void Transport::close() noexcept
	{
        return comm->close();
	}

	int Transport::send(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const bytes &cdata)
	{
        if (!comm->isOpen())
			return -1;

		auto header = apduHeader(cla, ins, p1, p2, cdata.size());
		header.insert(header.end(), cdata.begin(), cdata.end());

        return comm->send(header);
	}

    int Transport::receive(bytes &rdata)
	{
        if (!comm->isOpen())
			return -1;

        return comm->receive(rdata);
	}

    bytes Transport::apduHeader(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, uint8_t lc)
	{
		return bytes{cla, ins, p1, p2, lc};
	}
} // namespace ledger

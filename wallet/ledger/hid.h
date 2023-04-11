#pragma once

#include "ledger/comm.h"

#include "hidapi/hidapi.h"

namespace ledger
{
	class HID final : public Comm
	{
	public:
		void open() override;
		int send(const bytes &data) override;
		int receive(bytes &rdata) override;
		void close() noexcept override;
		[[nodiscard]] bool isOpen() const override;

	private:
		static std::vector<std::string> enumerateDevices(unsigned short vendor_id = 0x2c97) noexcept;

		hid_device *device = nullptr;
		std::string path = {};
		bool opened = false;
		const int timeoutMs = 60 * 1000;
		unsigned short vendorId = 0x2c97; // Ledger Vendor ID
	};
} // namespace ledger

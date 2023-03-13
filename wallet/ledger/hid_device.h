#pragma once

#include "comm.h"

#include "hidapi/hidapi.h"

namespace ledger
{
	class HID final : public Comm
	{
	public:
		Error open() override;
		int send(const bytes &data) override;
		int recv(bytes &rdata) override;
		void close() noexcept override;
		[[nodiscard]] bool is_open() const override;

	private:
		static std::vector<std::string> enumerate_devices(unsigned short vendor_id = 0x2c97) noexcept;

		hid_device *device_ = nullptr;
		std::string path_ = {};
		bool opened_ = false;
		const int timeout_ms_ = 60 * 1000;
		unsigned short vendor_id_ = 0x2c97; // Ledger Vendor ID
	};
} // namespace ledger

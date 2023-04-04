#include "error.h"
#include "hid_device.h"
#include "utils.h"

#include <cassert>

namespace ledger
{
	void HID::open()
	{
		if (!opened)
		{
			auto devices = enumerateDevices(vendorId);
			if (devices.empty())
			{
				throw LedgerException(ErrorCode::DEVICE_NOT_FOUND);
			}

			path = devices.at(0);
			device = hid_open_path(path.c_str());
			if (!device)
			{
				throw LedgerException(ErrorCode::DEVICE_OPEN_FAIL);
			}

			hid_set_nonblocking(device, true);

			opened = true;
		}
	}

	int HID::send(const bytes &data)
	{
		if (data.empty())
			return -1;

		auto dataNew = utils::IntToBytes(data.size(), 2);
		dataNew.insert(dataNew.end(), data.begin(), data.end());

		size_t offset = 0;
		size_t seqIdx = 0;
		size_t length = 0;

		while (offset < dataNew.size())
		{
			// Header: channel (0x0101), tag (0x05), sequence index
			bytes header{0x01, 0x01, 0x05};

			auto seqIdxBytes = utils::IntToBytes(seqIdx, 2);
			header.insert(header.end(), seqIdxBytes.begin(), seqIdxBytes.end());

			bytes::iterator it;
			if (dataNew.size() - offset < 64 - header.size())
			{
				it = dataNew.end();
			}
			else
			{
				it = dataNew.begin() + offset + 64 - header.size();
			}

			bytes dataChunk{dataNew.begin() + offset, it};
			dataChunk.insert(dataChunk.begin(), header.begin(), header.end());
			dataChunk.insert(dataChunk.begin(), 0x00);

			if (hid_write(device, dataChunk.data(), dataChunk.size()) == -1)
				return -1;

			length += dataChunk.size();
			offset += 64 - header.size();
			seqIdx += 1;
		}

		return length;
	}

	int HID::receive(bytes &rdata)
	{
		int seqIdx = 0;
		uint8_t buf[64];

		hid_set_nonblocking(device, false);
		if (hid_read_timeout(device, buf, sizeof(buf), timeoutMs) <= 0)
			return -1;
		hid_set_nonblocking(device, true);

		bytes dataChunk(buf, buf + sizeof(buf));

		assert(dataChunk[0] == 0x01);
		assert(dataChunk[1] == 0x01);
		assert(dataChunk[2] == 0x05);

		auto seqIdxBytes = utils::IntToBytes(seqIdx, 2);
		assert(seqIdxBytes[0] == dataChunk[3]);
		assert(seqIdxBytes[1] == dataChunk[4]);

		auto dataLen = utils::BytesToInt(bytes(dataChunk.begin() + 5, dataChunk.begin() + 7));
		bytes data(dataChunk.begin() + 7, dataChunk.end());

		while (data.size() < dataLen)
		{
			uint8_t readBytes[64];
			if (hid_read_timeout(device, readBytes, sizeof(readBytes), 1000) == -1)
				return -1;
			bytes tmp(readBytes, readBytes + sizeof(readBytes));
			data.insert(data.end(), tmp.begin() + 5, tmp.end());
		}

		auto sw = utils::BytesToInt(bytes(data.begin() + dataLen - 2, data.begin() + dataLen));
		rdata = bytes(data.begin(), data.begin() + dataLen - 2);

		return sw;
	}

	void HID::close() noexcept
	{
		if (opened)
		{
			hid_close(device);
			opened = false;
		}
		hid_exit();
	}

	bool HID::isOpen() const
	{
		return opened;
	}

	std::vector<std::string> HID::enumerateDevices(unsigned short vendorId) noexcept
	{
		std::vector<std::string> devices;

		struct hid_device_info *devs, *curDev;

		devs = hid_enumerate(vendorId, 0x0);
		curDev = devs;
		while (curDev)
		{
			if (curDev->interface_number == 0 ||
				// MacOS specific
				curDev->usage_page == 0xffa0)
			{
				devices.emplace_back(curDev->path);
			}
			curDev = curDev->next;
		}
		hid_free_enumeration(devs);

		return devices;
	}
} // namespace ledger

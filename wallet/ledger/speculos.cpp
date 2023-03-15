#include "error.h"
#include "speculos.h"
#include "utils.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

namespace ledger
{
	Error Speculos::open()
	{
		if (!opened_)
		{
			auto _sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (_sockfd < 0)
				return Error::DEVICE_OPEN_FAIL;

			auto server = "localhost";
			auto port = "9999";

			struct sockaddr_in serv_addr;
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			serv_addr.sin_port = htons(9999);
			if (connect(_sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
				return Error::DEVICE_OPEN_FAIL;

			this->sockfd = _sockfd;
			opened_ = true;
		}

		return Error::SUCCESS;
	}

	int Speculos::send(const bytes &data)
	{
		bytes d;
		utils::AppendUint32(d, data.size());
		utils::AppendVector(d, data);
		auto result = write(sockfd, d.data(), d.size());
		if (result == -1)
			return -1;
		return result;
	}

	int Speculos::recv(bytes &rdata)
	{
		uint8_t lengthB[4];
		if (read(sockfd, lengthB, sizeof(lengthB)) == -1)
			return -1;

		auto length = utils::BytesToInt(bytes(lengthB, lengthB + sizeof(lengthB)));
		uint8_t d[length];
		if (read(sockfd, d, sizeof(d)) == -1)
			return -1;

		uint8_t sw[2];
		if (read(sockfd, sw, sizeof(sw)) == -1)
			return -1;

		rdata = bytes(d, d + sizeof(d));
		return utils::BytesToInt(bytes(sw, sw + sizeof(sw)));
	}

	void Speculos::close() noexcept
	{
		if (opened_)
		{
			::close(sockfd);
			opened_ = false;
		}
	}

	bool Speculos::is_open() const
	{
		return opened_;
	}

	std::vector<std::string> Speculos::enumerate_devices(unsigned short vendor_id) noexcept
	{
		std::vector<std::string> devices;

		struct hid_device_info *devs, *cur_dev;

		devs = hid_enumerate(vendor_id, 0x0);
		cur_dev = devs;
		while (cur_dev)
		{
			if (cur_dev->interface_number == 0 ||
				// MacOS specific
				cur_dev->usage_page == 0xffa0)
			{
				devices.emplace_back(cur_dev->path);
			}
			cur_dev = cur_dev->next;
		}
		hid_free_enumeration(devs);

		return devices;
	}
} // namespace ledger

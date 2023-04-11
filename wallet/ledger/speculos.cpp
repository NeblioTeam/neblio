#include "ledger/error.h"
#include "ledger/speculos.h"
#include "ledger/utils.h"

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
	void Speculos::open()
	{
		if (!opened)
		{
			auto _sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (_sockfd < 0)
				throw LedgerException(ErrorCode::DEVICE_OPEN_FAIL);

			auto server = "localhost";
			auto port = "9999";

			struct sockaddr_in serv_addr;
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			serv_addr.sin_port = htons(9999);
			if (connect(_sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
				throw LedgerException(ErrorCode::DEVICE_OPEN_FAIL);

			this->sockfd = _sockfd;
			opened = true;
		}
	}

	int Speculos::send(const bytes &data)
	{
		bytes d;
		AppendUint32(d, data.size());
		AppendVector(d, data);
		auto result = write(sockfd, d.data(), d.size());
		if (result == -1)
			return -1;
		return result;
	}

	int Speculos::receive(bytes &rdata)
	{
		uint8_t lengthB[4];
		if (read(sockfd, lengthB, sizeof(lengthB)) == -1)
			return -1;

		auto length = BytesToInt(bytes(lengthB, lengthB + sizeof(lengthB)));
		uint8_t d[length];
		if (read(sockfd, d, sizeof(d)) == -1)
			return -1;

		uint8_t sw[2];
		if (read(sockfd, sw, sizeof(sw)) == -1)
			return -1;

		rdata = bytes(d, d + sizeof(d));
		return BytesToInt(bytes(sw, sw + sizeof(sw)));
	}

	void Speculos::close() noexcept
	{
		if (opened)
		{
			::close(sockfd);
			opened = false;
		}
	}

	bool Speculos::isOpen() const
	{
		return opened;
	}
} // namespace ledger

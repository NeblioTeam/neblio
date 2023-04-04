#pragma once

#include <string>
#include <vector>

#include "bytes.h"

namespace ledger
{
	class Comm
	{
	public:
		virtual ~Comm() = default;

		virtual void open() = 0;
		virtual int send(const bytes &data) = 0;
		virtual int recv(bytes &rdata) = 0;
		virtual void close() = 0;
		[[nodiscard]] virtual bool is_open() const = 0;
	};
} // namespace ledger

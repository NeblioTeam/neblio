#ifndef LEDGER_COMM_H
#define LEDGER_COMM_H

#include <string>
#include <vector>

#include "ledger/bytes.h"

namespace ledger
{
	class Comm
	{
	public:
		virtual ~Comm() = default;

		virtual void open() = 0;
		virtual int send(const bytes &data) = 0;
		virtual int receive(bytes &rdata) = 0;
		virtual void close() = 0;
		[[nodiscard]] virtual bool isOpen() const = 0;
	};
} // namespace ledger

#endif // LEDGER_COMM_H

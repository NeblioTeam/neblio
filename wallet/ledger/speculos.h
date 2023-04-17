#ifndef LEDGER_SPECULOS_H
#define LEDGER_SPECULOS_H

#include "ledger/comm.h"

#include "hidapi/hidapi.h"

namespace ledger
{
	class Speculos final : public Comm
	{
	public:
		void open() override;
		int send(const bytes &data) override;
		int receive(bytes &rdata) override;
		void close() noexcept override;
		[[nodiscard]] bool isOpen() const override;

	private:
		int sockfd = -1;
		bool opened = false;
	};
} // namespace ledger

#endif // LEDGER_SPECULOS_H

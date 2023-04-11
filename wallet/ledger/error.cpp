#include "error.h"

#include <QString>
#include <QObject>

namespace ledger
{
	LedgerException::LedgerException(ErrorCode errorCodeIn) : errorCode(errorCodeIn) {}

	ErrorCode LedgerException::GetErrorCode() const { return errorCode; }

	const char *LedgerException::what() const noexcept {
		return GetMessage().c_str();
	}

	std::string LedgerException::GetMessage() const
	{
		return LedgerException::GetMessage(errorCode);
	}

	QString LedgerException::GetQtMessage() const
	{
		return QObject::tr("A Ledger error occured: %1\n\nIf you did not cancel the operation intentionally, make sure that your device is connected to the computer and the Neblio app is opened on the device.").arg(QString::fromStdString(GetMessage()));
	}
}

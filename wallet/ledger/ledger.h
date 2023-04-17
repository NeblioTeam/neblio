#ifndef LEDGER_LEDGER_H
#define LEDGER_LEDGER_H

#include "ledger/bip32.h"
#include "ledger/bytes.h"
#include "ledger/transport.h"
#include "ledger/tx.h"

namespace ledger
{
	class Ledger
	{
		enum APDU : uint8_t
		{
			CLA = 0xe0,
			INS_GET_APP_CONFIGURATION = 0x01,
			INS_GET_PUBLIC_KEY = 0x40,
			INS_SIGN = 0x03,
			INS_GET_TRUSTED_INPUT = 0x42,
			INS_UNTRUSTED_HASH_TRANSACTION_INPUT_START = 0x44,
			INS_UNTRUSTED_HASH_TRANSACTION_INPUT_FINALIZE = 0x4A,
			INS_UNTRUSTED_HASH_SIGN = 0x48
		};

	public:
		Ledger(Transport::TransportType transportType = Transport::TransportType::HID);
		~Ledger();

		void open();

		std::tuple<bytes, std::string, bytes> GetPublicKey(const Bip32Path path, bool confirm);
        std::vector<std::tuple<int, bytes>> SignTransaction(const Tx &tx, bool hasChange, const Bip32Path changePath,  const std::vector<Bip32Path> &signPaths, const std::vector<Utxo> &utxos);

		void close();

	private:
		std::unique_ptr<Transport> transport;

		bytes ProcessScriptBlocks(const bytes &script, uint32_t sequence);
		bytes GetTrustedInput(const Tx &utxoTx, uint32_t indexLookup);
		bytes GetTrustedInputRaw(bool firstRound, const bytes &data);
		void UntrustedHashTxInputFinalize(const Tx &tx, bool hasChange, const Bip32Path changePath);
		void UntrustedHashTxInputStart(const Tx &tx, const std::vector<TrustedInput> &trustedInputs, int inputIndex, bytes script, bool isNewTransaction);
		TrustedInput DeserializeTrustedInput(const bytes &serializedTrustedInput);
	};
}

#endif // LEDGER_LEDGER_H

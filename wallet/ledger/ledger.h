#pragma once

#include "bytes.h"
#include "transport.h"
#include "tx.h"

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

		Error open();

		std::tuple<bytes, std::string, bytes> GetPublicKey(const std::string &path, bool confirm);
		std::vector<std::tuple<int, bytes>> SignTransaction(const std::string &address, uint64_t amount, uint64_t fees, const std::string &changePath, const std::vector<std::string> &signPaths, const std::vector<std::tuple<bytes, uint32_t>> &rawUtxos, uint32_t locktime);

		void close();

	private:
		std::unique_ptr<Transport> transport_;

		std::tuple<Error, bytes> ProcessScriptBlocks(const bytes &script, uint32_t sequence);
		std::tuple<Error, bytes> GetTrustedInput(uint32_t indexLookup, Tx tx);
		std::tuple<Error, bytes> GetTrustedInput(uint32_t indexLookup, const bytes &serializedTransaction);
		std::tuple<Error, bytes> GetTrustedInputRaw(bool firstRound, uint32_t indexLookup, const bytes &data);
		void UntrustedHashTxInputFinalize(Tx tx, const std::string &changePath);
		void UntrustedHashTxInputStart(Tx tx, const std::vector<TrustedInput> &trustedInputs, int inputIndex, bytes script, bool isNewTransaction);
		TrustedInput DeserializeTrustedInput(const bytes &serializedTrustedInput);
	};
}

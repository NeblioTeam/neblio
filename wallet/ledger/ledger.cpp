#include "ledger.h"
#include "error.h"
#include "hash.h"
#include "utils.h"
#include "base58.h"
#include "bip32.h"
#include "tx.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace ledger
{
	Ledger::Ledger(Transport::TransportType transportType) { this->transport_ = std::unique_ptr<Transport>(new Transport(transportType)); }

	Ledger::~Ledger() { transport_->close(); }

	void Ledger::open()
	{
		std::cout << "Opening Ledger connection." << std::endl;
		auto openError = transport_->open();
		if (openError != ledger::Error::SUCCESS)
		{
			// TODO GK - what should we be throwing? (in the whole file)
			throw openError;
		}
		std::cout << "Ledger connection opened." << std::endl;
	}

	std::tuple<bytes, std::string, bytes> Ledger::GetPublicKey(const Bip32Path path, bool confirm)
	{
		auto payload = bytes();

		auto pathBytes = path.Serialize();
		payload.push_back(pathBytes.size() / 4);
		utils::AppendVector(payload, pathBytes);

		// 0x00 = P2_LEGACY (base58)
		auto result = transport_->exchange(APDU::CLA, APDU::INS_GET_PUBLIC_KEY, confirm, 0x00, payload);
		auto err = std::get<0>(result);
		auto buffer = std::get<1>(result);
		if (err != Error::SUCCESS)
			throw err;

		auto offset = 1;
		auto pubKeyLen = (int)buffer[offset] * 16 + 1;
		auto pubKey = utils::Splice(buffer, offset, pubKeyLen);
		offset += pubKeyLen;

		auto addressLen = (int)buffer[offset];
		offset++;
		auto address = utils::Splice(buffer, offset, addressLen);
		offset += addressLen;

		auto chainCode = utils::Splice(buffer, offset, 32);
		offset += 32;

		if (offset != buffer.size())
			throw Error::UNRECOGNIZED_ERROR;

		return {pubKey, std::string(address.begin(), address.end()), chainCode};
	}

	bytes Ledger::GetTrustedInputRaw(bool firstRound, const bytes &transactionData)
	{
		auto result = transport_->exchange(APDU::CLA, APDU::INS_GET_TRUSTED_INPUT, firstRound ? 0x00 : 0x80, 0x00, transactionData);
		auto err = std::get<0>(result);
		auto buffer = std::get<1>(result);
		if (err != Error::SUCCESS)
			throw err;

        return buffer;
	}

	bytes Ledger::GetTrustedInput(const Tx& utxoTx, uint32_t indexLookup)
	{
		bytes firstRoundData;
		utils::AppendUint32(firstRoundData, indexLookup);
		utils::AppendUint32(firstRoundData, utxoTx.version, true);
		utils::AppendUint32(firstRoundData, utxoTx.time, true);
    	utils::AppendVector(firstRoundData, utils::CreateVarint(utxoTx.inputs.size()));

		GetTrustedInputRaw(true, firstRoundData);

		for (auto input : utxoTx.inputs)
		{
			bytes inputData;
			utils::AppendVector(inputData, input.prevout.hash);
			utils::AppendUint32(inputData, input.prevout.index, true);
			utils::AppendVector(inputData, utils::CreateVarint(input.script.size()));

			GetTrustedInputRaw(false, inputData);

			bytes inputScriptData;
			utils::AppendVector(inputScriptData, input.script);
			utils::AppendUint32(inputScriptData, input.sequence);

			GetTrustedInputRaw(false, inputScriptData);
		}

		GetTrustedInputRaw(false, utils::CreateVarint(utxoTx.outputs.size()));

		for (auto output : utxoTx.outputs)
		{
			bytes outputData;
			utils::AppendUint64(outputData, output.amount, true);
			utils::AppendVector(outputData, utils::CreateVarint(output.script.size()));
			GetTrustedInputRaw(false, outputData);

			bytes outputScriptData;
			utils::AppendVector(outputScriptData, output.script);

			GetTrustedInputRaw(false, outputScriptData);
		}

		return GetTrustedInputRaw(false, utils::IntToBytes(utxoTx.locktime, 4));
	}

	void Ledger::UntrustedHashTxInputFinalize(const Tx &tx, bool hasChange, const Bip32Path changePath)
	{
		auto ins = APDU::INS_UNTRUSTED_HASH_TRANSACTION_INPUT_FINALIZE;
		auto p2 = 0x00;

		auto p1 = 0xFF;
		if (hasChange)
		{
			auto serializedChangePath = changePath.Serialize();

			bytes changePathData;
			changePathData.push_back(serializedChangePath.size() / 4);
			utils::AppendVector(changePathData, serializedChangePath);

			auto result = transport_->exchange(APDU::CLA, ins, p1, p2, changePathData);
			auto err = std::get<0>(result);
			auto buffer = std::get<1>(result);
			if (err != Error::SUCCESS)
				throw err;
		}
		else
		{
			auto result = transport_->exchange(APDU::CLA, ins, p1, p2, {0x00});
			auto err = std::get<0>(result);
			auto buffer = std::get<1>(result);
			if (err != Error::SUCCESS)
				throw err;
		}

		p1 = 0x00;
		auto result = transport_->exchange(APDU::CLA, ins, p1, p2, utils::CreateVarint(tx.outputs.size()));
		auto err = std::get<0>(result);
		auto buffer = std::get<1>(result);
		if (err != Error::SUCCESS)
			throw err;

		for (auto i = 0; i < tx.outputs.size(); i++)
		{
			p1 = i < tx.outputs.size() - 1 ? 0x00 : 0x80;

			auto output = tx.outputs[i];
			bytes outputData;
			utils::AppendUint64(outputData, output.amount, true);
			utils::AppendVector(outputData, utils::CreateVarint(output.script.size()));
			utils::AppendVector(outputData, output.script);

			auto result = transport_->exchange(APDU::CLA, ins, p1, p2, outputData);
			auto err = std::get<0>(result);
			auto buffer = std::get<1>(result);
			if (err != Error::SUCCESS)
				throw err;
		}
	}

	void Ledger::UntrustedHashTxInputStart(const Tx &tx, const std::vector<TrustedInput> &trustedInputs, int inputIndex, bytes script, bool isNewTransaction)
	{
		auto ins = APDU::INS_UNTRUSTED_HASH_TRANSACTION_INPUT_START;
		auto p1 = 0x00;
		auto p2 = isNewTransaction ? 0x00 : 0x80;

		bytes data;
		utils::AppendUint32(data, tx.version, true);
		utils::AppendUint32(data, tx.time, true);
		utils::AppendVector(data, utils::CreateVarint(trustedInputs.size()));

		auto result = transport_->exchange(APDU::CLA, ins, p1, p2, data);
		auto err = std::get<0>(result);
		auto buffer = std::get<1>(result);
		if (err != Error::SUCCESS)
			throw err;

		p1 = 0x80;
		for (auto i = 0; i < trustedInputs.size(); i++)
		{
			auto trustedInput = trustedInputs[i];
			auto _script = i == inputIndex ? script : bytes();

			bytes _data;
			_data.push_back(0x01);
			_data.push_back(trustedInput.serialized.size());
			utils::AppendVector(_data, trustedInput.serialized);
			utils::AppendVector(_data, utils::CreateVarint(_script.size()));

			auto result = transport_->exchange(APDU::CLA, ins, p1, p2, _data);
			auto err = std::get<0>(result);
			auto buffer = std::get<1>(result);
			if (err != Error::SUCCESS)
				throw err;

			bytes scriptData;
			utils::AppendVector(scriptData, _script);
			utils::AppendUint32(scriptData, 0xffffffff, true);

			result = transport_->exchange(APDU::CLA, ins, p1, p2, scriptData);
			err = std::get<0>(result);
			buffer = std::get<1>(result);
			if (err != Error::SUCCESS)
				throw err;
		}
	}

    std::vector<std::tuple<int, bytes>> Ledger::SignTransaction(const Tx &tx, bool hasChange, const Bip32Path changePath, const std::vector<Bip32Path>& signPaths, const std::vector<Utxo> &utxos)
	{
		assert(tx.inputs.size() == signPaths.size());
		assert(tx.inputs.size() == utxos.size());

		// get trusted inputs
		std::vector<TrustedInput> trustedInputs;
		for (auto i = 0; i < utxos.size(); i++)
		{
			const auto &utxo = utxos[i];

			const auto serializedTrustedInput = GetTrustedInput(utxo.tx, utxo.outputIndex);
			const auto trustedInput = ledger::DeserializeTrustedInput(serializedTrustedInput);

            assert(trustedInput.prevTxId == tx.inputs[i].prevout.hash);

			trustedInputs.push_back(trustedInput);
		}

        std::vector<std::tuple<int, bytes>> signatures;
		for (auto i = 0; i < tx.inputs.size(); i++)
		{
			auto &script = utxos[i].tx.outputs[utxos[i].outputIndex].script;
            UntrustedHashTxInputStart(tx, trustedInputs, i, script, i == 0);

        	UntrustedHashTxInputFinalize(tx, hasChange, changePath);

			auto ins = INS_UNTRUSTED_HASH_SIGN;
			auto p1 = 0x00;
			auto p2 = 0x00;

			auto serializedSignPath = signPaths[i].Serialize();

			bytes data;
			data.push_back(serializedSignPath.size() / 4);
			utils::AppendVector(data, serializedSignPath);
			data.push_back(0x00);
			utils::AppendUint32(data, tx.locktime);
			data.push_back(0x01);

			auto result = transport_->exchange(APDU::CLA, ins, p1, p2, data);
			auto err = std::get<0>(result);
			auto buffer = std::get<1>(result);
			if (err != Error::SUCCESS)
				throw err;

			if (buffer[0] & 0x01)
			{
				bytes data;
				data.push_back(0x30);
				utils::AppendVector(data, bytes(buffer.begin() + 1, buffer.end()));
				signatures.push_back({1, data});
			}
			else
			{
				signatures.push_back({0, buffer});
			}
		}

		return signatures;
	}

	void Ledger::close() { return transport_->close(); }
} // namespace ledger

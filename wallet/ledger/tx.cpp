#include "error.h"
#include "tx.h"
#include "utils.h"

namespace ledger
{
bytes SerializeTransaction(const Tx& tx) { 
    bytes serializedTransaction;
    AppendUint32(serializedTransaction, tx.version, true);
    AppendUint32(serializedTransaction, tx.time, true);

    AppendVector(serializedTransaction, CreateVarint(tx.inputs.size()));
    for (auto input : tx.inputs)
    {
        AppendVector(serializedTransaction, input.prevout.hash);
        AppendUint32(serializedTransaction, input.prevout.index, true);
        AppendVector(serializedTransaction, CreateVarint(input.script.size()));
        AppendVector(serializedTransaction, input.script);
        AppendUint32(serializedTransaction, input.sequence);
    }

    AppendVector(serializedTransaction, CreateVarint(tx.outputs.size()));
    for (auto output : tx.outputs)
    {
        AppendUint64(serializedTransaction, output.amount, true);
        AppendVector(serializedTransaction, CreateVarint(output.script.size()));
        AppendVector(serializedTransaction, output.script);
    }

    AppendUint32(serializedTransaction, tx.locktime);
    return serializedTransaction;
}

Tx DeserializeTransaction(const bytes& transaction)
{
    Tx tx;
    tx.inputs  = std::vector<TxInput>();
    tx.outputs = std::vector<TxOutput>();

    auto offset = 0;

    tx.version = BytesToInt(Splice(transaction, offset, 4), true);
    offset += 4;

    tx.time = BytesToInt(Splice(transaction, offset, 4), true);
    offset += 4;

    auto varint      = DeserializeVarint(transaction, offset);
    auto inputsCount = std::get<0>(varint);
    offset += std::get<1>(varint);

    auto flags = 0;
    if (inputsCount == 0) {
        flags = BytesToInt(Splice(transaction, offset, 1));
        offset += 1;

        varint      = DeserializeVarint(transaction, offset);
        inputsCount = std::get<0>(varint);
        offset += std::get<1>(varint);
    }

    for (auto i = 0; i < inputsCount; i++) {
        TxInput input;

        TxPrevout prevout;
        prevout.hash = Splice(transaction, offset, 32);
        offset += 32;
        prevout.index = BytesToInt(Splice(transaction, offset, 4));
        offset += 4;

        input.prevout = prevout;

        varint = DeserializeVarint(transaction, offset);
        offset += std::get<1>(varint);
        input.script = Splice(transaction, offset, std::get<0>(varint));

        offset += std::get<0>(varint);
        input.sequence = BytesToInt(Splice(transaction, offset, 4));
        offset += 4;

        tx.inputs.push_back(input);
    }

    varint             = DeserializeVarint(transaction, offset);
    auto numberOutputs = std::get<0>(varint);
    offset += std::get<1>(varint);

    for (auto i = 0; i < numberOutputs; i++) {
        TxOutput output;

        output.amount = BytesToUint64(Splice(transaction, offset, 8), true);
        offset += 8;

        varint = DeserializeVarint(transaction, offset);
        offset += std::get<1>(varint);

        output.script = Splice(transaction, offset, std::get<0>(varint));
        offset += std::get<0>(varint);

        tx.outputs.push_back(output);
    }

    if (flags != 0) {
        TxWitness txWitness;
        for (auto i = 0; i < inputsCount; i++) {
            auto numberOfWitnesses = DeserializeVarint(transaction, offset);
            offset += std::get<1>(numberOfWitnesses);

            TxInWitness   txInWitness;
            ScriptWitness scriptWitness;
            for (auto j = 0; j < std::get<0>(numberOfWitnesses); j++) {
                auto scriptWitnessSize = DeserializeVarint(transaction, offset);
                offset += std::get<1>(scriptWitnessSize);
                scriptWitness.stack.push_back(
                    bytes(transaction.begin() + offset,
                          transaction.begin() + offset + std::get<0>(scriptWitnessSize)));
                offset += std::get<0>(scriptWitnessSize);
            }

            txInWitness.scriptWitness = scriptWitness;
            txWitness.txInWitnesses.push_back(txInWitness);
        }
    }

    tx.locktime = BytesToInt(Splice(transaction, offset, 4));

    return tx;
    }

    TrustedInput DeserializeTrustedInput(const bytes &serializedTrustedInput)
    {
        TrustedInput trustedInput;

        AppendVector(trustedInput.serialized, serializedTrustedInput);

        auto offset = 0;

        auto trustedInputMagic = serializedTrustedInput[offset];
        if (trustedInputMagic != 0x32)
            throw LedgerException(ErrorCode::INVALID_TRUSTED_INPUT);
        offset += 1;

        auto zeroByte = serializedTrustedInput[offset];
        if (zeroByte != 0x00)
            throw LedgerException(ErrorCode::INVALID_TRUSTED_INPUT);
        offset += 1;

        trustedInput.random = BytesToInt(Splice(serializedTrustedInput, offset, 2));
        offset += 2;

        trustedInput.prevTxId = Splice(serializedTrustedInput, offset, 32);
        offset += 32;

        trustedInput.outIndex = BytesToInt(Splice(serializedTrustedInput, offset, 4), true);
        offset += 4;

        trustedInput.amount = BytesToInt(Splice(serializedTrustedInput, offset, 8), true);
        offset += 8;

        trustedInput.hmac = Splice(serializedTrustedInput, offset, 8);
        offset += 8;

        if (offset != serializedTrustedInput.size())
            throw LedgerException(ErrorCode::INVALID_TRUSTED_INPUT);

        return trustedInput;
    }
} // namespace ledger

#include "tx.h"
#include "utils.h"

namespace ledger
{
bytes SerializeTransaction(const Tx& tx) { 
    bytes serializedTransaction;
    utils::AppendUint32(serializedTransaction, tx.version, true);
    utils::AppendUint32(serializedTransaction, tx.time, true);

    utils::AppendVector(serializedTransaction, utils::CreateVarint(tx.inputs.size()));
    for (auto input : tx.inputs)
    {
        utils::AppendVector(serializedTransaction, input.prevout.hash);
        utils::AppendUint32(serializedTransaction, input.prevout.index, true);
        utils::AppendVector(serializedTransaction, utils::CreateVarint(input.script.size()));
        utils::AppendVector(serializedTransaction, input.script);
        utils::AppendUint32(serializedTransaction, input.sequence);
    }

    utils::AppendVector(serializedTransaction, utils::CreateVarint(tx.outputs.size()));
    for (auto output : tx.outputs)
    {
        utils::AppendUint64(serializedTransaction, output.amount, true);
        utils::AppendVector(serializedTransaction, utils::CreateVarint(output.script.size()));
        utils::AppendVector(serializedTransaction, output.script);
    }

    utils::AppendUint32(serializedTransaction, tx.locktime);
    return serializedTransaction;
}

Tx DeserializeTransaction(const bytes& transaction)
{
    Tx tx;
    tx.inputs  = std::vector<TxInput>();
    tx.outputs = std::vector<TxOutput>();

    auto offset = 0;

    tx.version = utils::BytesToInt(utils::Splice(transaction, offset, 4), true);
    offset += 4;

    tx.time = utils::BytesToInt(utils::Splice(transaction, offset, 4), true);
    offset += 4;

    auto varint      = utils::DeserializeVarint(transaction, offset);
    auto inputsCount = std::get<0>(varint);
    offset += std::get<1>(varint);

    auto flags = 0;
    if (inputsCount == 0) {
        flags = utils::BytesToInt(utils::Splice(transaction, offset, 1));
        offset += 1;

        varint      = utils::DeserializeVarint(transaction, offset);
        inputsCount = std::get<0>(varint);
        offset += std::get<1>(varint);
    }

    for (auto i = 0; i < inputsCount; i++) {
        TxInput input;

        TxPrevout prevout;
        prevout.hash = utils::Splice(transaction, offset, 32);
        offset += 32;
        prevout.index = utils::BytesToInt(utils::Splice(transaction, offset, 4));
        offset += 4;

        input.prevout = prevout;

        varint = utils::DeserializeVarint(transaction, offset);
        offset += std::get<1>(varint);
        input.script = utils::Splice(transaction, offset, std::get<0>(varint));

        offset += std::get<0>(varint);
        input.sequence = utils::BytesToInt(utils::Splice(transaction, offset, 4));
        offset += 4;

        tx.inputs.push_back(input);
    }

    varint             = utils::DeserializeVarint(transaction, offset);
    auto numberOutputs = std::get<0>(varint);
    offset += std::get<1>(varint);

    for (auto i = 0; i < numberOutputs; i++) {
        TxOutput output;

        output.amount = utils::BytesToUint64(utils::Splice(transaction, offset, 8), true);
        offset += 8;

        varint = utils::DeserializeVarint(transaction, offset);
        offset += std::get<1>(varint);

        output.script = utils::Splice(transaction, offset, std::get<0>(varint));
        offset += std::get<0>(varint);

        tx.outputs.push_back(output);
    }

    if (flags != 0) {
        TxWitness txWitness;
        for (auto i = 0; i < inputsCount; i++) {
            auto numberOfWitnesses = utils::DeserializeVarint(transaction, offset);
            offset += std::get<1>(numberOfWitnesses);

            TxInWitness   txInWitness;
            ScriptWitness scriptWitness;
            for (auto j = 0; j < std::get<0>(numberOfWitnesses); j++) {
                auto scriptWitnessSize = utils::DeserializeVarint(transaction, offset);
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

    tx.locktime = utils::BytesToInt(utils::Splice(transaction, offset, 4));

    return tx;
    }

    TrustedInput DeserializeTrustedInput(const bytes &serializedTrustedInput)
    {
        TrustedInput trustedInput;

        // TODO GK - direct assignment ok?
        utils::AppendVector(trustedInput.serialized, serializedTrustedInput);

        auto offset = 0;

        auto trustedInputMagic = serializedTrustedInput[offset];
        if (trustedInputMagic != 0x32)
            throw "Invalid trusted input magic";
        offset += 1;

        auto zeroByte = serializedTrustedInput[offset];
        if (zeroByte != 0x00)
            throw "Zero byte is not a zero byte";
        offset += 1;

        trustedInput.random = utils::BytesToInt(utils::Splice(serializedTrustedInput, offset, 2));
        offset += 2;

        trustedInput.prevTxId = utils::Splice(serializedTrustedInput, offset, 32);
        offset += 32;

        trustedInput.outIndex = utils::BytesToInt(utils::Splice(serializedTrustedInput, offset, 4), true);
        offset += 4;

        trustedInput.amount = utils::BytesToInt(utils::Splice(serializedTrustedInput, offset, 8), true);
        offset += 8;

        trustedInput.hmac = utils::Splice(serializedTrustedInput, offset, 8);
        offset += 8;

        if (offset != serializedTrustedInput.size())
            throw "Leftover bytes in trusted input";

        return trustedInput;
    }
} // namespace ledger

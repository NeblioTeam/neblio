#ifndef LEDGER_TX_H
#define LEDGER_TX_H

#include "ledger/bytes.h"
#include "ledger/transport.h"

namespace ledger {
struct TxPrevout
{
    bytes    hash;
    uint32_t index;
};

struct TxInput
{
    TxPrevout prevout;
    bytes     script;
    uint32_t  sequence;
};

struct TxOutput
{
    uint64_t amount;
    bytes    script;
};

struct ScriptWitness
{
    std::vector<bytes> stack;
};

struct TxInWitness
{
    ScriptWitness scriptWitness;
};

struct TxWitness
{
    std::vector<TxInWitness> txInWitnesses;
};

struct Tx
{
    uint32_t              version;
    uint32_t              time;
    std::vector<TxInput>  inputs;
    std::vector<TxOutput> outputs;
    uint32_t              locktime;
    TxWitness             witness;
};

struct TrustedInput
{
    bytes    serialized;
    uint16_t random;
    bytes    prevTxId;
    uint32_t outIndex;
    uint64_t amount;
    bytes    hmac;
};

struct Utxo
{
    Tx       tx;
    uint32_t outputIndex;
};

bytes        SerializeTransaction(const Tx& tx);
Tx           DeserializeTransaction(const bytes& transaction);
TrustedInput DeserializeTrustedInput(const bytes& serializedTrustedInput);
} // namespace ledger

#endif // LEDGER_TX_H

#include "ledger/ledger.h"
#include "ledger/tx.h"
#include "ledger/utils.h"
#include "ledgerBridge.h"

#include <string>
#include <stdexcept>

namespace ledgerbridge
{
    LedgerBridge::LedgerBridge() {}

    LedgerBridge::~LedgerBridge() {}

    void LedgerBridge::SignTransaction(CWalletTx &wtxNew, const std::vector<LedgerBridgeUtxo> &utxos) {
        // TODO GK - proper paths
        std::string changePath = ledger::utils::GetBip32Path(0, 0);
        std::vector<std::string> sigPaths = {changePath};

        // transform wallet tx
        ledger::Tx tx = ToLedgerTx(wtxNew);
        // transform UTxOs
        std::vector<ledger::Utxo> ledgerUtxos;
        for (const auto &utxo : utxos){
            ledgerUtxos.push_back({ToLedgerTx(utxo.transaction), utxo.outputIndex});
        }

        ledger::Ledger ledger(ledger::Transport::TransportType::SPECULOS);
        ledger.open();

        // sign tx
        auto signTxResults = ledger.SignTransaction(tx, changePath, sigPaths, ledgerUtxos);

        // add signatures to tx and verify
        for (auto sigIndex = 0; sigIndex < signTxResults.size(); sigIndex++) {
            auto signature = std::get<1>(signTxResults[sigIndex]);

            auto pubKeyResult = ledger.GetPublicKey(sigPaths[sigIndex], false);
            auto pubKey = CPubKey(ledger::utils::CompressPubKey(std::get<0>(pubKeyResult)));

            // hash type
            signature.push_back(0x01);

            auto txIn = &wtxNew.vin[sigIndex];
            txIn->scriptSig << signature;
            txIn->scriptSig << pubKey;

            if (!VerifyScript(txIn->scriptSig, utxos[sigIndex].outputPubKey, wtxNew, sigIndex, true, false, 0).isOk()) {
                throw std::exception();
            }
        }

        ledger.close();
    }

    ledger::Tx LedgerBridge::ToLedgerTx(const CTransaction &tx)
    {
        ledger::Tx ledgerTx;
        ledgerTx.version = tx.nVersion;
        ledgerTx.time = tx.nTime;

        for (auto i = 0; i < tx.vin.size();i++){
            const auto& input = tx.vin[i];

            ledger::TxPrevout prevout = {
                .hash = ledger::bytes(input.prevout.hash.begin(), input.prevout.hash.end()),
                .index = input.prevout.n
            };

            ledgerTx.inputs.push_back({
                prevout,
                .script = input.scriptSig,
                .sequence = input.nSequence
            });
        }

        for (const auto& output: tx.vout) {
            ledgerTx.outputs.push_back({
                .amount = (uint64_t) output.nValue,
                .script = output.scriptPubKey
            });
        }

        ledgerTx.locktime = tx.nLockTime;

        return ledgerTx;
    }
}

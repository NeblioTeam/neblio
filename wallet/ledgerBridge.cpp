#include "ledger/bip32.h"
#include "ledger/ledger.h"
#include "ledger/tx.h"
#include "ledger/utils.h"
#include "ledgerBridge.h"
#include "key.h"
#include "script.h"

#include <string>
#include <stdexcept>
#include <vector>

namespace ledgerbridge
{
    LedgerBridge::LedgerBridge() {}

    LedgerBridge::~LedgerBridge() {}

    void LedgerBridge::SignTransaction(const ITxDB& txdb, const CWallet& wallet, CWalletTx &wtxNew, const std::vector<LedgerBridgeUtxo> &utxos) 
    {
        std::vector<std::string> signaturePaths;

        // transform wallet tx to ledger tx
        ledger::Tx tx = ToLedgerTx(wtxNew);        

        // transform UTxOs and build signature paths
        std::vector<ledger::Utxo> ledgerUtxos;
        for (const auto &utxo : utxos)
        {
            ledgerUtxos.push_back({ToLedgerTx(utxo.transaction), utxo.outputIndex});

            CKeyID keyID;
            if (!ExtractKeyID(txdb, utxo.outputPubKey, keyID)) {
                throw "Invalid key in script";
            }

            CLedgerKey ledgerKey;
            auto keyFound = wallet.GetLedgerKey(keyID, ledgerKey);
            if (!keyFound) {
                throw "Ledger key was not found in wallet";
            }

            signaturePaths.push_back(ledger::bip32::GetBip32Path(ledgerKey.account, ledgerKey.index));
        }

        auto changePath = signaturePaths[0];

        ledger::Ledger ledger(ledger::Transport::TransportType::SPECULOS);
        ledger.open();

        // sign tx
        auto signTxResults = ledger.SignTransaction(tx, changePath, signaturePaths, ledgerUtxos);

        // add signatures to tx and verify
        for (auto sigIndex = 0; sigIndex < signTxResults.size(); sigIndex++) {
            auto signature = std::get<1>(signTxResults[sigIndex]);
            
            auto pubKeyResult = ledger.GetPublicKey(signaturePaths[sigIndex], false);
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
        
        for (const auto& input : tx.vin){            
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

        for (const auto& output : tx.vout) {
            ledgerTx.outputs.push_back({
                .amount = (uint64_t) output.nValue,
                .script = output.scriptPubKey
            });
        }

        ledgerTx.locktime = tx.nLockTime;

        return ledgerTx;
    }
}

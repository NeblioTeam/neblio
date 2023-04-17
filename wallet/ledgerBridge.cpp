#include "ledgerBridge.h"
#include "key.h"
#include "ledger/ledger.h"
#include "ledger/utils.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ledgerbridge {
const ledger::Transport::TransportType TRANSPORT_TYPE = ledger::Transport::TransportType::HID;

LedgerBridge::LedgerBridge() {}

LedgerBridge::~LedgerBridge() {}

ledger::bytes LedgerBridge::GetPublicKey(ledger::Ledger& ledger, const ledger::Bip32Path path,
                                         bool display)
{
    auto result = ledger.GetPublicKey(path, display);

    return ledger::CompressPubKey(std::get<0>(result));
}

ledger::bytes LedgerBridge::GetPublicKey(const ledger::Bip32Path path, bool display)
{
    ledger::Ledger ledger(TRANSPORT_TYPE);
    ledger.open();

    return GetPublicKey(ledger, path, display);
}

ledger::bytes LedgerBridge::GetPublicKey(int account, bool isChange, int index, bool display)
{
    return GetPublicKey(ledger::Bip32Path(account, isChange, index), display);
}

ledger::bytes LedgerBridge::GetAccountPublicKey(int account, bool display)
{
    return GetPublicKey(ledger::Bip32Path(account), display);
}

void LedgerBridge::SignTransaction(const ITxDB& txdb, const CWallet& wallet, CWalletTx& wtxNew,
                                   const std::vector<LedgerBridgeUtxo>& utxos, bool hasChange)
{
    std::vector<ledger::Bip32Path> signaturePaths;

    // transform wallet tx to ledger tx
    ledger::Tx tx = ToLedgerTx(wtxNew);

    // transform UTxOs and build signature paths
    std::vector<ledger::Utxo> ledgerUtxos;
    for (const auto& utxo : utxos) {
        ledgerUtxos.push_back({ToLedgerTx(utxo.transaction), utxo.outputIndex});

        CKeyID keyID;
        if (!ExtractKeyID(txdb, utxo.outputPubKey, keyID)) {
            throw "Invalid key in script";
        }

        CLedgerKey ledgerKey;
        auto       keyFound = wallet.GetLedgerKey(keyID, ledgerKey);
        if (!keyFound) {
            throw "Ledger key was not found in wallet";
        }

        signaturePaths.push_back(
            ledger::Bip32Path(ledgerKey.account, ledgerKey.isChange, ledgerKey.index));
    }

    // We only support transaction from a single Ledger address for now so all the signature paths
    // should be the same. Even if that was to change this would still be a valid approach
    // to determining the change path. If users would like to use a different change path
    // they could leverage the coin control feature.
    auto changePath = signaturePaths[0].ToChangePath();

    ledger::Ledger ledger(TRANSPORT_TYPE);
    ledger.open();

    // sign tx
    auto signTxResults = ledger.SignTransaction(tx, hasChange, changePath, signaturePaths, ledgerUtxos);

    // add signatures to tx and verify
    for (auto sigIndex = 0; sigIndex < signTxResults.size(); sigIndex++) {
        auto signature = signTxResults[sigIndex];

        auto pubKey = CPubKey(GetPublicKey(ledger, signaturePaths[sigIndex], false));

        // hash type
        signature.push_back(0x01);

        auto txIn = &wtxNew.vin[sigIndex];
        txIn->scriptSig << signature;
        txIn->scriptSig << pubKey;

        if (!VerifyScript(txIn->scriptSig, utxos[sigIndex].outputPubKey, wtxNew, sigIndex, true, false,
                          0)
                 .isOk()) {
            throw std::exception();
        }
    }
}

ledger::Tx LedgerBridge::ToLedgerTx(const CTransaction& tx)
{
    ledger::Tx ledgerTx;
    ledgerTx.version = tx.nVersion;
    ledgerTx.time    = tx.nTime;

    for (const auto& input : tx.vin) {
        ledger::TxPrevout prevout = {
            .hash  = ledger::bytes(input.prevout.hash.begin(), input.prevout.hash.end()),
            .index = input.prevout.n};

        ledgerTx.inputs.push_back({prevout, .script = input.scriptSig, .sequence = input.nSequence});
    }

    for (const auto& output : tx.vout) {
        ledgerTx.outputs.push_back({.amount = (uint64_t)output.nValue, .script = output.scriptPubKey});
    }

    ledgerTx.locktime = tx.nLockTime;

    return ledgerTx;
}
} // namespace ledgerbridge

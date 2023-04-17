#ifndef LEDGERBRIDGE_H
#define LEDGERBRIDGE_H

#include "ledger/bip32.h"
#include "ledger/tx.h"
#include "ledger/bytes.h"
#include "ledger/ledger.h"
#include "ledger/utils.h"

#include "itxdb.h"
#include "wallet.h"
#include "script.h"

#include <vector>

namespace ledgerbridge
{
    struct LedgerBridgeUtxo
    {
        CTransaction transaction;
        uint32_t outputIndex;
        CScript outputPubKey;
    };

    class LedgerBridge
    {
        public:
            static bool ValidateAccountIndex(int accountIndex)
            {
                return 0 <= accountIndex && accountIndex <= ledger::MAX_RECOMMENDED_ACCOUNT;
            }

            static bool ValidateAddressIndex(int addressIndex)
            {
                return 0 <= addressIndex && addressIndex <= ledger::MAX_RECOMMENDED_INDEX;
            }

            LedgerBridge();
            ~LedgerBridge();

            ledger::bytes GetPublicKey(ledger::Ledger& ledger, const ledger::Bip32Path path, bool display);
            ledger::bytes GetPublicKey(const ledger::Bip32Path path, bool display);
            ledger::bytes GetPublicKey(int account, bool isChange, int index, bool display);
            ledger::bytes GetAccountPublicKey(int account, bool display);
            void SignTransaction(const ITxDB& txdb, const CWallet& wallet, CWalletTx &wtxNew, const std::vector<LedgerBridgeUtxo> &utxos, bool hasChange);
        private:
            ledger::Tx ToLedgerTx(const CTransaction& tx);
    };
}

#endif // LEDGERBRIDGE_H

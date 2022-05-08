#include "wallet_interface.h"

#include "transaction.h"
#include "txdb.h"
#include "wallet.h"

std::shared_ptr<CWallet> pwalletMain;

// notify wallets about a new best chain
void SetBestChain(const CBlockLocator& loc)
{
    if (pwalletMain)
        pwalletMain->SetBestChain(loc);
}

// notify wallets about an updated transaction
void UpdatedTransaction(const uint256& hashTx)
{
    if (pwalletMain)
        pwalletMain->UpdatedTransaction(hashTx);
}

// dump all wallets
void PrintWallets(const CBlock& block)
{
    if (pwalletMain)
        pwalletMain->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
{
    if (pwalletMain)
        pwalletMain->ResendWalletTransactions(CTxDB(), fForce);
}

bool IsFromMe(const CTransaction& tx)
{
    if (pwalletMain)
        return pwalletMain->IsFromMe(tx);
    return false;
}

bool GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    if (pwalletMain)
        return pwalletMain->GetTransaction(hashTx, wtx);
    return false;
}

void EraseFromWallets(uint256 hash)
{
    if (pwalletMain)
        pwalletMain->EraseFromWallet(hash);
}

void Inventory(const uint256& hash)
{
    if (pwalletMain)
        pwalletMain->Inventory(hash);
}

// make sure all wallets know about the given transaction, in the given block
void SyncWithWallets(const ITxDB& txdb, const CTransaction& tx, const CBlock* pblock)
{
    if (!pwalletMain)
        return;

    // update NTP1 transactions
    if (pwalletMain && pwalletMain->walletNewTxUpdateFunctor) {
        pwalletMain->walletNewTxUpdateFunctor->run(tx.GetHash(), txdb.GetBestChainHeight());
    }

    pwalletMain->SyncTransaction(txdb, tx, pblock);
}

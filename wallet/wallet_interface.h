#ifndef WALLET_INTERFACE_H
#define WALLET_INTERFACE_H

#include "uint256.h"
#include <memory>
#include <boost/enable_shared_from_this.hpp>

class CWallet;
class CBlockLocator;
class CWalletTx;
class CTransaction;
class CBlock;
class ITxDB;

extern std::shared_ptr<CWallet> pwalletMain;

// check whether the passed transaction is from us
bool IsFromMe(const CTransaction& tx);

// get the wallet transaction with the given hash (if it exists)
bool GetTransaction(const uint256& hashTx, CWalletTx& wtx);

// erases transaction with the given hash from all wallets
void EraseFromWallets(uint256 hash);

void PrintWallets(const CBlock& block);

void SetBestChain(const CBlockLocator& loc);
void UpdatedTransaction(const uint256& hashTx);
void ResendWalletTransactions(bool fForce = false);

void Inventory(const uint256& hash);

void SyncWithWallets(const ITxDB& txdb, const CTransaction& tx, const CBlock* pblock = nullptr);

class WalletNewTxUpdateFunctor : public boost::enable_shared_from_this<WalletNewTxUpdateFunctor>
{
    // reload balances if the current height less than the registered height plus this next value
public:
    virtual void run(uint256 /*txhash*/, int /*CurrentBlockHeight*/) {}
    virtual void setReferenceBlockHeight() {}
};

#endif // WALLET_INTERFACE_H

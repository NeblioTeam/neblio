#include "transactionrecord.h"

#include "base58.h"
#include "main.h"
#include "txmempool.h"
#include "wallet.h"

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx& wtx)
{
    if (wtx.IsCoinBase()) {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain()) {
            return false;
        }
    }
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet*   wallet,
                                                                 const CWalletTx& wtx)
{
    QList<TransactionRecord> parts;
    int64_t                  nTime   = wtx.GetTxTime();
    int64_t                  nCredit = wtx.GetCredit(static_cast<isminefilter>(isminetype::ISMINE_ALL));
    int64_t                  nDebit  = wtx.GetDebit(static_cast<isminefilter>(isminetype::ISMINE_ALL));
    int64_t                  nNet    = nCredit - nDebit;
    uint256                  hash = wtx.GetHash(), hashPrev = 0;
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    if (nNet > 0 || wtx.IsCoinBase() || wtx.IsCoinStake()) {
        //
        // Credit
        //
        for (const CTxOut& txout : wtx.vout) {
            if (NTP1Transaction::IsTxOutputOpRet(&txout, nullptr)) {
                continue;
            }
            if (wallet->IsMine(txout) != isminetype::ISMINE_NO) {
                TransactionRecord sub(hash, nTime);
                CTxDestination    address;
                sub.idx    = parts.size(); // sequence number
                sub.credit = txout.nValue;
                if (ExtractDestination(txout.scriptPubKey, address) &&
                    IsMine(*wallet, address) != isminetype::ISMINE_NO) {
                    // Received by Bitcoin Address
                    sub.type    = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                } else {
                    // Received by IP connection (deprecated features), or a multisignature or other
                    // non-simple transaction
                    sub.type    = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }
                if (wtx.IsCoinBase()) {
                    // Generated (proof-of-work)
                    sub.type = TransactionRecord::Generated;
                }
                if (wtx.IsCoinStake()) {
                    // Generated (proof-of-stake)

                    if (hashPrev == hash)
                        continue; // last coinstake output

                    sub.type   = TransactionRecord::Generated;
                    sub.credit = nNet > 0 ? nNet : wtx.GetValueOut() - nDebit;
                    hashPrev   = hash;
                }
                if (wtx.HasP2CSOutputs()) {
                    CTxOut p2csUtxo;
                    for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++) {
                        const CTxOut& txout = wtx.vout[nOut];
                        if (txout.scriptPubKey.IsPayToColdStaking()) {
                            p2csUtxo = txout;
                            break;
                        }
                    }
                    bool isSpendable = wallet->IsMine(p2csUtxo) & ISMINE_SPENDABLE_DELEGATED;
                    if (isSpendable) {
                        // Wallet delegating balance
                        sub.type = TransactionRecord::ColdDelegator;
                        CTxDestination dest;
                        if (ExtractDestination(p2csUtxo.scriptPubKey, dest, true)) {
                            sub.address = "Delegated to: " + CBitcoinAddress(dest).ToString();
                        }
                    } else {
                        // Wallet receiving a delegation
                        sub.type = TransactionRecord::ColdStaker;
                        CTxDestination dest;
                        if (ExtractDestination(p2csUtxo.scriptPubKey, dest, false)) {
                            sub.address = "Delegated from: " + CBitcoinAddress(dest).ToString();
                        }
                    }
                }

                parts.append(sub);
            }
        }
    } else {
        bool fAllFromMe = true;
        for (const CTxIn& txin : wtx.vin) {
            fAllFromMe = fAllFromMe && IsMineCheck(wallet->IsMine(txin), isminetype::ISMINE_SPENDABLE);
        }

        bool fAllToMe = true;
        for (const CTxOut& txout : wtx.vout) {
            // OP_RETURN is not to decide whether an output is mine
            if (NTP1Transaction::IsTxOutputOpRet(&txout, nullptr)) {
                continue;
            }
            fAllToMe = fAllToMe && IsMineCheck(wallet->IsMine(txout), isminetype::ISMINE_SPENDABLE);
        }

        if (fAllFromMe && fAllToMe) {
            // Payment to self
            int64_t nChange = wtx.GetChange();

            parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                                           -(nDebit - nChange), nCredit - nChange));
        } else if (fAllFromMe) {
            //
            // Debit
            //
            int64_t nTxFee = nDebit - wtx.GetValueOut();

            for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++) {
                const CTxOut& txout = wtx.vout[nOut];
                if (NTP1Transaction::IsTxOutputOpRet(&txout, nullptr)) {
                    continue;
                }
                TransactionRecord sub(hash, nTime);
                sub.idx = parts.size();

                if (wallet->IsMine(txout) != isminetype::ISMINE_NO) {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                CTxDestination address;
                if (ExtractDestination(txout.scriptPubKey, address)) {
                    // Sent to Bitcoin Address
                    sub.type    = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                } else {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type    = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
                }

                int64_t nValue = txout.nValue;
                /* Add fee to first output */
                if (nTxFee > 0) {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                parts.append(sub);
            }
        } else {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
        }
    }

    return parts;
}

void TransactionRecord::readNTP1TxData()
{
    try {
        if (!mempool.lookup(hash, tx)) {
            tx = CTransaction::FetchTxFromDisk(hash);
        }
        std::vector<std::pair<CTransaction, NTP1Transaction>> ntp1inputs =
            NTP1Transaction::GetAllNTP1InputsOfTx(tx, false);
        ntp1tx.readNTP1DataFromTx(tx, ntp1inputs);
        ntp1DataLoaded    = true;
        ntp1DataLoadError = false;
    } catch (std::exception& ex) {
        printf("Failed to read NTP1 transaction data for transaction record. Transaction hash: %s. "
               "Error: %s\n",
               hash.ToString().c_str(), ex.what());
        ntp1DataLoadError = true;
    }
}

void TransactionRecord::updateStatus(const CWalletTx& wtx)
{
    AssertLockHeld(cs_main);
    // Determine transaction status

    // Find the block the tx is in
    CBlockIndex*                pindex = nullptr;
    BlockIndexMapType::iterator mi     = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = boost::atomic_load(&mi->second).get();

    // Sort order, unrecorded transactions sort to the top
    status.sortKey =
        strprintf("%010d-%01d-%010u-%03d", (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
                  (wtx.IsCoinBase() ? 1 : 0), wtx.nTimeReceived, idx);
    status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity() > 0);
    bool fConflicted        = false;
    status.depth            = wtx.GetDepthAndMempool(fConflicted);
    status.cur_num_blocks   = bestChain.height();

    if (!IsFinalTx(wtx, bestChain.height() + 1)) {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD) {
            status.status   = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - bestChain.height();
        } else {
            status.status   = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }

    // For generated transactions, determine maturity
    else if (type == TransactionRecord::Generated) {
        if (wtx.GetBlocksToMaturity() > 0) {
            status.status = TransactionStatus::Immature;

            if (wtx.IsInMainChain()) {
                status.matures_in = wtx.GetBlocksToMaturity();

                // Check if the block was requested by anyone
                if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
                    status.status = TransactionStatus::MaturesWarning;
            } else {
                status.status = TransactionStatus::NotAccepted;
            }
        } else {
            status.status = TransactionStatus::Confirmed;
        }
    } else {
        if (status.depth < 0 || fConflicted) {
            status.status = TransactionStatus::Conflicted;
        } else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0) {
            status.status = TransactionStatus::Offline;
        } else if (status.depth == 0) {
            status.status = TransactionStatus::Unconfirmed;
        } else if (status.depth < RecommendedNumConfirmations) {
            status.status = TransactionStatus::Confirming;
        } else {
            status.status = TransactionStatus::Confirmed;
        }
    }
}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != bestChain.height();
}

std::string TransactionRecord::getTxID() { return hash.ToString(); }

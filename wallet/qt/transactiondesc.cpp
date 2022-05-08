#include "transactiondesc.h"

#include "bitcoinunits.h"
#include "guiutil.h"

#include "base58.h"
#include "blockindex.h"
#include "consensus.h"
#include "txdb.h"
#include "ui_interface.h"
#include "wallet.h"

QString TransactionDesc::FormatTxStatus(const CWalletTx& wtx)
{
    AssertLockHeld(cs_main);

    const CTxDB       txdb;
    const CBlockIndex bestBlockIndex = txdb.GetBestBlockIndex().value_or(CBlockIndex());
    if (bestBlockIndex.GetBlockHash() == 0) {
        NLog.write(b_sev::critical, "CRITICAL ERROR: Failed to read the best block index");
    }
    if (!IsFinalTx(wtx, txdb, bestBlockIndex.nHeight + 1)) {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
            return tr("Open for %n more block(s)", "", wtx.nLockTime - bestBlockIndex.nHeight);
        else
            return tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx.nLockTime));
    } else {
        bool fConflicted = false;
        int  nDepth      = wtx.GetDepthAndMempool(fConflicted, txdb, bestBlockIndex.GetBlockHash());
        if (nDepth < 0 || fConflicted)
            return tr("conflicted");
        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
            return tr("%1/offline").arg(nDepth);
        else if (nDepth < 10)
            return tr("%1/unconfirmed").arg(nDepth);
        else
            return tr("%1 confirmations").arg(nDepth);
    }
}

std::string FormatNTP1TokenAmount(const NTP1TokenTxData& token)
{
    return ::ToString(token.getAmount()) + " " + token.getTokenSymbol() +
           " (Token ID: " + token.getTokenId() + ")";
}

std::string FormatNTP1TokenAmount(const TokenMinimalData& token)
{
    return ::ToString(token.amount) + " " + token.tokenName + " (Token ID: " + token.tokenId + ")";
}

QString TransactionDesc::toHTML(const ITxDB& txdb, CWallet* wallet, const CWalletTx& wtx)
{
    QString strHTML;

    const uint256 bestBlockHash   = txdb.GetBestBlockHash();
    const int     bestBlockHeight = txdb.GetBestChainHeight().value_or(0);

    LOCK2(cs_main, wallet->cs_wallet);
    strHTML.reserve(4000);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    int64_t nTime = wtx.GetTxTime();
    int64_t nCredit =
        wtx.GetCredit(bestBlockHash, txdb, static_cast<isminefilter>(isminetype::ISMINE_ALL));
    int64_t nDebit = wtx.GetDebit(static_cast<isminefilter>(isminetype::ISMINE_ALL));
    int64_t nNet   = nCredit - nDebit;

    strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(wtx);
    int nRequests = wtx.GetRequestCount();
    if (nRequests != -1) {
        if (nRequests == 0)
            strHTML += tr(", has not been successfully broadcast yet");
        else if (nRequests > 0)
            strHTML += tr(", broadcast through %n node(s)", "", nRequests);
    }
    strHTML += "<br>";

    strHTML += "<b>" + tr("Date") + ":</b> " + (nTime ? GUIUtil::dateTimeStr(nTime) : "") + "<br>";

    //
    // From
    //
    if (wtx.IsCoinBase() || wtx.IsCoinStake()) {
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Generated") + "<br>";
    } else if (wtx.mapValue.count("from") && !wtx.mapValue.at("from").empty()) {
        // Online transaction
        strHTML += "<b>" + tr("From") + ":</b> " + GUIUtil::HtmlEscape(wtx.mapValue.at("from")) + "<br>";
    } else {
        // Offline transaction
        if (nNet > 0) {
            // Credit
            for (const CTxOut& txout : wtx.vout) {
                if (CTransaction::IsOutputOpRet(&txout)) {
                    continue;
                }
                if (wallet->IsMine(txout) != isminetype::ISMINE_NO) {
                    CTxDestination address;
                    if (ExtractDestination(bestBlockHeight, txout.scriptPubKey, address) &&
                        IsMineCheck(IsMine(*wallet, address), isminetype::ISMINE_SPENDABLE)) {
                        if (const auto entry = wallet->mapAddressBook.get(address)) {
                            strHTML += "<b>" + tr("From") + ":</b> " + tr("unknown") + "<br>";
                            strHTML += "<b>" + tr("To") + ":</b> ";
                            strHTML += GUIUtil::HtmlEscape(CBitcoinAddress(address).ToString());
                            if (!entry->name.empty())
                                strHTML += " (" + tr("own address") + ", " + tr("label") + ": " +
                                           GUIUtil::HtmlEscape(entry->name) + ")";
                            else
                                strHTML += " (" + tr("own address") + ")";
                            strHTML += "<br>";
                        }
                    }
                    break;
                }
            }
        }
    }

    bool successInRetrievingNTP1Tx = true;

    NTP1Transaction ntp1tx;
    try {
        std::vector<std::pair<CTransaction, NTP1Transaction>> ntp1inputs =
            NTP1Transaction::GetAllNTP1InputsOfTx(wtx, txdb, false);
        ntp1tx.readNTP1DataFromTx(bestBlockHeight, wtx, ntp1inputs);
    } catch (std::exception& ex) {
        NLog.write(b_sev::err,
                   "(This doesn't have to be an error if the tx is not NTP1). For transaction details, "
                   "failed to retrieve NTP1 data of transaction: {}. Error: {}",
                   wtx.GetHash().ToString(), ex.what());
        successInRetrievingNTP1Tx = false;
    }

    //
    // To
    //
    if (wtx.mapValue.count("to") && !wtx.mapValue.at("to").empty()) {
        // Online transaction
        std::string strAddress = wtx.mapValue.at("to");
        strHTML += "<b>" + tr("To") + ":</b> ";
        CTxDestination dest  = CBitcoinAddress(strAddress).Get();
        const auto     entry = wallet->mapAddressBook.get(dest);
        if (entry.is_initialized() && !entry->name.empty())
            strHTML += GUIUtil::HtmlEscape(entry->name) + " ";
        strHTML += GUIUtil::HtmlEscape(strAddress) + "<br>";
    }

    //
    // Amount
    //
    if (wtx.IsCoinBase() && nCredit == 0) {
        //
        // Coinbase
        //
        int64_t nUnmatured = 0;
        for (const CTxOut& txout : wtx.vout) {
            if (CTransaction::IsOutputOpRet(&txout)) {
                continue;
            }
            nUnmatured += wallet->GetCredit(txout, static_cast<isminefilter>(isminetype::ISMINE_ALL));
        }
        strHTML += "<b>" + tr("Credit") + ":</b> ";
        if (wtx.IsInMainChain(txdb, bestBlockHash)) {
            strHTML +=
                BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nUnmatured) + " (" +
                tr("matures in %n more block(s)", "", wtx.GetBlocksToMaturity(txdb, bestBlockHash)) +
                ")";
        } else {
            strHTML += "(" + tr("not accepted") + ")";
        }
        strHTML += "<br>";
    } else if (nNet > 0) {
        //
        // Credit
        //
        strHTML += "<b>" + tr("Credit") + ":</b> " +
                   BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nNet) + "<br>";
        if (successInRetrievingNTP1Tx) {
            // get token amounts in outputs of the transaction
            std::unordered_map<std::string, TokenMinimalData> outputsTokens =
                NTP1Transaction::CalculateTotalOutputTokens(ntp1tx);

            // calculate total tokens of all kinds to see if there's any tokens involved in the
            // transaction
            NTP1Int totalOutputsTokens =
                std::accumulate(outputsTokens.begin(), outputsTokens.end(), NTP1Int(0),
                                [](NTP1Int currRes, const std::pair<std::string, TokenMinimalData>& t) {
                                    return currRes + t.second.amount;
                                });
            if (totalOutputsTokens != 0) {
                for (const auto& in : outputsTokens) {
                    strHTML += "<b>" + tr("NTP1 credit") + ":</b> " +
                               QString::fromStdString(FormatNTP1TokenAmount(in.second)) + "<br>";
                }
            }
        }
    } else {
        bool fAllFromMe = true;
        for (const CTxIn& txin : wtx.vin) {
            fAllFromMe = fAllFromMe && IsMineCheck(wallet->IsMine(txin), isminetype::ISMINE_SPENDABLE);
        }

        bool fAllToMe = true;
        for (const CTxOut& txout : wtx.vout) {
            if (CTransaction::IsOutputOpRet(&txout)) {
                continue;
            }
            fAllToMe = fAllToMe && IsMineCheck(wallet->IsMine(txout), isminetype::ISMINE_SPENDABLE);
        }

        if (fAllFromMe) {
            //
            // Debit
            //
            for (int i = 0; i < (int)wtx.vout.size(); i++) {
                const CTxOut& txout = wtx.vout[i];
                if (CTransaction::IsOutputOpRet(&txout)) {
                    continue;
                }
                if (IsMineCheck(wallet->IsMine(txout), isminetype::ISMINE_SPENDABLE))
                    continue;

                if (!wtx.mapValue.count("to") || wtx.mapValue.at("to").empty()) {
                    // Offline transaction
                    CTxDestination address;
                    if (ExtractDestination(bestBlockHeight, txout.scriptPubKey, address)) {
                        strHTML += "<b>" + tr("To") + ":</b> ";
                        const auto entry = wallet->mapAddressBook.get(address);
                        if (entry && !entry->name.empty())
                            strHTML += GUIUtil::HtmlEscape(entry->name) + " ";
                        strHTML += GUIUtil::HtmlEscape(CBitcoinAddress(address).ToString());
                        strHTML += "<br>";
                    }
                }

                strHTML += "<b>" + tr("Debit") + ":</b> " +
                           BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -txout.nValue) + "<br>";

                if (successInRetrievingNTP1Tx && i < (int)ntp1tx.getTxOutCount()) {
                    for (int j = 0; j < (int)ntp1tx.getTxOut(i).tokenCount(); j++) {
                        const NTP1TokenTxData& token = ntp1tx.getTxOut(i).getToken(j);
                        strHTML += "<b>" + tr("NTP1 Debit") + ":</b> " +
                                   QString::fromStdString(FormatNTP1TokenAmount(token)) + "<br>";
                    }
                }
            }

            if (fAllToMe) {
                // Payment to self
                int64_t nChange = wtx.GetChange(txdb);
                int64_t nValue  = nCredit - nChange;
                strHTML += "<b>" + tr("Debit") + ":</b> " +
                           BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -nValue) + "<br>";
                strHTML += "<b>" + tr("Credit") + ":</b> " +
                           BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nValue) + "<br>";
                if (successInRetrievingNTP1Tx) {
                    // get token amounts in outputs of the transaction
                    std::unordered_map<std::string, TokenMinimalData> outputsTokens =
                        NTP1Transaction::CalculateTotalOutputTokens(ntp1tx);

                    // calculate total tokens of all kinds to see if there's any tokens involved in the
                    // transaction
                    NTP1Int totalOutputsTokens = std::accumulate(
                        outputsTokens.begin(), outputsTokens.end(), NTP1Int(0),
                        [](NTP1Int currRes, const std::pair<std::string, TokenMinimalData>& t) {
                            return currRes + t.second.amount;
                        });
                    if (totalOutputsTokens != 0) {
                        for (const auto& in : outputsTokens) {
                            strHTML += "<b>" + tr("NTP1 credit") + ":</b> " +
                                       QString::fromStdString(FormatNTP1TokenAmount(in.second)) + "<br>";
                        }
                    }
                }
            }

            int64_t nTxFee = nDebit - wtx.GetValueOut();
            if (nTxFee > 0)
                strHTML += "<b>" + tr("Transaction fee") + ":</b> " +
                           BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, -nTxFee) + "<br>";
        } else {
            //
            // Mixed debit transaction
            //
            for (const CTxIn& txin : wtx.vin) {
                if (wallet->IsMine(txin) != isminetype::ISMINE_NO)
                    strHTML +=
                        "<b>" + tr("Debit") + ":</b> " +
                        BitcoinUnits::formatWithUnit(
                            BitcoinUnits::BTC,
                            -wallet->GetDebit(txin, static_cast<isminefilter>(isminetype::ISMINE_ALL))) +
                        "<br>";
            }
            for (const CTxOut& txout : wtx.vout) {
                if (CTransaction::IsOutputOpRet(&txout)) {
                    continue;
                }
                if (wallet->IsMine(txout) != isminetype::ISMINE_NO)
                    strHTML += "<b>" + tr("Credit") + ":</b> " +
                               BitcoinUnits::formatWithUnit(
                                   BitcoinUnits::BTC,
                                   wallet->GetCredit(
                                       txout, static_cast<isminefilter>(isminetype::ISMINE_ALL))) +
                               "<br>";
            }
        }
    }

    strHTML += "<b>" + tr("Net amount") + ":</b> " +
               BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nNet, true) + "<br>";

    //
    // Message
    //
    if (wtx.mapValue.count("message") && !wtx.mapValue.at("message").empty())
        strHTML += "<br><b>" + tr("Message") + ":</b><br>" +
                   GUIUtil::HtmlEscape(wtx.mapValue.at("message"), true) + "<br>";
    if (wtx.mapValue.count("comment") && !wtx.mapValue.at("comment").empty())
        strHTML += "<br><b>" + tr("Comment") + ":</b><br>" +
                   GUIUtil::HtmlEscape(wtx.mapValue.at("comment"), true) + "<br>";

    strHTML += "<b>" + tr("Transaction ID") + ":</b> " + wtx.GetHash().ToString().c_str() + "<br>";

    if (wtx.IsCoinBase() || wtx.IsCoinStake())
        strHTML += "<br>" +
                   tr("Generated coins must mature 120 blocks before they can be spent. When you "
                      "generated this block, it was broadcast to the network to be added to the block "
                      "chain. If it fails to get into the chain, its state will change to \"not "
                      "accepted\" and it won't be spendable. This may occasionally happen if another "
                      "node generates a block within a few seconds of yours.") +
                   "<br>";

    //
    // Debug view
    //
    if (fDebug) {
        strHTML += "<hr><br>" + tr("Debug information") + "<br><br>";
        for (const CTxIn& txin : wtx.vin)
            if (wallet->IsMine(txin) != isminetype::ISMINE_NO)
                strHTML +=
                    "<b>" + tr("Debit") + ":</b> " +
                    BitcoinUnits::formatWithUnit(
                        BitcoinUnits::BTC,
                        -wallet->GetDebit(txin, static_cast<isminefilter>(isminetype::ISMINE_ALL))) +
                    "<br>";
        for (int i = 0; i < (int)wtx.vout.size(); i++) {
            const CTxOut& txout = wtx.vout[i];
            if (CTransaction::IsOutputOpRet(&txout)) {
                continue;
            }
            if (wallet->IsMine(txout) != isminetype::ISMINE_NO) {
                strHTML +=
                    "<b>" + tr("Credit") + ":</b> " +
                    BitcoinUnits::formatWithUnit(
                        BitcoinUnits::BTC,
                        wallet->GetCredit(txout, static_cast<isminefilter>(isminetype::ISMINE_ALL))) +
                    "<br>";
                if (successInRetrievingNTP1Tx && i < (int)ntp1tx.getTxOutCount()) {
                    for (int j = 0; j < (int)ntp1tx.getTxOut(i).tokenCount(); j++) {
                        const NTP1TokenTxData& token = ntp1tx.getTxOut(i).getToken(j);
                        strHTML += "<b>" + tr("NTP1 Credit") + ":</b> " +
                                   QString::fromStdString(FormatNTP1TokenAmount(token)) + "<br>";
                    }
                }
            }
        }

        strHTML += "<br><b>" + tr("Transaction") + ":</b><br>";
        strHTML += GUIUtil::HtmlEscape(wtx.ToString(), true);

        strHTML += "<br><b>" + tr("Inputs") + ":</b>";
        strHTML += "<ul>";

        for (const CTxIn& txin : wtx.vin) {
            COutPoint prevout = txin.prevout;

            CTransaction prev;
            if (txdb.ReadDiskTx(prevout.hash, prev)) {
                if (prevout.n < prev.vout.size()) {
                    strHTML += "<li>";
                    const CTxOut&  vout = prev.vout[prevout.n];
                    CTxDestination address;
                    if (ExtractDestination(bestBlockHeight, vout.scriptPubKey, address)) {
                        const auto entry = wallet->mapAddressBook.get(address);
                        if (entry && !entry->name.empty())
                            strHTML += GUIUtil::HtmlEscape(entry->name) + " ";
                        strHTML += QString::fromStdString(CBitcoinAddress(address).ToString());
                    }
                    strHTML = strHTML + " " + tr("Amount") + "=" +
                              BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, vout.nValue);
                    strHTML =
                        strHTML + " IsMine=" +
                        (IsMineCheck(wallet->IsMine(vout), isminetype::ISMINE_SPENDABLE) ? tr("true")
                                                                                         : tr("false")) +
                        "</li>";
                    strHTML = strHTML + " IsWatchOnly=" +
                              (IsMineCheck(wallet->IsMine(vout), isminetype::ISMINE_WATCH_ONLY)
                                   ? tr("true")
                                   : tr("false")) +
                              "</li>";
                }
            }
        }

        strHTML += "</ul>";
    }

    strHTML += "</font></html>";
    return strHTML;
}

#ifndef TRANSACTIONDESC_H
#define TRANSACTIONDESC_H

#include "itxdb.h"
#include <QObject>
#include <QString>
#include <string>

class CWallet;
class CWalletTx;

/** Provide a human-readable extended HTML description of a transaction.
 */
class TransactionDesc : public QObject
{
    Q_OBJECT
public:
    static QString toHTML(const ITxDB& txdb, CWallet* wallet, const CWalletTx& wtx);

private:
    TransactionDesc() {}

    static QString FormatTxStatus(const CWalletTx& wtx);
};

#endif // TRANSACTIONDESC_H

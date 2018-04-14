#ifndef NTP1TRANSACTION_H
#define NTP1TRANSACTION_H

#include "ntp1txin.h"
#include "ntp1txout.h"

#include <string>
#include <vector>

/**
 * @brief The NTP1Transaction class
 * A single NTP1 transaction
 */
class NTP1Transaction
{
    static const int CURRENT_VERSION = 1;
    int nVersion;
    uint256 txHash;
    std::vector<unsigned char> txSerialized;
    std::vector<NTP1TxIn>  vin;
    std::vector<NTP1TxOut> vout;
    uint64_t nLockTime;
    uint64_t nTime;

public:
    NTP1Transaction();
    void setNull();
    bool isNull() const;
    void importJsonData(const std::string& data);
    std::string getHex() const;
    uint256 getTxHash() const;
    uint64_t getLockTime() const;
    uint64_t getTime() const;
    unsigned long getTxInCount() const;
    const NTP1TxIn& getTxIn(unsigned long index) const;
    unsigned long getTxOutCount() const;
    const NTP1TxOut& getTxOut(unsigned long index) const;
};

#endif // NTP1TRANSACTION_H

#ifndef NTP1TRANSACTION_H
#define NTP1TRANSACTION_H

#include "ntp1txin.h"
#include "ntp1txout.h"
#include "uint256.h"

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
    json_spirit::Value exportDatabaseJsonData() const;
    void importDatabaseJsonData(const json_spirit::Value& data);
    void setHex(const std::string &Hex);
    std::string getHex() const;
    uint256 getTxHash() const;
    uint64_t getLockTime() const;
    uint64_t getTime() const;
    unsigned long getTxInCount() const;
    const NTP1TxIn& getTxIn(unsigned long index) const;
    unsigned long getTxOutCount() const;
    const NTP1TxOut& getTxOut(unsigned long index) const;
    friend inline bool operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs);
};

bool operator==(const NTP1Transaction &lhs, const NTP1Transaction &rhs)
{
    return (lhs.nVersion == rhs.nVersion &&
            lhs.txHash == rhs.txHash &&
            lhs.txSerialized == rhs.txSerialized &&
            lhs.vin == rhs.vin &&
            lhs.vout == rhs.vout &&
            lhs.nLockTime == rhs.nLockTime &&
            lhs.nTime == rhs.nTime);
}

#endif // NTP1TRANSACTION_H

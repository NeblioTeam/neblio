#ifndef NTP1TRANSACTION_H
#define NTP1TRANSACTION_H

#include "main.h"
#include "ntp1txin.h"
#include "ntp1txout.h"
#include "uint256.h"

#include <string>
#include <vector>

/** Position on disk for a particular transaction. */
class DiskNTP1TxPos
{
public:
    unsigned int nFile;
    unsigned int nTxPos;

    DiskNTP1TxPos() { SetNull(); }

    DiskNTP1TxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn)
    {
        nFile  = nFileIn;
        nTxPos = nTxPosIn;
    }

    IMPLEMENT_SERIALIZE(READWRITE(FLATDATA(*this));)
    void SetNull()
    {
        nFile  = (unsigned int)-1;
        nTxPos = 0;
    }
    bool IsNull() const { return (nFile == (unsigned int)-1); }

    friend bool operator==(const DiskNTP1TxPos& a, const DiskNTP1TxPos& b)
    {
        return (a.nFile == b.nFile && a.nTxPos == b.nTxPos);
    }

    friend bool operator!=(const DiskNTP1TxPos& a, const DiskNTP1TxPos& b) { return !(a == b); }

    std::string ToString() const
    {
        if (IsNull())
            return "null";
        else
            return strprintf("(nFile=%u, nTxPos=%u)", nFile, nTxPos);
    }

    void print() const { printf("%s", ToString().c_str()); }
};

/**
 * @brief The NTP1Transaction class
 * A single NTP1 transaction
 */
class NTP1Transaction
{
    static const int           CURRENT_VERSION = 1;
    int                        nVersion;
    uint256                    txHash;
    std::vector<unsigned char> txSerialized;
    std::vector<NTP1TxIn>      vin;
    std::vector<NTP1TxOut>     vout;
    uint64_t                   nLockTime;
    uint64_t                   nTime;
    NTP1TransactionType        ntp1TransactionType = NTP1TxType_NOT_NTP1;

public:
    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(this->nVersion);
                        nVersion = this->nVersion;
                        READWRITE(nTime);
                        READWRITE(txHash);
                        READWRITE(vin);
                        READWRITE(vout);
                        READWRITE(nLockTime);
                        READWRITE(ntp1TransactionType);
                        )
    // clang-format on

    NTP1Transaction();
    void               setNull();
    bool               isNull() const;
    void               importJsonData(const std::string& data);
    json_spirit::Value exportDatabaseJsonData() const;
    void               importDatabaseJsonData(const json_spirit::Value& data);
    void               setHex(const std::string& Hex);
    std::string        getHex() const;
    uint256            getTxHash() const;
    uint64_t           getLockTime() const;
    uint64_t           getTime() const;
    unsigned long      getTxInCount() const;
    const NTP1TxIn&    getTxIn(unsigned long index) const;
    unsigned long      getTxOutCount() const;
    const NTP1TxOut&   getTxOut(unsigned long index) const;
    friend inline bool operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs);

    void readNTP1DataFromTx(const CTransaction& tx);

    bool writeToDisk(unsigned int& nFileRet, unsigned int& nTxPosRet);
    bool readFromDisk(DiskNTP1TxPos pos, FILE** pfileRet = NULL);
};

bool operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs)
{
    return (lhs.nVersion == rhs.nVersion && lhs.txHash == rhs.txHash &&
            lhs.txSerialized == rhs.txSerialized && lhs.vin == rhs.vin && lhs.vout == rhs.vout &&
            lhs.nLockTime == rhs.nLockTime && lhs.nTime == rhs.nTime);
}

#endif // NTP1TRANSACTION_H

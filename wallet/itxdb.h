#ifndef ITXDB_H
#define ITXDB_H

#include <boost/optional.hpp>
#include <string>
#include <vector>

class uint256;
class CTransaction;
class NTP1Transaction;
class CTxIndex;
class CDiskTxPos;
class CBitcoinAddress;
class COutPoint;
class CBlock;
class CDiskBlockIndex;
class CBigNum;

class ITxDB
{
public:
    virtual ~ITxDB() = default;

    virtual boost::optional<int> ReadVersion()                                                   = 0;
    virtual bool                 WriteVersion(int nVersion)                                      = 0;
    virtual bool                 ReadTxIndex(const uint256& hash, CTxIndex& txindex)             = 0;
    virtual bool                 UpdateTxIndex(const uint256& hash, const CTxIndex& txindex)     = 0;
    virtual bool                 ReadTx(const CDiskTxPos& txPos, CTransaction& tx)               = 0;
    virtual bool                 ReadNTP1Tx(const uint256& hash, NTP1Transaction& ntp1tx)        = 0;
    virtual bool                 WriteNTP1Tx(const uint256& hash, const NTP1Transaction& ntp1tx) = 0;
    virtual bool                 ReadAllIssuanceTxs(std::vector<uint256>& txs)                   = 0;
    virtual bool ReadNTP1TxsWithTokenSymbol(std::string tokenName, std::vector<uint256>& txs)    = 0;
    virtual bool WriteNTP1TxWithTokenSymbol(std::string tokenName, const NTP1Transaction& tx)    = 0;
    virtual bool ReadAddressPubKey(const CBitcoinAddress& address, std::vector<uint8_t>& pubkey) = 0;
    virtual bool WriteAddressPubKey(const CBitcoinAddress&      address,
                                    const std::vector<uint8_t>& pubkey)                          = 0;
    virtual bool EraseTxIndex(const uint256& hash)                                               = 0;
    virtual bool ContainsTx(const uint256& hash)                                                 = 0;
    virtual bool ContainsNTP1Tx(const uint256& hash)                                             = 0;
    virtual bool ReadDiskTx(const uint256& hash, CTransaction& tx, CTxIndex& txindex)            = 0;
    virtual bool ReadDiskTx(const uint256& hash, CTransaction& tx)                               = 0;
    virtual bool ReadDiskTx(const COutPoint& outpoint, CTransaction& tx, CTxIndex& txindex)      = 0;
    virtual bool ReadDiskTx(const COutPoint& outpoint, CTransaction& tx)                         = 0;
    virtual bool ReadBlock(const uint256& hash, CBlock& blk, bool fReadTransactions = true)      = 0;
    virtual bool WriteBlock(const uint256& hash, const CBlock& blk)                              = 0;
    virtual bool WriteBlockIndex(const CDiskBlockIndex& blockindex)                              = 0;
    virtual bool ReadHashBestChain(uint256& hashBestChain)                                       = 0;
    virtual bool WriteHashBestChain(const uint256& hashBestChain)                                = 0;
    virtual bool ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust)                               = 0;
    virtual bool WriteBestInvalidTrust(const CBigNum& bnBestInvalidTrust)                        = 0;
    virtual bool LoadBlockIndex()                                                                = 0;
};

#endif // ITXDB_H

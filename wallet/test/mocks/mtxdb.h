#include "gmock/gmock.h"

#include "itxdb.h"

struct mTxDB : public ITxDB
{
    MOCK_METHOD(boost::optional<int>, ReadVersion, (), (override));
    MOCK_METHOD(bool, WriteVersion, (int nVersion), (override));
    MOCK_METHOD(bool, ReadTxIndex, (const uint256& hash, CTxIndex& txindex), (override));
    MOCK_METHOD(bool, UpdateTxIndex, (const uint256& hash, const CTxIndex& txindex), (override));
    MOCK_METHOD(bool, ReadTx, (const CDiskTxPos& txPos, CTransaction& tx), (override));
    MOCK_METHOD(bool, ReadNTP1Tx, (const uint256& hash, NTP1Transaction& ntp1tx), (override));
    MOCK_METHOD(bool, WriteNTP1Tx, (const uint256& hash, const NTP1Transaction& ntp1tx), (override));
    MOCK_METHOD(bool, ReadAllIssuanceTxs, (std::vector<uint256> & txs), (override));
    MOCK_METHOD(bool, ReadNTP1TxsWithTokenSymbol,
                (const std::string& tokenName, std::vector<uint256>& txs), (override));
    MOCK_METHOD(bool, WriteNTP1TxWithTokenSymbol,
                (const std::string& tokenName, const NTP1Transaction& tx), (override));
    MOCK_METHOD(bool, ReadAddressPubKey, (const CBitcoinAddress& address, std::vector<uint8_t>& pubkey),
                (override));
    MOCK_METHOD(bool, WriteAddressPubKey,
                (const CBitcoinAddress& address, const std::vector<uint8_t>& pubkey), (override));
    MOCK_METHOD(bool, EraseTxIndex, (const uint256& hash), (override));
    MOCK_METHOD(bool, ContainsTx, (const uint256& hash), (override));
    MOCK_METHOD(bool, ContainsNTP1Tx, (const uint256& hash), (override));
    MOCK_METHOD(bool, ReadDiskTx, (const uint256& hash, CTransaction& tx, CTxIndex& txindex),
                (override));
    MOCK_METHOD(bool, ReadDiskTx, (const uint256& hash, CTransaction& tx), (override));
    MOCK_METHOD(bool, ReadDiskTx, (const COutPoint& outpoint, CTransaction& tx, CTxIndex& txindex),
                (override));
    MOCK_METHOD(bool, ReadDiskTx, (const COutPoint& outpoint, CTransaction& tx), (override));
    MOCK_METHOD(bool, ReadBlock, (const uint256& hash, CBlock& blk, bool fReadTransactions), (override));
    MOCK_METHOD(bool, WriteBlock, (const uint256& hash, const CBlock& blk), (override));
    MOCK_METHOD(bool, WriteBlockIndex, (const CDiskBlockIndex& blockindex), (override));
    MOCK_METHOD(bool, ReadHashBestChain, (uint256 & hashBestChain), (override));
    MOCK_METHOD(bool, WriteHashBestChain, (const uint256& hashBestChain), (override));
    MOCK_METHOD(bool, ReadBestInvalidTrust, (CBigNum & bnBestInvalidTrust), (override));
    MOCK_METHOD(bool, WriteBestInvalidTrust, (const CBigNum& bnBestInvalidTrust), (override));
    MOCK_METHOD(bool, LoadBlockIndex, (), (override));
};

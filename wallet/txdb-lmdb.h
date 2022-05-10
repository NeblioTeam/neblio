// Copyright (c) 2009-2012 The Bitcoin Developers.
// Authored by Google, Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LMDB_H
#define BITCOIN_LMDB_H

//#define DEEP_LMDB_LOGGING

#include <atomic>
#include <boost/filesystem.hpp>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "db/lmdb/lmdb.h"
#include "db/lmdb/lmdbtransaction.h"
#include "disktxpos.h"
#include "itxdb.h"
#include "outpoint.h"
#include "txindex.h"
#include "util.h"

class NTP1Transaction;
class CBigNum;
class CBlock;
class CTransaction;
class CBitcoinAddress;

const std::string QuickSyncDataLink =
    "https://raw.githubusercontent.com/NeblioTeam/neblio-quicksync/master/download.json";

namespace {
template <typename T>
boost::optional<std::string> SerializeSimple(const T& key)
{
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    try {
        ssKey.reserve(1000);
        ssKey << key;
        return ssKey.str();
    } catch (const std::exception& e) {
        NLog.write(b_sev::err, "Failed to serialize key in SerializeKey {}", ssKey.str());
        return boost::none;
    }
}

// template <>
// boost::optional<std::string> SerializeSimple(const std::string& key)
//{
//    return key;
//}
} // namespace

// Class that provides access to a LevelDB. Note that this class is frequently
// instantiated on the stack and then destroyed again, so instantiation has to
// be very cheap. Unfortunately that means, a CTxDB instance is actually just a
// wrapper around some global state.
//
// A LevelDB is a key/value store that is optimized for fast usage on hard
// disks. It prefers long read/writes to seeks and is based on a series of
// sorted key/value mapping files that are stacked on top of each other, with
// newer files overriding older files. A background thread compacts them
// together when too many files stack up.
//
// Learn more: http://code.google.com/p/leveldb/
class CTxDB : public ITxDB
{
    std::unique_ptr<IDB> db;

public:
    static boost::filesystem::path DB_DIR;

    // this flag is useful for disabling quicksync manually, for example, for tests
    static bool QuickSyncHigherControl_Enabled;

    CTxDB();
    CTxDB(const CTxDB&) = delete;
    CTxDB(CTxDB&&)      = delete;
    CTxDB& operator=(const CTxDB&) = delete;
    CTxDB& operator=(CTxDB&&) = delete;

    // Destroys the underlying shared global state accessed by this TxDB.
    void Close();

protected:
    // Returns true and sets (value,false) if activeBatch contains the given key
    // or leaves value alone and sets deleted = true if activeBatch contains a
    // delete for it.
    //    bool ScanBatch(const CDataStream& key, std::string* value, bool* deleted) const;

    template <typename K, typename T>
    bool Read(const K& key, T& value, IDB::Index dbindex, int serializationTypeModifiers = 0,
              size_t offset = 0) const
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        const Result<boost::optional<std::string>, int> res =
            db->read(dbindex, *ssKey, offset, boost::none);

        if (!res.isOk()) {
            return false;
        }

        const boost::optional<std::string>& rVal = res.UNWRAP();

        if (!rVal) {
            return false;
        }

        try {
            CDataStream ssValue(rVal->c_str(), rVal->c_str() + rVal->size(),
                                SER_DISK | serializationTypeModifiers, CLIENT_VERSION);
            ssValue >> value;
            return true;
        } catch (const std::exception& e) {
            NLog.write(b_sev::err, "Failed to deserialized in lmdb Read() data for key {}",
                       ssKey->c_str());
            return false;
        }
    }

    /**
     * ReadMultiple key/value pairs, either starting at "key" or just all the keys in the db. If readAll
     * is true, everything in the db will be read
     */
    template <typename K, typename T, template <typename, typename = std::allocator<T>> class Container>
    bool ReadMultiple(const K& key, Container<T>& values, IDB::Index dbindex) const
    {
        values.clear();

        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        const Result<std::vector<std::string>, int> res = db->readMultiple(dbindex, *ssKey);
        if (res.isErr()) {
            return false;
        }

        for (const auto& v : res.UNWRAP()) {
            const std::string& valStr = v;
            try {
                T value;

                CDataStream ssValue(valStr.c_str(), valStr.c_str() + valStr.size(), SER_DISK,
                                    CLIENT_VERSION);
                ssValue >> value;
                values.insert(values.end(), std::move(value));
            } catch (const std::exception& e) {
                unsigned int sz = static_cast<unsigned int>(values.size());
                NLog.write(b_sev::err,
                           "Failed to deserialized element number {} in lmdb ReadMultiple() data", sz);
                return false;
            }
        }

        return true;
    }

    /**
     * ReadMultipleWithKeys reads all keys and values in a db
     */
    template <typename K, typename T, template <typename, typename = std::allocator<T>> class Container>
    bool ReadMultipleWithKeys(std::map<K, Container<T>>& values, IDB::Index dbindex) const
    {
        const Result<std::map<std::string, std::vector<std::string>>, int> resStr = db->readAll(dbindex);

        if (!resStr.isOk()) {
            return false;
        }

        // deserialize retrieved values
        for (const auto& r : resStr.UNWRAP()) {
            const auto& key = r.first;
            for (const auto& v : r.second) {
                const std::string& valStr = v;
                try {
                    CDataStream ssKey(key.c_str(), key.c_str() + key.size(), SER_DISK, CLIENT_VERSION);
                    std::string keyP;
                    ssKey >> keyP;
                    CDataStream ssValue(valStr.c_str(), valStr.c_str() + valStr.size(), SER_DISK,
                                        CLIENT_VERSION);
                    T           value;
                    ssValue >> value;
                    Container<T>& cont = values[keyP];
                    cont.insert(cont.end(), value);
                } catch (const std::exception& e) {
                    unsigned int sz = static_cast<unsigned int>(values.size());
                    NLog.write(
                        b_sev::err,
                        "Failed to deserialized element number {} in lmdb ReadMultipleWithKeys() data",
                        sz);
                    return false;
                }
            }
        }
        return true;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, IDB::Index dbindex)
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        const boost::optional<std::string> ssValue = SerializeSimple(value);
        if (!ssValue) {
            return false;
        }

        return db->write(dbindex, *ssKey, *ssValue).isOk();
    }

    template <typename K>
    bool Erase(const K& key, IDB::Index dbindex)
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        return db->erase(dbindex, *ssKey).isOk();
    }

    template <typename K>
    bool EraseAll(const K& key, IDB::Index dbindex)
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        return db->eraseAll(dbindex, *ssKey).isOk();
    }

    template <typename K>
    bool Exists(const K& key, IDB::Index dbindex) const
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        return db->exists(dbindex, *ssKey).unwrapOr(false);
    }

public:
    bool TxnBegin(std::size_t required_size = 0) override;
    bool TxnCommit() override;
    bool TxnAbort() override;

    boost::optional<int> ReadVersion();

    bool WriteVersion(int nVersionIn) override;
    bool ReadTxIndex(const uint256& hash, CTxIndex& txindex) const override;
    bool UpdateTxIndex(const uint256& hash, const CTxIndex& txindex) override;
    bool ReadTx(const CDiskTxPos& txPos, CTransaction& tx) const override;
    bool ReadNTP1Tx(const uint256& hash, NTP1Transaction& ntp1tx) const override;
    bool WriteNTP1Tx(const uint256& hash, const NTP1Transaction& ntp1tx) override;
    bool ReadAllIssuanceTxs(std::vector<uint256>& txs) const override;
    bool ReadNTP1TxsWithTokenSymbol(std::string tokenName, std::vector<uint256>& txs) const override;
    bool WriteNTP1TxWithTokenSymbol(std::string tokenName, const NTP1Transaction& tx) override;
    bool ReadAddressPubKey(const CBitcoinAddress& address, std::vector<uint8_t>& pubkey) const override;
    bool WriteAddressPubKey(const CBitcoinAddress& address, const std::vector<uint8_t>& pubkey) override;
    bool EraseTxIndex(const uint256& hash) override;
    bool ContainsTx(const uint256& hash) const override;
    bool ContainsNTP1Tx(const uint256& hash) const override;
    bool ReadDiskTx(const uint256& hash, CTransaction& tx, CTxIndex& txindex) const override;
    bool ReadDiskTx(const uint256& hash, CTransaction& tx) const override;
    bool ReadDiskTx(const COutPoint& outpoint, CTransaction& tx, CTxIndex& txindex) const override;
    bool ReadDiskTx(const COutPoint& outpoint, CTransaction& tx) const override;
    bool ReadBlock(const uint256& hash, CBlock& blk, bool fReadTransactions = true) const override;
    bool WriteBlock(const uint256& hash, const CBlock& blk) override;
    boost::optional<CBlockIndex> ReadBlockIndex(const uint256& blockHash) const override;
    bool                         WriteBlockIndex(const CBlockIndex& blockindex) override;
    bool                         EraseBlockHashOfHeight(int32_t height) override;
    boost::optional<uint256>     ReadBlockHashOfHeight(int32_t height) const override;
    bool WriteBlockHashOfHeight(int32_t height, const uint256& blockHash) override;
    boost::optional<BlockMetadata> ReadBlockMetadata(const uint256& blockHash) const override;
    bool                           WriteBlockMetadata(const BlockMetadata& blockMetadata) override;
    bool                           ReadHashBestChain(uint256& hashBestChain) const override;
    bool                           WriteHashBestChain(const uint256& hashBestChain) override;
    bool                           ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust) const override;
    bool                           WriteBestInvalidTrust(const CBigNum& bnBestInvalidTrust) override;
    boost::optional<std::map<uint256, CBlockIndex>> ReadAllBlockIndexEntries() const override;
    bool                  WriteStakeSeen(const std::pair<COutPoint, unsigned int>& stake) override;
    boost::optional<bool> WasStakeSeen(const std::pair<COutPoint, unsigned int>& stake) const override;
    bool                  LoadBlockIndex() override;
    int                   GetBestChainHeight() const override;
    boost::optional<uint256>     GetBestChainTrust() const override;
    boost::optional<CBlockIndex> GetBestBlockIndex() const override;
    uint256                      GetBestBlockHash() const override;

    static uintmax_t GetCurrentDiskUsage();

    void resyncIfNecessary(bool forceClearDB = false);
};

#endif // BITCOIN_LMDB_H

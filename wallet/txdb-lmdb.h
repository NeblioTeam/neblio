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
#include "diskblockindex.h"
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
        printf("Failed to serialize key in SerializeKey %s\n", ssKey.str().c_str());
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
    std::unique_ptr<LMDB> db;

public:
    static boost::filesystem::path DB_DIR;

    // this flag is useful for disabling quicksync manually, for example, for tests
    static bool QuickSyncHigherControl_Enabled;

    static std::unique_ptr<ILog> TxDBLogger;

    CTxDB();
    CTxDB(const CTxDB&) = delete;
    CTxDB(CTxDB&&)      = delete;
    CTxDB& operator=(const CTxDB&) = delete;
    CTxDB& operator=(CTxDB&&) = delete;
    ~CTxDB();

    // Destroys the underlying shared global state accessed by this TxDB.
    void Close();

    static void __deleteDb();

private:
    int nVersion;

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

        const boost::optional<std::string> res = db->read(dbindex, *ssKey, offset, boost::none);
        if (!res) {
            return false;
        }

        try {
            CDataStream ssValue(res->c_str(), res->c_str() + res->size(),
                                SER_DISK | serializationTypeModifiers, CLIENT_VERSION);
            ssValue >> value;
            return true;
        } catch (const std::exception& e) {
            printf("Failed to deserialized in lmdb Read() data for key %s\n", ssKey->c_str());
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

        const boost::optional<std::vector<std::string>> res = db->readMultiple(dbindex, *ssKey);
        if (!res) {
            return false;
        }
        for (const auto& v : *res) {
            const std::string& valStr = v;
            try {
                T value;

                CDataStream ssValue(valStr.c_str(), valStr.c_str() + valStr.size(), SER_DISK,
                                    CLIENT_VERSION);
                ssValue >> value;
                values.insert(values.end(), std::move(value));
            } catch (const std::exception& e) {
                unsigned int sz = static_cast<unsigned int>(values.size());
                printf("Failed to deserialized element number %u in lmdb ReadMultiple() data\n", sz);
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
        const boost::optional<std::map<std::string, std::vector<std::string>>> resStr =
            db->readAll(dbindex);

        if (!resStr) {
            return false;
        }

        // deserialize retrieved values
        for (const auto& r : *resStr) {
            const auto& key = r.first;
            for (const auto& v : r.second) {
                const std::string& valStr = v;
                try {
                    CDataStream ssKey(key.c_str(), key.c_str() + key.size(), SER_DISK, CLIENT_VERSION);
                    std::string key;
                    ssKey >> key;
                    CDataStream ssValue(valStr.c_str(), valStr.c_str() + valStr.size(), SER_DISK,
                                        CLIENT_VERSION);
                    T           value;
                    ssValue >> value;
                    Container<T>& cont = values[key];
                    cont.insert(cont.end(), value);
                } catch (const std::exception& e) {
                    unsigned int sz = static_cast<unsigned int>(values.size());
                    printf(
                        "Failed to deserialized element number %u in lmdb ReadMultipleWithKeys() data\n",
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

        return db->write(dbindex, *ssKey, *ssValue);
    }

    template <typename K>
    bool Erase(const K& key, IDB::Index dbindex)
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        return db->erase(dbindex, *ssKey);
    }

    template <typename K>
    bool EraseAll(const K& key, IDB::Index dbindex)
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        return db->eraseAll(dbindex, *ssKey);
    }

    template <typename K>
    bool Exists(const K& key, IDB::Index dbindex) const
    {
        const boost::optional<std::string> ssKey = SerializeSimple(key);
        if (!ssKey) {
            return false;
        }

        return db->exists(dbindex, *ssKey);
    }

public:
    bool TxnBegin(std::size_t required_size = 0);
    bool TxnCommit();
    bool TxnAbort();

    // for tests
    bool test1_WriteStrKeyVal(const std::string& key, const std::string& val);
    bool test1_ReadStrKeyVal(const std::string& key, std::string& val);
    bool test1_ExistsStrKeyVal(const std::string& key);
    bool test1_EraseStrKeyVal(const std::string& key);

    bool test2_ReadMultipleStr1KeyVal(const std::string& key, std::vector<std::string>& val);
    bool test2_ReadMultipleAllStr1KeyVal(std::map<std::string, std::vector<std::string>>& vals);
    bool test2_WriteStrKeyVal(const std::string& key, const std::string& val);
    bool test2_ExistsStrKeyVal(const std::string& key);
    bool test2_EraseStrKeyVal(const std::string& key);

    boost::optional<int> ReadVersion();

    bool WriteVersion(int nVersion) override;
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
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex) override;
    bool ReadHashBestChain(uint256& hashBestChain) const override;
    bool WriteHashBestChain(const uint256& hashBestChain) override;
    bool ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust) const override;
    bool WriteBestInvalidTrust(const CBigNum& bnBestInvalidTrust) override;
    bool LoadBlockIndex() override;
    boost::optional<int>           GetBestChainHeight() const override;
    boost::optional<uint256>       GetBestChainTrust() const override;
    boost::shared_ptr<CBlockIndex> GetBestBlockIndex() const override;
    uint256                        GetBestBlockHash() const override;

    static uintmax_t GetCurrentDiskUsage();

    void resyncIfNecessary(bool forceClearDB = false);
};

#endif // BITCOIN_LMDB_H

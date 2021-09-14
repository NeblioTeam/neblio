#include "inmemorydb.h"

std::array<std::map<std::string, std::vector<std::string>>,
           static_cast<std::size_t>(IDB::Index::Index_Last)>
                      InMemoryDB::data({});
InMemoryDB::MutexType InMemoryDB::mtx;

InMemoryDB::InMemoryDB(const boost::filesystem::path* const, bool startNewDatabase)
{
    if (startNewDatabase) {
        InMemoryDB::clearDBData();
    }
}

std::map<IDB::Index, std::map<std::string, TransactionOperation>> GetAllTxData(HierarchicalDB* tx)
{
    if (!tx) {
        return {};
    }

    std::map<IDB::Index, std::map<std::string, TransactionOperation>> txData;
    for (int index = 0; index < static_cast<int>(IDB::Index::Index_Last); index++) {
        txData[static_cast<IDB::Index>(index)] = tx->getAllDataForDB(index);
    }
    return txData;
}

boost::optional<std::string> InMemoryDB::read(Index dbindex, const std::string& key, std::size_t offset,
                                              const boost::optional<std::size_t>& size) const
{
    if (tx) {
        const auto& op = tx->getOp(static_cast<int>(dbindex), key);
        if (op) {
            switch (op->getOpType()) {
            case TransactionOperation::UniqueSet:
            case TransactionOperation::Append:
                if (!op->getValues().empty()) {
                    if (size) {
                        return op->getValues().front().substr(offset, *size);
                    } else {
                        return op->getValues().front().substr(offset);
                    }
                }
                break;
            case TransactionOperation::Erase:
                return boost::none;
            }
        }
    }

    std::lock_guard<MutexType> lg(mtx);

    const std::map<std::string, std::vector<std::string>>& kvMap =
        data[static_cast<std::size_t>(dbindex)];

    auto it_key = kvMap.find(key);
    if (it_key == kvMap.cend()) {
        return boost::none;
    }

    const std::vector<std::string>& vec = it_key->second;

    if (vec.empty()) {
        return boost::none;
    }

    if (size) {
        return boost::make_optional(vec.front().substr(offset, *size));
    } else {
        return boost::make_optional(vec.front().substr(offset));
    }
}

boost::optional<std::vector<std::string>> InMemoryDB::readMultiple(Index              dbindex,
                                                                   const std::string& key) const
{
    std::vector<std::string> valuesToAppend;

    if (tx) {
        const auto& op = tx->getOp(static_cast<int>(dbindex), key);
        if (op) {
            switch (op->getOpType()) {
            case TransactionOperation::UniqueSet:
            case TransactionOperation::Append: {
                valuesToAppend = op->getValues();
                break;
            case TransactionOperation::Erase: {
                return std::vector<std::string>();
            }
            }
            }
        }
    }

    std::lock_guard<MutexType> lg(mtx);

    const std::map<std::string, std::vector<std::string>>& kvMap =
        data[static_cast<std::size_t>(dbindex)];

    auto it_key = kvMap.find(key);
    if (it_key == kvMap.cend()) {
        return boost::make_optional(valuesToAppend);
    }

    std::vector<std::string> vec = it_key->second;

    vec.insert(vec.end(), std::make_move_iterator(valuesToAppend.begin()),
               std::make_move_iterator(valuesToAppend.end()));

    return boost::make_optional(std::move(vec));
}

static void MergeTxDataWithData(std::map<std::string, std::vector<std::string>>& dataMap,
                                std::map<std::string, TransactionOperation>&&    txData)
{
    for (auto&& p : txData) {
        auto&& key  = p.first;
        auto&& txOp = p.second;

        switch (txOp.getOpType()) {
        case TransactionOperation::Append:
            dataMap[key].insert(dataMap[key].end(), std::make_move_iterator(txOp.getValues().begin()),
                                std::make_move_iterator(txOp.getValues().end()));
            break;
        case TransactionOperation::UniqueSet:
            if (!txOp.getValues().empty()) {
                dataMap[key] = txOp.getValues();
            }
            break;
        case TransactionOperation::Erase:
            dataMap.erase(key);
            break;
        }
    }
}

boost::optional<std::map<std::string, std::vector<std::string>>> InMemoryDB::readAll(Index dbindex) const
{
    std::vector<std::string> valuesToAppend;

    std::map<std::string, TransactionOperation> txData;
    if (tx) {
        txData = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    std::map<std::string, std::vector<std::string>> finalData;
    {
        std::lock_guard<MutexType> lg(mtx);

        finalData = data[static_cast<std::size_t>(dbindex)];
    }

    MergeTxDataWithData(finalData, std::move(txData));

    return boost::make_optional(std::move(finalData));
}

static void MergeTxDataWithData(std::map<std::string, std::string>&           dataMap,
                                std::map<std::string, TransactionOperation>&& txData)
{
    for (auto&& p : txData) {
        auto&& key  = p.first;
        auto&& txOp = p.second;

        switch (txOp.getOpType()) {
        case TransactionOperation::Append:
        case TransactionOperation::UniqueSet:
            if (!txOp.getValues().empty()) {
                dataMap[key] = txOp.getValues().front();
            }
            break;
        case TransactionOperation::Erase:
            dataMap.erase(key);
            break;
        }
    }
}

boost::optional<std::map<std::string, std::string>> InMemoryDB::readAllUnique(Index dbindex) const
{
    std::map<std::string, TransactionOperation> txData;
    if (tx) {
        txData = tx->getAllDataForDB(static_cast<int>(dbindex));
    }

    std::map<std::string, std::string> finalData;

    std::lock_guard<MutexType> lg(mtx);

    const std::map<std::string, std::vector<std::string>>& dataInDB =
        data[static_cast<std::size_t>(dbindex)];

    for (const auto& p : dataInDB) {
        if (!p.second.empty()) {
            finalData[p.first] = p.second.front();
        }
    }

    MergeTxDataWithData(finalData, std::move(txData));

    return boost::make_optional(finalData);
}

bool InMemoryDB::write_in_tx(Index dbindex, const std::string& key, const std::string& value)
{
    if (tx) {
        if (IDB::DuplicateKeysAllowed(dbindex)) {
            return tx->multi_append(static_cast<int>(dbindex), key, value);
        } else {
            return tx->unique_set(static_cast<int>(dbindex), key, value);
        }
    }
    return false;
}

bool InMemoryDB::write_unsafe(Index dbindex, const std::string& key, const std::string& value)
{
    if (write_in_tx(dbindex, key, value)) {
        return true;
    }

    if (IDB::DuplicateKeysAllowed(dbindex)) {
        data[static_cast<std::size_t>(dbindex)][key].push_back(value);
    } else [[likely]] {
        data[static_cast<std::size_t>(dbindex)][key] = std::vector<std::string>(1, value);
    }
    return true;
}

bool InMemoryDB::write(Index dbindex, const std::string& key, const std::string& value)
{
    if (write_in_tx(dbindex, key, value)) {
        return true;
    }

    std::lock_guard<MutexType> lg(mtx);

    return write_unsafe(dbindex, key, value);
}

bool InMemoryDB::erase_unsafe(Index dbindex, const std::string& key)
{
    data[static_cast<std::size_t>(dbindex)].erase(key);

    return true;
}

bool InMemoryDB::erase_in_tx(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key);
    }
    return false;
}

bool InMemoryDB::erase(Index dbindex, const std::string& key)
{
    if (erase_in_tx(dbindex, key)) {
        return true;
    }

    std::lock_guard<MutexType> lg(mtx);

    return erase_unsafe(dbindex, key);
}

bool InMemoryDB::eraseAll(Index dbindex, const std::string& key)
{
    if (erase_in_tx(dbindex, key)) {
        return true;
    }

    std::lock_guard<MutexType> lg(mtx);

    return erase_unsafe(dbindex, key);
}

bool InMemoryDB::exists(Index dbindex, const std::string& key) const
{
    if (tx) {
        const auto& op = tx->getOp(static_cast<int>(dbindex), key);
        if (op) {
            switch (op->getOpType()) {
            case TransactionOperation::UniqueSet:
            case TransactionOperation::Append:
                if (!op->getValues().empty()) {
                    return true;
                }
                break;
            case TransactionOperation::Erase:
                return false;
            }
        }
    }

    std::lock_guard<MutexType> lg(mtx);

    const std::map<std::string, std::vector<std::string>>& kvMap =
        data[static_cast<std::size_t>(dbindex)];

    auto it_key = kvMap.find(key);
    if (it_key == kvMap.cend()) {
        return false;
    }

    return true;
}

void InMemoryDB::clearDBData()
{
    tx.reset();
    std::lock_guard<MutexType> lg(mtx);
    for (auto&& m : data) {
        m.clear();
    }
}

bool InMemoryDB::beginDBTransaction(std::size_t /*expectedDataSize*/)
{
    if (tx) {
        return false;
    }
    tx = std::unique_ptr<HierarchicalDB>(new HierarchicalDB(""));
    return true;
}

bool InMemoryDB::commitDBTransaction()
{
    if (!tx) {
        return false;
    }

    std::unique_ptr<HierarchicalDB> movedTx = std::move(tx);
    tx.reset();

    std::map<IDB::Index, std::map<std::string, TransactionOperation>> txData =
        GetAllTxData(movedTx.get());

    bool result = true;

    std::lock_guard<MutexType> lg(mtx);

    for (auto&& dbid_pair : txData) {
        const IDB::Index                             dbid    = dbid_pair.first;
        std::map<std::string, TransactionOperation>& txOpMap = dbid_pair.second;
        for (const auto& kv_pair : txOpMap) {
            const std::string&          key = kv_pair.first;
            const TransactionOperation& op  = kv_pair.second;

            switch (op.getOpType()) {
            case TransactionOperation::Append:
                for (const auto& val : op.getValues()) {
                    result = result && write_unsafe(dbid, key, val);
                }
                break;
            case TransactionOperation::UniqueSet:
                if (!op.getValues().empty()) {
                    result = result && write_unsafe(dbid, key, op.getValues().front());
                }
                break;
            case TransactionOperation::Erase:
                result = result && erase_unsafe(dbid, key);
                break;
            }
        }
    }
    return result;
}

bool InMemoryDB::abortDBTransaction()
{
    if (!tx) {
        return false;
    }
    tx.reset();
    return true;
}

boost::optional<boost::filesystem::path> InMemoryDB::getDataDir() const { return boost::none; }

bool InMemoryDB::openDB(bool clearDataBeforeOpen)
{
    if (clearDataBeforeOpen) {
        clearDBData();
    }
    return true;
}

void InMemoryDB::close() {}

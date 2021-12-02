#include "inmemorydb.h"

using namespace DBOperation;

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

InMemoryDB::InMemoryDB(bool startNewDatabase)
{
    if (startNewDatabase) {
        InMemoryDB::clearDBData();
    }
}

template <typename MutexType>
static std::map<IDB::Index, std::map<std::string, TransactionOperation>>
GetAllTxData(HierarchicalDB<MutexType>* tx)
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

Result<boost::optional<std::string>, int>
InMemoryDB::read(Index dbindex, const std::string& key, std::size_t offset,
                 const boost::optional<std::size_t>& size) const
{
    if (tx) {
        const auto& op = tx->getOp(static_cast<int>(dbindex), key);
        if (op) {
            switch (op->getOpType()) {
            case WriteOperationType::UniqueSet:
            case WriteOperationType::Append:
                if (!op->getValues().empty()) {
                    if (size) {
                        return Ok(boost::make_optional(op->getValues().front().substr(offset, *size)));
                    } else {
                        return Ok(boost::make_optional(op->getValues().front().substr(offset)));
                    }
                }
                break;
            case WriteOperationType::Erase:
                return Ok(boost::optional<std::string>());
            }
        }
    }

    std::lock_guard<MutexType> lg(mtx);

    const std::map<std::string, std::vector<std::string>>& kvMap =
        data[static_cast<std::size_t>(dbindex)];

    auto it_key = kvMap.find(key);
    if (it_key == kvMap.cend()) {
        return Ok(boost::optional<std::string>());
    }

    const std::vector<std::string>& vec = it_key->second;

    if (vec.empty()) {
        return Ok(boost::optional<std::string>());
    }

    if (size) {
        return Ok(boost::make_optional(vec.front().substr(offset, *size)));
    } else {
        return Ok(boost::make_optional(vec.front().substr(offset)));
    }
}

Result<std::vector<std::string>, int> InMemoryDB::readMultiple(Index              dbindex,
                                                               const std::string& key) const
{
    std::vector<std::string> valuesToAppend;

    if (tx) {
        const auto& op = tx->getOp(static_cast<int>(dbindex), key);
        if (op) {
            switch (op->getOpType()) {
            case WriteOperationType::UniqueSet:
            case WriteOperationType::Append: {
                valuesToAppend = op->getValues();
                break;
            case WriteOperationType::Erase: {
                return Ok(std::vector<std::string>());
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
        return Ok(std::move(valuesToAppend));
    }

    std::vector<std::string> vec = it_key->second;

    vec.insert(vec.end(), std::make_move_iterator(valuesToAppend.begin()),
               std::make_move_iterator(valuesToAppend.end()));

    return Ok(std::move(vec));
}

static void MergeTxDataWithData(std::map<std::string, std::vector<std::string>>& dataMap,
                                std::map<std::string, TransactionOperation>&&    txData)
{
    for (auto&& p : txData) {
        auto&& key  = p.first;
        auto&& txOp = p.second;

        switch (txOp.getOpType()) {
        case WriteOperationType::Append:
            dataMap[key].insert(dataMap[key].end(), std::make_move_iterator(txOp.getValues().begin()),
                                std::make_move_iterator(txOp.getValues().end()));
            break;
        case WriteOperationType::UniqueSet:
            if (!txOp.getValues().empty()) {
                dataMap[key] = txOp.getValues();
            }
            break;
        case WriteOperationType::Erase:
            dataMap.erase(key);
            break;
        }
    }
}

Result<std::map<std::string, std::vector<std::string>>, int> InMemoryDB::readAll(Index dbindex) const
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

    return Ok(std::move(finalData));
}

static void MergeTxDataWithData(std::map<std::string, std::string>&           dataMap,
                                std::map<std::string, TransactionOperation>&& txData)
{
    for (auto&& p : txData) {
        auto&& key  = p.first;
        auto&& txOp = p.second;

        switch (txOp.getOpType()) {
        case WriteOperationType::Append:
        case WriteOperationType::UniqueSet:
            if (!txOp.getValues().empty()) {
                dataMap[key] = txOp.getValues().front();
            }
            break;
        case WriteOperationType::Erase:
            dataMap.erase(key);
            break;
        }
    }
}

Result<std::map<std::string, std::string>, int> InMemoryDB::readAllUnique(Index dbindex) const
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

    return Ok(finalData);
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

Result<void, int> InMemoryDB::write_unsafe(Index dbindex, const std::string& key,
                                           const std::string& value)
{
    if (write_in_tx(dbindex, key, value)) {
        return Ok();
    }

    if (IDB::DuplicateKeysAllowed(dbindex)) {
        data[static_cast<std::size_t>(dbindex)][key].push_back(value);
    } else [[likely]] {
        data[static_cast<std::size_t>(dbindex)][key] = std::vector<std::string>(1, value);
    }
    return Ok();
}

Result<void, int> InMemoryDB::write(Index dbindex, const std::string& key, const std::string& value)
{
    if (write_in_tx(dbindex, key, value)) {
        return Ok();
    }

    std::lock_guard<MutexType> lg(mtx);

    return write_unsafe(dbindex, key, value);
}

Result<void, int> InMemoryDB::erase_unsafe(Index dbindex, const std::string& key)
{
    data[static_cast<std::size_t>(dbindex)].erase(key);

    return Ok();
}

bool InMemoryDB::erase_in_tx(Index dbindex, const std::string& key)
{
    if (tx) {
        return tx->erase(static_cast<int>(dbindex), key);
    }
    return false;
}

Result<void, int> InMemoryDB::erase(Index dbindex, const std::string& key)
{
    if (erase_in_tx(dbindex, key)) {
        return Ok();
    }

    std::lock_guard<MutexType> lg(mtx);

    return erase_unsafe(dbindex, key);
}

Result<void, int> InMemoryDB::eraseAll(Index dbindex, const std::string& key)
{
    if (erase_in_tx(dbindex, key)) {
        return Ok();
    }

    std::lock_guard<MutexType> lg(mtx);

    return erase_unsafe(dbindex, key);
}

Result<bool, int> InMemoryDB::exists(Index dbindex, const std::string& key) const
{
    if (tx) {
        const auto& op = tx->getOp(static_cast<int>(dbindex), key);
        if (op) {
            switch (op->getOpType()) {
            case WriteOperationType::UniqueSet:
            case WriteOperationType::Append:
                if (!op->getValues().empty()) {
                    return Ok(true);
                }
                break;
            case WriteOperationType::Erase:
                return Ok(false);
            }
        }
    }

    std::lock_guard<MutexType> lg(mtx);

    const std::map<std::string, std::vector<std::string>>& kvMap =
        data[static_cast<std::size_t>(dbindex)];

    auto it_key = kvMap.find(key);
    if (it_key == kvMap.cend()) {
        return Ok(false);
    }

    return Ok(true);
}

void InMemoryDB::clearDBData()
{
    tx.reset();
    std::lock_guard<MutexType> lg(mtx);
    for (auto&& m : data) {
        m.clear();
    }
}

Result<void, int> InMemoryDB::beginDBTransaction(std::size_t /*expectedDataSize*/)
{
    if (tx) {
        return Err(-1);
    }
    tx = std::unique_ptr<HierarchicalDB<decltype(tx)::element_type::MutexT>>(
        new HierarchicalDB<decltype(tx)::element_type::MutexT>(""));
    return Ok();
}

Result<void, int> InMemoryDB::commitDBTransaction()
{
    if (!tx) {
        return Err(-1);
    }

    std::unique_ptr<HierarchicalDB<decltype(tx)::element_type::MutexT>> movedTx = std::move(tx);
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
            case WriteOperationType::Append:
                for (const auto& val : op.getValues()) {
                    result = result && write_unsafe(dbid, key, val).isOk();
                }
                break;
            case WriteOperationType::UniqueSet:
                if (!op.getValues().empty()) {
                    result = result && write_unsafe(dbid, key, op.getValues().front()).isOk();
                }
                break;
            case WriteOperationType::Erase:
                result = result && erase_unsafe(dbid, key).isOk();
                break;
            }
        }
    }
    return result ? Result<void, int>(Ok()) : Err(-1);
}

bool InMemoryDB::abortDBTransaction()
{
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

#include "hierarchicaldb.h"

#include <boost/range/adaptor/reversed.hpp>

[[nodiscard]] static boost::optional<std::string>
extractValueForUniqueGet(const TransactionOperation& operation, std::size_t offset,
                         const boost::optional<std::size_t>& size)
{
    switch (operation.getOpType()) {
    case TransactionOperation::OperationType::Erase:
        return boost::none;
    case TransactionOperation::OperationType::Append:
    case TransactionOperation::OperationType::UniqueSet:
        if (!operation.getValues().empty()) {
            if (size.is_initialized()) {
                return operation.getValues().front().substr(offset, *size);
            } else {
                return operation.getValues().front().substr(offset);
            }
        } else {
            return boost::none;
        }
    }
    throw std::runtime_error("Unhandled OperationType: " + std::to_string(operation.getOpType()));
}

[[nodiscard]] static bool extractValueForExists(const TransactionOperation& operation)
{
    switch (operation.getOpType()) {
    case TransactionOperation::OperationType::UniqueSet:
    case TransactionOperation::OperationType::Append:
        return true;
    case TransactionOperation::OperationType::Erase:
        return false;
    }
    throw std::runtime_error("Unhandled OperationType: " + std::to_string(operation.getOpType()));
}

[[nodiscard]] static std::vector<std::string>
extractValueForMultiGetAllWithKey(const TransactionOperation& val)
{
    switch (val.getOpType()) {
    case TransactionOperation::OperationType::Append:
    case TransactionOperation::OperationType::UniqueSet:
        return val.getValues();
    case TransactionOperation::OperationType::Erase:
        return {};
    }
    throw std::runtime_error("Unhandled OperationType: " + std::to_string(val.getOpType()));
}

const char* HierarchicalDB::CommitErrorToString(HierarchicalDB::CommitError err)
{
    switch (err) {
    case CommitError::AlreadyCommitted:
        return "AlreadyCommitted";
    case CommitError::UncommittedChildren:
        return "UncommittedChildren";
    case CommitError::Conflict:
        return "Conflict";
    }
    return "Unknown";
}

std::pair<HierarchicalDB*, boost::optional<boost::unique_lock<HierarchicalDB::MutexType>>>
HierarchicalDB::getLockedInstanceToModify()
{
    boost::optional<boost::unique_lock<decltype(mtx)>> lg1 =
        boost::make_optional(boost::unique_lock<decltype(mtx)>(mtx));
    HierarchicalDB* instance = nullptr;

    // if there's transactions committed, we use the last one, otherwise, this object
    instance = (committedTransactions.empty() ? this : committedTransactions.back().get());

    boost::optional<boost::unique_lock<decltype(mtx)>> lg2;
    if (instance != this) {
        lg2 = boost::make_optional(boost::unique_lock<decltype(mtx)>(instance->mtx));
        lg1->unlock();
        return std::make_pair(instance, std::move(lg2));
    }
    return std::make_pair(instance, std::move(lg1));
}

std::size_t HierarchicalDB::calculateParentsCommittedTxsOnStart(const HierarchicalDB* parentDB)
{
    if (parentDB) {
        const std::vector<HierarchicalDB::Ptr>& parentsTxs = parentDB->committedTransactions;
        return parentsTxs.empty() ? 0 : parentsTxs.size() - 1;
    }
    return 0;
}

std::map<std::string, TransactionOperation> HierarchicalDB::getAllOpsOfThisTx(int dbid) const
{
    return data[dbid];
}

std::map<std::string, std::vector<TransactionOperation>>
HierarchicalDB::getCommittedAllOpsMap(int dbid, bool lookIntoParent,
                                      std::size_t lastCommittedTransaction) const
{
    std::map<std::string, std::vector<TransactionOperation>> result;

    // we get all the relevant data from the parent, recursively
    if (parent && lookIntoParent) {
        result = parent->getCommittedAllOpsMap(dbid, true, parentCommittedTxsOnStart);
    }

    // then we get it from this instance
    {
        const std::map<std::string, TransactionOperation> thisVal = getAllOpsOfThisTx(dbid);
        for (auto&& kv : thisVal) {
            result[kv.first].push_back(kv.second);
        }
    }

    // then we get it from committed trnsactions, recursively
    std::vector<TransactionOperation> txsResult;
    for (std::size_t i = 0; i < committedTransactions.size() && i < lastCommittedTransaction; i++) {
        const auto& tx = committedTransactions[i];

        std::map<std::string, std::vector<TransactionOperation>> subResult =
            tx->getCommittedAllOpsMap(dbid, false);
        for (auto&& kv : subResult) {
            const std::string&                 key = kv.first;
            std::vector<TransactionOperation>& ops = kv.second;
            result[key].insert(result[key].end(), std::make_move_iterator(ops.begin()),
                               std::make_move_iterator(ops.end()));
        }
    }

    return result;
}

template <typename T>
struct skip
{
    T&          t;
    std::size_t n;
    skip(T& v, std::size_t s) : t(v), n(s) {}
    auto begin() -> decltype(std::begin(t)) { return std::next(std::begin(t), n); }
    auto end() -> decltype(std::end(t)) { return std::end(t); }
};

std::map<std::string, TransactionOperation>
HierarchicalDB::collapseAllOpsMap(std::map<std::string, std::vector<TransactionOperation>>&& opsMap)
{
    std::map<std::string, TransactionOperation> result;
    for (auto&& opsPair : opsMap) {
        const std::string&                 key = opsPair.first;
        std::vector<TransactionOperation>& ops = opsPair.second;

        TransactionOperation& subResult = ops.front();

        for (auto&& op : skip<decltype(ops)>(ops, 1)) {
            TransactionOperation::collapseOperations(subResult, op);
        }
        result.insert(std::make_pair(key, std::move(subResult)));
    }
    return result;
}

std::map<std::string, TransactionOperation> HierarchicalDB::getCollapsedOpsForAll(int dbid) const
{

    std::map<std::string, std::vector<TransactionOperation>> allOps = getCommittedAllOpsMap(dbid, true);
    return collapseAllOpsMap(std::move(allOps));
}

std::vector<TransactionOperation>
HierarchicalDB::getCommittedOpVec(int dbid, const std::string& key, bool lookIntoParent,
                                  std::size_t lastCommittedTransaction) const
{
    std::vector<TransactionOperation> result;

    // get related data from parents, recursively
    if (parent && lookIntoParent) {
        result = parent->getCommittedOpVec(dbid, key, true, parentCommittedTxsOnStart);
    }

    // we lock only this instance, and we depend on getCommittedOpVec() calls to lock the other instances
    // in parents and children
    boost::lock_guard<MutexType> lg(mtx);

    // get related data from this instance
    {

        const boost::optional<TransactionOperation> thisVal = getOpOfThisTx(dbid, key);
        if (thisVal) {
            result.push_back(*thisVal);
        }
    }

    // get related data from committed transactions, recursively
    {
        std::vector<TransactionOperation> txsResult;
        for (std::size_t i = 0; i < committedTransactions.size() && i < lastCommittedTransaction; i++) {
            const auto& tx = committedTransactions[i];

            std::vector<TransactionOperation> subResult = tx->getCommittedOpVec(dbid, key, false);
            result.insert(result.end(), std::make_move_iterator(subResult.begin()),
                          std::make_move_iterator(subResult.end()));
        }
    }

    return result;
}

boost::optional<TransactionOperation> HierarchicalDB::getOpOfThisTx(int                dbid,
                                                                    const std::string& key) const
{
    const auto& kvMap = data[dbid];
    auto        it_kv = kvMap.find(key);
    if (it_kv == kvMap.cend()) {
        return boost::none;
    }
    return boost::make_optional(it_kv->second);
}

boost::optional<TransactionOperation>
HierarchicalDB::collapseOpsVec(std::vector<TransactionOperation>&& ops)
{
    if (ops.empty()) {
        return boost::none;
    }

    TransactionOperation& result = ops.front();

    for (auto&& op : skip<decltype(ops)>(ops, 1)) {
        TransactionOperation::collapseOperations(result, op);
    }

    return boost::make_optional(std::move(result));
}

boost::optional<TransactionOperation> HierarchicalDB::getCollapsedOps(int                dbid,
                                                                      const std::string& key) const
{
    std::vector<TransactionOperation> allOps = getCommittedOpVec(dbid, key, true);
    return collapseOpsVec(std::move(allOps));
}

HierarchicalDB::HierarchicalDB(const std::string& name, const std::shared_ptr<HierarchicalDB>& parentDB)
    : parent(parentDB), parentCommittedTxsOnStart(calculateParentsCommittedTxsOnStart(parentDB.get())),
      dbName(name)
{
}

HierarchicalDB::~HierarchicalDB()
{
    if (!committed) {
        cancel();
    }
}

bool HierarchicalDB::unique_set(int dbid, const std::string& key, const std::string& value)
{
    assertNotCommitted();

    auto            instanceLocked = getLockedInstanceToModify();
    HierarchicalDB* instance       = instanceLocked.first;

    instance->data[dbid].erase(key);
    instance->data[dbid].insert(std::make_pair(
        key, TransactionOperation(TransactionOperation::OperationType::UniqueSet, value)));
    return true;
}

boost::optional<std::string> HierarchicalDB::unique_get(int dbid, const std::string& key,
                                                        std::size_t                         offset,
                                                        const boost::optional<std::size_t>& size) const
{
    const boost::optional<TransactionOperation>& resultOp = getCollapsedOps(dbid, key);

    if (!resultOp) {
        return boost::none;
    }

    return extractValueForUniqueGet(*resultOp, offset, size);
}

boost::optional<TransactionOperation> HierarchicalDB::getOp(int dbid, const std::string& key) const
{
    return getCollapsedOps(dbid, key);
}

bool HierarchicalDB::exists(int dbid, const std::string& key) const
{
    const boost::optional<TransactionOperation>& resultOp = getCollapsedOps(dbid, key);

    if (!resultOp) {
        return false;
    }

    return extractValueForExists(*resultOp);
}

bool HierarchicalDB::erase(int dbid, const std::string& key)
{
    assertNotCommitted();

    auto            instanceLocked = getLockedInstanceToModify();
    HierarchicalDB* instance       = instanceLocked.first;

    instance->data[dbid].erase(key);
    instance->data[dbid].insert(
        std::make_pair(key, TransactionOperation(TransactionOperation::OperationType::Erase, key)));
    return true;
}

bool HierarchicalDB::multi_append(int dbid, const std::string& key, const std::string& value)
{
    assertNotCommitted();

    auto            instanceLocked = getLockedInstanceToModify();
    HierarchicalDB* instance       = instanceLocked.first;

    // we find the key
    auto it = data[dbid].find(key);
    if (it == data[dbid].end()) {
        // key doesn't exist, let's add it
        instance->data[dbid].insert(std::make_pair(
            key, TransactionOperation(TransactionOperation::OperationType::Append, value)));
        return true;
    }

    // the key exists, let's insert the value
    switch (it->second.getOpType()) {
    case TransactionOperation::OperationType::Append:
        it->second.getValues().push_back(value);
        break;
    case TransactionOperation::OperationType::Erase:
    case TransactionOperation::OperationType::UniqueSet:
        instance->data[dbid].insert(std::make_pair(
            key, TransactionOperation(TransactionOperation::OperationType::Append, value)));
    }
    return true;
}

std::vector<std::string> HierarchicalDB::multi_getAllWithKey(int dbid, const std::string& key) const
{
    assertNotCommitted();
    const boost::optional<TransactionOperation>& resultOp = getCollapsedOps(dbid, key);

    if (!resultOp) {
        return {};
    }

    return extractValueForMultiGetAllWithKey(*resultOp);
}

std::map<std::string, std::vector<std::string>> HierarchicalDB::multi_getAll(int dbid) const
{
    assertNotCommitted();

    std::map<std::string, TransactionOperation>&& collapsed = getCollapsedOpsForAll(dbid);

    std::map<std::string, std::vector<std::string>> result;

    for (auto&& colPair : collapsed) {
        auto&& key = colPair.first;
        auto&& val = extractValueForMultiGetAllWithKey(colPair.second);
        if (!val.empty()) {
            result[key] = val;
        }
    }

    return result;
}

void HierarchicalDB::revert()
{
    for (auto&& m : data) {
        m.clear();
    }
}

Result<void, HierarchicalDB::CommitError> HierarchicalDB::commit()
{
    if (committed) {
        return Err(CommitError::AlreadyCommitted);
    }
    if (doesParentHaveUncommittedChildren()) {
        return Err(CommitError::UncommittedChildren);
    }

    // we don't commit the root
    if (parent) {
        // we go through the parent txs since we started this tx, and we search for conflicts
        for (std::size_t i = parentCommittedTxsOnStart; i < parent->committedTransactions.size(); i++) {
            const std::size_t          index       = parent->committedTransactions.size() - i - 1;
            const HierarchicalDB::Ptr& committedTx = parent->committedTransactions[index];

            // for every commited after this, we search for the same keys, and ensure the same keys were
            // not changed

            // loop over everything that this tx changed, and ensure it doesn't conflict with anything in
            // the previously committed txs
            for (std::size_t dbid = 0; dbid < data.size(); dbid++) {
                const std::map<std::string, TransactionOperation>& db_map = data[dbid];
                for (const auto& kv_pair : db_map) {
                    const std::string& key = kv_pair.first;
                    // check if any of the kv_pair's were modified in
                    const auto& parent_stored_tx_data = committedTx->data;

                    auto it_key = parent_stored_tx_data[dbid].find(key);
                    if (it_key != parent_stored_tx_data[dbid].cend()) {
                        // the same key exists in a committed transaction as the one in this one;
                        // unnacceptable conflict
                        return Err(CommitError::Conflict);
                    }
                }
            }
        }
        parent->committedTransactions.push_back(shared_from_this());

        // we push another transaction on commit as the one to be modified in the parent when modifying
        // functions are called
        auto separatorTransaction = HierarchicalDB::Make(dbName + "-Separator", shared_from_this());
        separatorTransaction->committed = true; // this transaction is already committed
        parent->committedTransactions.push_back(std::move(separatorTransaction));

        cancel();
    }
    return Ok();
}

void HierarchicalDB::cancel()
{
    if (committed) {
        return;
    }

    if (parent) {
        parent->openTransactions.fetch_sub(1);
        assert(parent->openTransactions >= 0);
    }
    committed = true;
}

bool HierarchicalDB::doesParentHaveUncommittedChildren() const
{
    // the db root is an exception because it doesn't need to be committed
    if (parent && openTransactions.load() > 1) {
        return true;
    }
    return false;
}

std::shared_ptr<HierarchicalDB> HierarchicalDB::startDBTransaction(const std::string& txName)
{
    openTransactions.fetch_add(1);
    // we push a new empty transaction to make all the changes from this instance after this point go to
    // this new transaction instead of the local data
    //    committedTransactions.push_back(HierarchicalDB::Make(txName + "-StartSep",
    //    shared_from_this()));
    return HierarchicalDB::Ptr(new HierarchicalDB(dbName + "-" + txName, shared_from_this()));
}

void HierarchicalDB::assertNotCommitted() const
{
    if (committed) {
        throw std::runtime_error("This transaction is locked after having been committed");
    }
}

HierarchicalDB::Ptr HierarchicalDB::Make(const std::string&                     name,
                                         const std::shared_ptr<HierarchicalDB>& parentDB)
{
    return HierarchicalDB::Ptr(new HierarchicalDB(name, parentDB));
}

int_fast32_t HierarchicalDB::getOpenTransactionsCount() const { return openTransactions.load(); }

std::map<std::string, TransactionOperation> HierarchicalDB::getAllDataForDB(int dbid) const
{
    return getCollapsedOpsForAll(dbid);
}

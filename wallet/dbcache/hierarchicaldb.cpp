#include "hierarchicaldb.h"

#include <array>
#include <boost/range/adaptor/reversed.hpp>
#include <utility>

using namespace DBOperation;

[[nodiscard]] static boost::optional<std::string>
extractValueForUniqueGet(const TransactionOperation& operation, std::size_t offset,
                         const boost::optional<std::size_t>& size)
{
    switch (operation.getOpType()) {
    case WriteOperationType::Erase:
        return boost::none;
    case WriteOperationType::Append:
    case WriteOperationType::UniqueSet:
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
    throw std::runtime_error("Unhandled OperationType: " +
                             std::to_string(static_cast<int>(operation.getOpType())));
}

[[nodiscard]] static bool extractValueForExists(const TransactionOperation& operation)
{
    switch (operation.getOpType()) {
    case WriteOperationType::UniqueSet:
    case WriteOperationType::Append:
        return true;
    case WriteOperationType::Erase:
        return false;
    }
    throw std::runtime_error("Unhandled OperationType: " +
                             std::to_string(static_cast<int>(operation.getOpType())));
}

[[nodiscard]] static std::vector<std::string>
extractValueForMultiGetAllWithKey(const TransactionOperation& val)
{
    switch (val.getOpType()) {
    case WriteOperationType::Append:
    case WriteOperationType::UniqueSet:
        return val.getValues();
    case WriteOperationType::Erase:
        return {};
    }
    throw std::runtime_error("Unhandled OperationType: " +
                             std::to_string(static_cast<int>(val.getOpType())));
}

template <typename MutexType>
const char* HierarchicalDB<MutexType>::CommitErrorToString(HierarchicalDB<MutexType>::CommitError err)
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

template <typename MutexType>
std::pair<HierarchicalDB<MutexType>*, std::unique_ptr<boost::unique_lock<MutexType>>>
HierarchicalDB<MutexType>::getLockedInstanceToModify()
{
    std::unique_ptr<boost::unique_lock<MutexType>> lg1 =
        std::unique_ptr<boost::unique_lock<MutexType>>(new boost::unique_lock<MutexType>(mtx));
    HierarchicalDB* instance = nullptr;

    // if there's transactions committed, we use the last one, otherwise, this object
    instance = (committedTransactions.empty() ? this : committedTransactions.back().get());

    std::unique_ptr<boost::unique_lock<MutexType>> lg2;
    if (instance != this) {
        lg2 = std::unique_ptr<boost::unique_lock<MutexType>>(
            new boost::unique_lock<MutexType>(instance->mtx));
        lg1->unlock();
        return std::make_pair(instance, std::move(lg2));
    }
    return std::make_pair(instance, std::move(lg1));
}

template <typename MutexType>
std::size_t
HierarchicalDB<MutexType>::calculateParentsCommittedTxsOnStart(const HierarchicalDB* parentDB)
{
    if (parentDB) {
        const std::vector<HierarchicalDB<MutexType>::Ptr>& parentsTxs = parentDB->committedTransactions;
        return parentsTxs.empty() ? 0 : parentsTxs.size() - 1;
    }
    return 0;
}

template <typename MutexType>
std::map<std::string, TransactionOperation> HierarchicalDB<MutexType>::getAllOpsOfThisTx(int dbid) const
{
    return data[dbid];
}

template <typename MutexType>
std::map<std::string, std::vector<TransactionOperation>>
HierarchicalDB<MutexType>::getCommittedAllOpsMap(int dbid, bool lookIntoParent,
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

template <typename MutexType>
std::map<std::string, TransactionOperation> HierarchicalDB<MutexType>::collapseAllOpsMap(
    std::map<std::string, std::vector<TransactionOperation>>&& opsMap)
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

template <typename MutexType>
std::map<std::string, TransactionOperation>
HierarchicalDB<MutexType>::getCollapsedOpsForAll(int dbid) const
{

    std::map<std::string, std::vector<TransactionOperation>> allOps = getCommittedAllOpsMap(dbid, true);
    return collapseAllOpsMap(std::move(allOps));
}

template <typename MutexType>
std::vector<TransactionOperation>
HierarchicalDB<MutexType>::getCommittedOpVec(int dbid, const std::string& key, bool lookIntoParent,
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

template <typename MutexType>
boost::optional<TransactionOperation>
HierarchicalDB<MutexType>::getOpOfThisTx(int dbid, const std::string& key) const
{
    const auto& kvMap = data[dbid];
    auto        it_kv = kvMap.find(key);
    if (it_kv == kvMap.cend()) {
        return boost::none;
    }
    return boost::make_optional(it_kv->second);
}

template <typename MutexType>
boost::optional<TransactionOperation>
HierarchicalDB<MutexType>::collapseOpsVec(std::vector<TransactionOperation>&& ops)
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

template <typename MutexType>
boost::optional<TransactionOperation>
HierarchicalDB<MutexType>::getCollapsedOps(int dbid, const std::string& key) const
{
    std::vector<TransactionOperation> allOps = getCommittedOpVec(dbid, key, true);
    return collapseOpsVec(std::move(allOps));
}

template <typename MutexType>
HierarchicalDB<MutexType>::HierarchicalDB(const std::string&                     name,
                                          const std::shared_ptr<HierarchicalDB>& parentDB)
    : parent(parentDB), parentCommittedTxsOnStart(calculateParentsCommittedTxsOnStart(parentDB.get())),
      dbName(name)
{
}

template <typename MutexType>
HierarchicalDB<MutexType>::~HierarchicalDB()
{
    if (!committed) {
        cancel();
    }
}

template <typename MutexType>
bool HierarchicalDB<MutexType>::unique_set(int dbid, const std::string& key, const std::string& value)
{
    assertNotCommitted();

    auto            instanceLocked = getLockedInstanceToModify();
    HierarchicalDB* instance       = instanceLocked.first;

    instance->data[dbid].erase(key);
    instance->data[dbid].insert(
        std::make_pair(key, TransactionOperation(WriteOperationType::UniqueSet, value)));
    return true;
}

template <typename MutexType>
boost::optional<std::string>
HierarchicalDB<MutexType>::unique_get(int dbid, const std::string& key, std::size_t offset,
                                      const boost::optional<std::size_t>& size) const
{
    const boost::optional<TransactionOperation>& resultOp = getCollapsedOps(dbid, key);

    if (!resultOp) {
        return boost::none;
    }

    return extractValueForUniqueGet(*resultOp, offset, size);
}

template <typename MutexType>
boost::optional<TransactionOperation> HierarchicalDB<MutexType>::getOp(int                dbid,
                                                                       const std::string& key) const
{
    return getCollapsedOps(dbid, key);
}

template <typename MutexType>
bool HierarchicalDB<MutexType>::exists(int dbid, const std::string& key) const
{
    const boost::optional<TransactionOperation>& resultOp = getCollapsedOps(dbid, key);

    if (!resultOp) {
        return false;
    }

    return extractValueForExists(*resultOp);
}

template <typename MutexType>
bool HierarchicalDB<MutexType>::erase(int dbid, const std::string& key)
{
    assertNotCommitted();

    auto            instanceLocked = getLockedInstanceToModify();
    HierarchicalDB* instance       = instanceLocked.first;

    instance->data[dbid].erase(key);
    instance->data[dbid].insert(
        std::make_pair(key, TransactionOperation(WriteOperationType::Erase, key)));
    return true;
}

template <typename MutexType>
bool HierarchicalDB<MutexType>::multi_append(int dbid, const std::string& key, const std::string& value)
{
    assertNotCommitted();

    auto            instanceLocked = getLockedInstanceToModify();
    HierarchicalDB* instance       = instanceLocked.first;

    // we find the key
    auto it = instance->data[dbid].find(key);
    if (it == instance->data[dbid].end()) {
        // key doesn't exist, let's add it
        instance->data[dbid].insert(
            std::make_pair(key, TransactionOperation(WriteOperationType::Append, value)));
        return true;
    }

    // the key exists, let's insert the value
    switch (it->second.getOpType()) {
    case WriteOperationType::Append:
        it->second.getValues().push_back(value);
        break;
    case WriteOperationType::Erase:
    case WriteOperationType::UniqueSet:
        instance->data[dbid].insert(
            std::make_pair(key, TransactionOperation(WriteOperationType::Append, value)));
    }
    return true;
}

template <typename MutexType>
std::vector<std::string> HierarchicalDB<MutexType>::multi_getAllWithKey(int                dbid,
                                                                        const std::string& key) const
{
    assertNotCommitted();
    const boost::optional<TransactionOperation>& resultOp = getCollapsedOps(dbid, key);

    if (!resultOp) {
        return {};
    }

    return extractValueForMultiGetAllWithKey(*resultOp);
}

template <typename MutexType>
std::map<std::string, std::vector<std::string>> HierarchicalDB<MutexType>::multi_getAll(int dbid) const
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

template <typename MutexType>
void HierarchicalDB<MutexType>::revert()
{
    for (auto&& m : data) {
        m.clear();
    }
}

template <typename MutexType>
Result<void, typename HierarchicalDB<MutexType>::CommitError> HierarchicalDB<MutexType>::commit()
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
            const std::size_t                     index = parent->committedTransactions.size() - i - 1;
            const HierarchicalDB<MutexType>::Ptr& committedTx = parent->committedTransactions[index];

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
        parent->committedTransactions.push_back(HierarchicalDB<MutexType>::shared_from_this());

        // we push another transaction on commit as the one to be modified in the parent when modifying
        // functions are called
        auto separatorTransaction = HierarchicalDB<MutexType>::Make(
            dbName + "-Separator", HierarchicalDB<MutexType>::shared_from_this());
        separatorTransaction->committed = true; // this transaction is already committed
        parent->committedTransactions.push_back(std::move(separatorTransaction));

        cancel();
    }
    return Ok();
}

template <typename MutexType>
void HierarchicalDB<MutexType>::cancel()
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

template <typename MutexType>
bool HierarchicalDB<MutexType>::doesParentHaveUncommittedChildren() const
{
    // the db root is an exception because it doesn't need to be committed
    if (parent && openTransactions.load() > 1) {
        return true;
    }
    return false;
}

template <typename MutexType>
std::shared_ptr<HierarchicalDB<MutexType>>
HierarchicalDB<MutexType>::startDBTransaction(const std::string& txName)
{
    openTransactions.fetch_add(1);
    // we push a new empty transaction to make all the changes from this instance after this point go to
    // this new transaction instead of the local data
    //    committedTransactions.push_back(HierarchicalDB<MutexType>::Make(txName + "-StartSep",
    //    shared_from_this()));
    return HierarchicalDB<MutexType>::Ptr(
        new HierarchicalDB(dbName + "-" + txName, HierarchicalDB<MutexType>::shared_from_this()));
}

template <typename MutexType>
void HierarchicalDB<MutexType>::assertNotCommitted() const
{
    if (committed) {
        throw std::runtime_error("This transaction is locked after having been committed");
    }
}

template <typename MutexType>
typename HierarchicalDB<MutexType>::Ptr
HierarchicalDB<MutexType>::Make(const std::string&                                name,
                                const std::shared_ptr<HierarchicalDB<MutexType>>& parentDB)
{
    return HierarchicalDB<MutexType>::Ptr(new HierarchicalDB(name, parentDB));
}

template <typename MutexType>
int_fast32_t HierarchicalDB<MutexType>::getOpenTransactionsCount() const
{
    return openTransactions.load();
}

template <typename MutexType>
std::map<std::string, TransactionOperation> HierarchicalDB<MutexType>::getAllDataForDB(int dbid) const
{
    return getCollapsedOpsForAll(dbid);
}

template <typename MutexType>
std::vector<std::pair<IDB::Index, std::map<std::string, TransactionOperation>>>
HierarchicalDB<MutexType>::getAllData() const
{
    std::vector<std::pair<IDB::Index, std::map<std::string, TransactionOperation>>> result;
    for (int dbid = 0; dbid < static_cast<int>(IDB::Index::Index_Last); dbid++) {
        auto&& subRes = getAllDataForDB(dbid);
        if (subRes.size() > 0) {
            result.push_back(std::make_pair(static_cast<IDB::Index>(dbid), subRes));
        }
    }
    return result;
}

template class HierarchicalDB<std::mutex>;
template class HierarchicalDB<hdb_dummy_mutex>;

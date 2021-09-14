#ifndef HIERARCHICALDB_H
#define HIERARCHICALDB_H

#include "db/idb.h"
#include "result.h"
#include "transactionoperation.h"
#include <boost/atomic.hpp>
#include <boost/optional.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

class HierarchicalDB : public std::enable_shared_from_this<HierarchicalDB>
{
public:
    class dummy_mutex
    {
    public:
        void lock() {}
        bool try_lock() { return true; }
        void unlock() {}
    };

    using Ptr = std::shared_ptr<HierarchicalDB>;
    //    using MutexType = boost::mutex;
    using MutexType = dummy_mutex;

    enum class CommitError : uint_fast16_t
    {
        AlreadyCommitted,
        UncommittedChildren,
        Conflict
    };

    static const char* CommitErrorToString(HierarchicalDB::CommitError err);

private:
    // map<DB_ID, map<key, vector<values>>>
    std::array<std::map<std::string, TransactionOperation>,
               static_cast<std::size_t>(IDB::Index::Index_Last)>
        data;

    // the parent can either be another transaction or can be the lowest database level
    std::shared_ptr<HierarchicalDB> parent;

    mutable MutexType mtx;

    boost::atomic_int_fast32_t openTransactions{0};
    bool                       committed{false};

    std::vector<HierarchicalDB::Ptr> committedTransactions;

    const std::size_t parentCommittedTxsOnStart;

    std::string dbName;

    std::pair<HierarchicalDB*, boost::optional<boost::unique_lock<HierarchicalDB::MutexType>>>
    getLockedInstanceToModify();

    [[nodiscard]] static std::size_t calculateParentsCommittedTxsOnStart(const HierarchicalDB* parentDB);

    [[nodiscard]] std::vector<TransactionOperation> getCommittedOpVec(
        int dbid, const std::string& key, bool lookIntoParent,
        std::size_t lastCommittedTransaction = std::numeric_limits<std::size_t>::max()) const;
    [[nodiscard]] boost::optional<TransactionOperation> getOpOfThisTx(int                dbid,
                                                                      const std::string& key) const;

    [[nodiscard]] static boost::optional<TransactionOperation>
    collapseOpsVec(std::vector<TransactionOperation>&& ops);

    [[nodiscard]] boost::optional<TransactionOperation> getCollapsedOps(int                dbid,
                                                                        const std::string& key) const;

    [[nodiscard]] std::map<std::string, TransactionOperation> getAllOpsOfThisTx(int dbid) const;

    [[nodiscard]] std::map<std::string, std::vector<TransactionOperation>> getCommittedAllOpsMap(
        int dbid, bool lookIntoParent,
        std::size_t lastCommittedTransaction = std::numeric_limits<std::size_t>::max()) const;
    [[nodiscard]] static std::map<std::string, TransactionOperation>
    collapseAllOpsMap(std::map<std::string, std::vector<TransactionOperation>>&& opsMap);
    [[nodiscard]] std::map<std::string, TransactionOperation> getCollapsedOpsForAll(int dbid) const;

    void assertNotCommitted() const;

public:
    HierarchicalDB(const std::string& name, const std::shared_ptr<HierarchicalDB>& parentDB = nullptr);
    ~HierarchicalDB();

    bool                         unique_set(int dbid, const std::string& key, const std::string& value);
    boost::optional<std::string> unique_get(int dbid, const std::string& key, std::size_t offset,
                                            const boost::optional<std::size_t>& size) const;
    boost::optional<TransactionOperation> getOp(int dbid, const std::string& key) const;

    [[nodiscard]] bool exists(int dbid, const std::string& key) const;
    bool               erase(int dbid, const std::string& key);

    bool multi_append(int dbid, const std::string& key, const std::string& value);
    [[nodiscard]] std::vector<std::string> multi_getAllWithKey(int dbid, const std::string& key) const;
    [[nodiscard]] std::map<std::string, std::vector<std::string>> multi_getAll(int dbid) const;

    void                      revert();
    Result<void, CommitError> commit();

    /**
     * @brief cancel, deems the transaction unusable, as if it's committed, but without committing the
     * data
     */
    void cancel();
    bool doesParentHaveUncommittedChildren() const;

    std::shared_ptr<HierarchicalDB> startDBTransaction(const std::string& txName);

    int_fast32_t getOpenTransactionsCount() const;

    std::map<std::string, TransactionOperation> getAllDataForDB(int dbid) const;

    static HierarchicalDB::Ptr Make(const std::string&                     name,
                                    const std::shared_ptr<HierarchicalDB>& parentDB = nullptr);
};

#endif // HIERARCHICALDB_H

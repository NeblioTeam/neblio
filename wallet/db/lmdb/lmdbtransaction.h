#ifndef LMDBTRANSACTION_H
#define LMDBTRANSACTION_H

#include <string>
#include <liblmdb/lmdb.h>
#include <atomic>

struct LMDBTransaction
{
    LMDBTransaction(const bool check = true);
    LMDBTransaction(const LMDBTransaction&) = delete;
    LMDBTransaction& operator=(const LMDBTransaction&) = delete;
    ~LMDBTransaction();

    LMDBTransaction(LMDBTransaction&& other);
    LMDBTransaction& operator=(LMDBTransaction&& other);

    void commit(std::string message = "");
    void commitIfValid(std::string message = "");

    // This should only be needed for batch transaction which must be ensured to
    // be aborted before mdb_env_close, not after. So we can't rely on
    // BlockchainLMDB destructor to call mdb_txn_safe destructor, as that's too late
    // to properly abort, since mdb_env_close would have been called earlier.
    void abort();
    void abortIfValid();
    void uncheck();

    operator MDB_txn*() { return m_txn; }

    operator MDB_txn**() { return &m_txn; }

    MDB_txn* rawPtr() const { return m_txn; }

    uint64_t num_active_tx() const;

    static void prevent_new_txns();
    static void wait_no_active_txns();
    static void allow_new_txns();

    MDB_txn*                     m_txn;
    bool                         m_batch_txn = false;
    bool                         m_check;
    static std::atomic<uint64_t> num_active_txns;

    // could use a mutex here, but this should be sufficient.
    static std::atomic_flag creation_gate;
};


#endif // LMDBTRANSACTION_H

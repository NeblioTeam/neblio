#include "lmdbtransaction.h"

#include "logging/logger.h"
#include <chrono>
#include <stdexcept>
#include <thread>

std::atomic<uint64_t> LMDBTransaction::num_active_txns{0};
std::atomic_flag      LMDBTransaction::creation_gate = ATOMIC_FLAG_INIT;

LMDBTransaction::LMDBTransaction(const bool check) : m_txn(nullptr), m_check(check)
{
    if (check) {
        while (creation_gate.test_and_set()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        num_active_txns++;
        creation_gate.clear();
    }
}

LMDBTransaction::~LMDBTransaction()
{
    if (!m_check)
        return;
    NLog.write(b_sev::trace, "mdb_txn_safe: destructor");
    if (m_txn != nullptr) {
        if (m_batch_txn) // this is a batch txn and should have been handled before this point for safety
        {
            NLog.write(b_sev::err,
                       "WARNING: mdb_txn_safe: m_txn is a batch txn and it's not NULL in destructor - "
                       "calling mdb_txn_abort()");
        } else {
            // Example of when this occurs: a lookup fails, so a read-only txn is
            // aborted through this destructor. However, successful read-only txns
            // ideally should have been committed when done and not end up here.
            //
            // NOTE: not sure if this is ever reached for a non-batch write
            // transaction, but it's probably not ideal if it did.
            NLog.write(b_sev::err,
                       "mdb_txn_safe: m_txn not NULL in destructor - calling mdb_txn_abort()");
        }
    }
    mdb_txn_abort(m_txn);

    num_active_txns--;
}

LMDBTransaction& LMDBTransaction::operator=(LMDBTransaction&& other)
{
    m_txn             = other.m_txn;
    m_batch_txn       = other.m_batch_txn;
    m_check           = other.m_check;
    other.m_check     = false;
    other.m_txn       = nullptr;
    other.m_batch_txn = false;
    return *this;
}

LMDBTransaction::LMDBTransaction(LMDBTransaction&& other)
    : m_txn(other.m_txn), m_batch_txn(other.m_batch_txn), m_check(other.m_check)
{
    other.m_check     = false;
    other.m_txn       = nullptr;
    other.m_batch_txn = false;
}

void LMDBTransaction::uncheck()
{
    num_active_txns--;
    m_check = false;
}

bool LMDBTransaction::isChecked() const { return m_check; }

int LMDBTransaction::commit(std::string message)
{
    if (message.size() == 0) {
        message = "Failed to commit a transaction to the db";
    }

    if (auto result = mdb_txn_commit(m_txn)) {
        m_txn = nullptr;
        NLog.write(b_sev::err, "{}: {}", message, std::to_string(result));
        return result;
    }
    m_txn = nullptr;
    return MDB_SUCCESS;
}

void LMDBTransaction::commitIfValid(std::string message)
{
    if (m_txn) {
        commit(message);
    }
}

void LMDBTransaction::abort()
{
    if (m_txn != nullptr) {
        mdb_txn_abort(m_txn);
        m_txn = nullptr;
    } else {
        NLog.write(b_sev::warn, "WARNING: mdb_txn_safe: abort() called, but m_txn is NULL");
    }
}

void LMDBTransaction::abortIfValid()
{
    if (m_txn) {
        abort();
    }
}

uint64_t LMDBTransaction::num_active_tx() { return num_active_txns; }

void LMDBTransaction::prevent_new_txns()
{
    while (creation_gate.test_and_set()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void LMDBTransaction::wait_no_active_txns()
{
    while (num_active_txns > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void LMDBTransaction::allow_new_txns() { creation_gate.clear(); }

void LMDBTransaction::increment_txns(int i)
{
    if (i) // why do an atomic operation if this is zero?
        num_active_txns += i;
}

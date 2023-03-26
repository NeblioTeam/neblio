#ifndef TRANSACTABLEDBENTRY_H
#define TRANSACTABLEDBENTRY_H

#include "hierarchicaldb.h"
#include <boost/container/flat_map.hpp>
#include <boost/variant.hpp>
#include <deque>
#include <vector>

class TransactableDBEntry
{
public:
    enum class EntryOperation
    {
        Write,
        Erase,
        Transaction
    };

    using SingleKeyValue    = std::tuple<int, std::string, std::string>;
    using EraseKeyValue     = std::pair<int, std::string>;
    using TransactionValues = std::shared_ptr<HierarchicalDB<hdb_dummy_mutex>>;

    using ValueType = boost::variant<SingleKeyValue, EraseKeyValue, TransactionValues>;

private:
    ValueType value;

    EntryOperation op;

public:
    TransactableDBEntry(const SingleKeyValue& kv);
    TransactableDBEntry(SingleKeyValue&& kv);
    TransactableDBEntry(const EraseKeyValue& k);
    TransactableDBEntry(EraseKeyValue&& k);
    TransactableDBEntry(TransactionValues&& hdb);
    TransactableDBEntry(const TransactionValues& hdb);

    static std::pair<int, std::string> GetKeyFromWrite(const SingleKeyValue& dbid_k_v);

    EntryOperation                           getOp() const;
    const ValueType&                         getValue() const;
    std::vector<std::pair<int, std::string>> getAllKeys() const;
};

#endif // TRANSACTABLEDBENTRY_H

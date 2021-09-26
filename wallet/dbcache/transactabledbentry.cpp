#include "transactabledbentry.h"
#include "db/idb.h"
#include "dbcache/hierarchicaldb.h"
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

TransactableDBEntry::TransactableDBEntry(const SingleKeyValue& kv) : value(kv), op(EntryOperation::Write)
{
}

TransactableDBEntry::TransactableDBEntry(SingleKeyValue&& kv)
    : value(std::move(kv)), op(EntryOperation::Write)
{
}

TransactableDBEntry::TransactableDBEntry(const EraseKeyValue& k) : value(k), op(EntryOperation::Erase) {}

TransactableDBEntry::TransactableDBEntry(EraseKeyValue&& k)
    : value(std::move(k)), op(EntryOperation::Erase)
{
}

TransactableDBEntry::TransactableDBEntry(TransactionValues&& hdb)
    : value(std::move(hdb)), op(EntryOperation::Transaction)
{
}

TransactableDBEntry::TransactableDBEntry(const TransactionValues& hdb)
    : value(hdb), op(EntryOperation::Transaction)
{
}

std::pair<int, std::string> TransactableDBEntry::GetKeyFromWrite(const SingleKeyValue& dbid_k_v)
{
    auto&& v = boost::get<SingleKeyValue>(dbid_k_v);
    return std::make_pair(std::get<0>(v), std::get<1>(v));
}

TransactableDBEntry::EntryOperation TransactableDBEntry::getOp() const { return op; }

const TransactableDBEntry::ValueType& TransactableDBEntry::getValue() const { return value; }

std::vector<std::pair<int, std::string>> TransactableDBEntry::getAllKeys() const
{
    switch (op) {
    case EntryOperation::Erase:
        return std::vector<std::pair<int, std::string>>(1, boost::get<EraseKeyValue>(value));
    case EntryOperation::Write:
        return std::vector<std::pair<int, std::string>>(
            1, GetKeyFromWrite(boost::get<SingleKeyValue>(value)));
    case EntryOperation::Transaction: {
        const TransactionValues& tx = boost::get<TransactionValues>(value);

        std::vector<std::pair<int, std::string>> result;
        for (int i = 0; i < static_cast<int>(IDB::Index::DB_MAIN_INDEX); i++) {
            const auto allTxData = tx->getAllDataForDB(0);

            // get all keys of the map
            for (auto&& d : allTxData) {
                result.push_back(std::make_pair(i, d.first));
            }
        }
        return result;
    }
    }

    throw std::runtime_error("Unexpected point reached + " + std::string(__PRETTY_FUNCTION__));
}

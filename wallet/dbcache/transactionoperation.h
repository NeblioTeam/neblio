#ifndef TRANSACTIONOPERATION_H
#define TRANSACTIONOPERATION_H

#include <string>
#include <vector>

namespace DBOperation {
enum class WriteOperationType
{
    Erase,
    Append,
    UniqueSet,
};

enum class ReadOperationType
{
    ValueRead,
    ValueWritten,
    NotFound,
    Erased,
};
} // namespace DBOperation

class TransactionOperation
{
    DBOperation::WriteOperationType op;
    std::vector<std::string>        relevantValues;

public:
    TransactionOperation(DBOperation::WriteOperationType operation, std::string value);
    TransactionOperation(DBOperation::WriteOperationType operation, std::vector<std::string> value);
    DBOperation::WriteOperationType getOpType() const { return op; }

    static void collapseOperations(TransactionOperation&       destination,
                                   const TransactionOperation& source);

    const std::vector<std::string>& getValues() const;

    std::vector<std::string>& getValues();
};

class DBCachedRead
{
    DBOperation::ReadOperationType op;
    std::vector<std::string>       relevantValues;

public:
    DBCachedRead(DBOperation::ReadOperationType operation, std::string value);
    DBCachedRead(DBOperation::ReadOperationType operation, std::vector<std::string> value);
    DBOperation::ReadOperationType getOpType() const { return op; }

    const std::vector<std::string>& getValues() const;

    std::vector<std::string>& getValues();
    void                      switchOpToWrite();
};

#endif // TRANSACTIONOPERATION_H

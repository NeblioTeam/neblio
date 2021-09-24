#include "transactionoperation.h"

using namespace DBOperation;

TransactionOperation::TransactionOperation(WriteOperationType operation, std::string value)
    : op(operation)
{
    if (op == WriteOperationType::Append) {
        relevantValues.push_back(value);
    }
    if (op == WriteOperationType::UniqueSet) {
        relevantValues.push_back(value);
    }
}

TransactionOperation::TransactionOperation(DBOperation::WriteOperationType operation,
                                           std::vector<std::string>        value)
    : op(operation)
{
    if (op == WriteOperationType::Append || op == WriteOperationType::UniqueSet) {
        relevantValues = std::move(value);
    }
}

void TransactionOperation::collapseOperations(TransactionOperation&       destination,
                                              const TransactionOperation& source)
{
    switch (source.getOpType()) {
    case WriteOperationType::Erase:
    case WriteOperationType::UniqueSet:
        destination = source;
        return;
    case WriteOperationType::Append:
        destination.getValues().insert(destination.getValues().end(), source.getValues().begin(),
                                       source.getValues().end());
        return;
    }
}

const std::vector<std::string>& TransactionOperation::getValues() const { return relevantValues; }

std::vector<std::string>& TransactionOperation::getValues() { return relevantValues; }

DBCachedRead::DBCachedRead(DBOperation::ReadOperationType operation, std::string value) : op(operation)
{
    if (op == ReadOperationType::ValueRead || op == ReadOperationType::ValueWritten) {
        relevantValues.push_back(value);
    }
}

DBCachedRead::DBCachedRead(DBOperation::ReadOperationType operation, std::vector<std::string> value)
    : op(operation)
{
    if (op == ReadOperationType::ValueRead || op == ReadOperationType::ValueWritten) {
        relevantValues = std::move(value);
    }
}

const std::vector<std::string>& DBCachedRead::getValues() const { return relevantValues; }

std::vector<std::string>& DBCachedRead::getValues() { return relevantValues; }

void DBCachedRead::switchOpToWrite() { op = DBOperation::ReadOperationType::ValueWritten; }

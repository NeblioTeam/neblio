#include "transactionoperation.h"

TransactionOperation::TransactionOperation(TransactionOperation::OperationType operation,
                                           std::string                         value)
    : op(operation)
{
    if (op == OperationType::Append) {
        relevantValues.push_back(value);
    }
    if (op == OperationType::UniqueSet) {
        relevantValues.push_back(value);
    }
}

void TransactionOperation::collapseOperations(TransactionOperation&       destination,
                                              const TransactionOperation& source)
{
    switch (source.getOpType()) {
    case OperationType::Erase:
    case OperationType::UniqueSet:
        destination = source;
        return;
    case OperationType::Append:
        destination.getValues().insert(destination.getValues().end(), source.getValues().begin(),
                                       source.getValues().end());
        return;
    }
}

const std::vector<std::string>& TransactionOperation::getValues() const { return relevantValues; }

std::vector<std::string>& TransactionOperation::getValues() { return relevantValues; }

#ifndef TRANSACTIONOPERATION_H
#define TRANSACTIONOPERATION_H

#include <string>
#include <vector>

class TransactionOperation
{
public:
    enum OperationType
    {
        Erase,
        Append,
        UniqueSet,
    };

private:
    OperationType            op;
    std::vector<std::string> relevantValues;

public:
    TransactionOperation(OperationType operation, std::string value);
    OperationType getOpType() const { return op; }

    static void collapseOperations(TransactionOperation&       destination,
                                   const TransactionOperation& source);

    const std::vector<std::string>& getValues() const;

    std::vector<std::string>& getValues();
};

#endif // TRANSACTIONOPERATION_H

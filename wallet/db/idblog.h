#ifndef ILOG_H
#define ILOG_H

#include "CustomTypes.h"

class ILog
{
public:
    virtual void logWrite(const StringViewT message) = 0;
};

#endif // ILOG_H

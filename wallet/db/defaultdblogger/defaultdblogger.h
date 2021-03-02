#ifndef DEFAULTLOGGER_H
#define DEFAULTLOGGER_H

#include "../idblog.h"

class DefaultDBLogger : public ILog
{
public:
    void logWrite(const StringViewT message) override;
};

#endif // DEFAULTLOGGER_H

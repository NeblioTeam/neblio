#ifndef DEFAULTLOGGER_H
#define DEFAULTLOGGER_H

#include "../ilog.h"

class DefaultLogger : public ILog
{
public:
    void logWrite(const StringViewT message) override;
};

#endif // DEFAULTLOGGER_H

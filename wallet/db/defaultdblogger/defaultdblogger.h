#ifndef DEFAULTDBLOGGER_H
#define DEFAULTDBLOGGER_H

#include "../idblog.h"

class DefaultDBLogger : public ILog
{
public:
    void logWrite(const StringViewT message) override;
};

#endif // DEFAULTDBLOGGER_H

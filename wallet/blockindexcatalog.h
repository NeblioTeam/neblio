#ifndef BLOCKINDEXCATALOG_H
#define BLOCKINDEXCATALOG_H

#include "blockindex.h"
#include "globals.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <map>
#include <uint256.h>

class CBlockIndex;

class BlockIndexCatalog
{
public:
    BlockIndexCatalog() {}
};

#endif // BLOCKINDEXCATALOG_H

#ifndef MEMPOOLMISC_H
#define MEMPOOLMISC_H

#include "consensus.h"
#include "result.h"
#include "validation.h"

class CTxMemPool;

/** (try to) add transaction to memory pool **/
Result<void, TxValidationState> AcceptToMemoryPool(CTxMemPool& pool, const CTransaction& tx,
                                                   const ITxDB* txdbPtr = nullptr);

bool EnableEnforceUniqueTokenSymbols(int blockHeight);

#endif // MEMPOOLMISC_H

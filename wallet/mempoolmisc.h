#ifndef MEMPOOLMISC_H
#define MEMPOOLMISC_H

#include "result.h"
#include "consensus.h"
#include "validation.h"

class CTxMemPool;

/** (try to) add transaction to memory pool **/
Result<void, TxValidationState> AcceptToMemoryPool(CTxMemPool& pool, const CTransaction& tx,
                                                   const ITxDB* txdbPtr = nullptr);

bool EnableEnforceUniqueTokenSymbols(const ITxDB& txdb);

#endif // MEMPOOLMISC_H

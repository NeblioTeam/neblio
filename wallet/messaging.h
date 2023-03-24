#ifndef MESSAGING_H
#define MESSAGING_H

#include "medianfilter.h"
#include "sync.h"
#include <string>
#include "uint256.h"
#include "serialize.h"

class CNode;
class ITxDB;
class CBlock;
class CInv;

enum class MessageProcessResult
{
    ReturnTrue,
    ReturnFalse,
    DoNothing,
};

// Amount of blocks that other nodes claim to have
extern CMedianFilter<int> cPeerBlockCounts;

void static PruneOrphanBlocks();
uint256 static GetOrphanRoot(const CBlock* pblock);

/** Minimum Peer Protocol Version */
int MinPeerVersion(const ITxDB& txdb);

bool IsInitialBlockDownload(const ITxDB& txdb);

bool ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv);

bool ProcessBlock(CNode* pfrom, CBlock* pblock);

bool AlreadyHave(const ITxDB& txdb, const CInv& inv);

#endif // MESSAGING_H

#include "diskblockindex.h"

#include "block.h"
#include "globals.h"
#include "util.h"

// uint256 CDiskBlockIndex::GetBlockHash() const { return blockHash; }

// std::string CDiskBlockIndex::ToString() const
//{
//    std::string str = "CDiskBlockIndex(";
//    str += CBlockIndex::ToString();
//    str += fmt::format("hashBlock={}, hashPrev={}, hashNext={})", GetBlockHash().ToString(),
//                       hashPrev.ToString(), hashNext.ToString());
//    return str;
//}

// void CDiskBlockIndex::print() const { NLog.write(b_sev::info, "{}", ToString()); }

#ifndef CBLOCKREJECT_H
#define CBLOCKREJECT_H

#include <string>
#include "uint256.h"

/** "reject" message codes */
static const unsigned char REJECT_MALFORMED   = 0x01;
static const unsigned char REJECT_INVALID     = 0x10;
static const unsigned char REJECT_OBSOLETE    = 0x11;
static const unsigned char REJECT_DUPLICATE   = 0x12;
static const unsigned char REJECT_NONSTANDARD = 0x40;
// static const unsigned char REJECT_DUST = 0x41; // part of BIP 61
static const unsigned char REJECT_INSUFFICIENTFEE = 0x42;
static const unsigned char REJECT_CHECKPOINT      = 0x43;


struct CBlockReject
{
    unsigned char chRejectCode;
    std::string   strRejectReason;
    uint256       hashBlock;
    CBlockReject(int rejectCode = 0, const std::string& rejectReason = "",
                 const uint256& blockHash = 0)
        : chRejectCode(static_cast<unsigned char>(rejectCode)), strRejectReason(rejectReason),
          hashBlock(blockHash)
    {
    }
};

#endif // CBLOCKREJECT_H

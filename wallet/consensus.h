#ifndef CONSENSUS_H
#define CONSENSUS_H

#include <string>

extern bool fEnforceCanonical;

class CTransaction;
class ITxDB;
class NTP1Transaction;
class uint256;
class CBlockIndex;

inline int64_t PastDrift(int64_t nTime) { return nTime - 10 * 60; }   // up to 10 minutes from the past
inline int64_t FutureDrift(int64_t nTime) { return nTime + 10 * 60; } // up to 10 minutes in the future

bool IsFinalTx(const CTransaction& tx, const ITxDB& txdb, int nBlockHeight = 0, int64_t nBlockTime = 0);

/** Check for standard transaction types
    @return True if all outputs (scriptPubKeys) use only standard transaction forms
*/
bool IsStandardTx(const ITxDB& txdb, const CTransaction& tx, std::string& reason);

/** blacklisted tokens are tokens that are to be ignored and not used for historical reasons */
bool IsIssuedTokenBlacklisted(std::pair<CTransaction, NTP1Transaction>& txPair);

void AssertNTP1TokenNameIsNotAlreadyInMainChain(const std::string& sym, const uint256& txHash,
                                                const ITxDB& txdb);
void AssertNTP1TokenNameIsNotAlreadyInMainChain(const NTP1Transaction& ntp1tx, const ITxDB& txdb);

/// Given a block index object, find the last block that matches the consensus type (PoW/PoS)
CBlockIndex GetLastBlockIndex(const CBlockIndex& indexIn, bool fProofOfStake, const ITxDB& txdb);

#endif // CONSENSUS_H

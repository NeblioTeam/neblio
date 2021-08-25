#ifndef WORK_H
#define WORK_H

#include "itxdb.h"
#include <cstdint>

int64_t GetProofOfWorkReward(const ITxDB& txdb, int64_t nFees);
int64_t GetProofOfStakeReward(const ITxDB& txdb, int64_t nCoinAge, int64_t nFees);

unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime);
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int nBlockTime);

#endif // WORK_H

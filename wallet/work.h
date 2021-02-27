#ifndef WORK_H
#define WORK_H

#include "itxdb.h"
#include <cstdint>

int64_t GetProofOfWorkReward(const ITxDB& txdb, int64_t nFees);
int64_t GetProofOfStakeReward(const ITxDB& txdb, int64_t nCoinAge, int64_t nFees);

#endif // WORK_H

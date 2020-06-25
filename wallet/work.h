#ifndef WORK_H
#define WORK_H

#include <cstdint>

int64_t            GetProofOfWorkReward(int64_t nFees);
int64_t            GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees);

#endif // WORK_H

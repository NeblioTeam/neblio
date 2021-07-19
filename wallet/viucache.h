#ifndef VIUCACHE_H
#define VIUCACHE_H

#include "forkspendsimulator.h"
#include "result.h"
#include <cstdint>
#include <map>
#include <random>

class VIUCache
{
    const std::size_t maxSize;

    std::map<uint256, const ForkSpendSimulatorCachedObj> tipBlockVsCachedObj;

    std::mt19937 randGen;

    void dropOneElement();

    static int GetRandomSeed();

public:
    VIUCache(const std::size_t maxSizeIn);

    void push(const ForkSpendSimulatorCachedObj& obj);

    bool push_with_probability(const ForkSpendSimulatorCachedObj& obj, unsigned probability_numerator,
                               unsigned probability_denominator);

    boost::optional<ForkSpendSimulatorCachedObj> get(uint256 tipBlockHash) const;
};

#endif // VIUCACHE_H

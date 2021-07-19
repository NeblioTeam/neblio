#include "viucache.h"

#include "logging/logger.h"

void VIUCache::dropOneElement()
{
    if (tipBlockVsCachedObj.empty()) {
        return;
    }
    auto it = tipBlockVsCachedObj.begin();
    std::advance(it, rand() % tipBlockVsCachedObj.size());
    tipBlockVsCachedObj.erase(it);
}

int VIUCache::GetRandomSeed()
{
    const int seed = std::random_device{}();

    NLog.write(b_sev::info, "Using seed for random VIU cache push probability: {}", seed);

    return seed;
}

VIUCache::VIUCache(const std::size_t maxSizeIn) : maxSize(maxSizeIn), randGen(GetRandomSeed()) {}

void VIUCache::push(const ForkSpendSimulatorCachedObj& obj)
{
    if (tipBlockVsCachedObj.size() + 1 > maxSize) {
        dropOneElement();
    }
    tipBlockVsCachedObj.emplace(std::make_pair(obj.lastProcessedTipBlockHash, obj));
}

bool VIUCache::push_with_probability(const ForkSpendSimulatorCachedObj& obj,
                                     unsigned probability_numerator, unsigned probability_denominator)
{
    assert(probability_denominator > 0);
    const unsigned outcome = static_cast<unsigned>(randGen()) % probability_denominator;
    if (outcome < probability_numerator) {
        push(obj);
        return true;
    }
    return false;
}

boost::optional<ForkSpendSimulatorCachedObj> VIUCache::get(uint256 tipBlockHash) const
{
    const auto it = tipBlockVsCachedObj.find(tipBlockHash);
    if (it == tipBlockVsCachedObj.cend()) {
        return boost::none;
    }
    return boost::make_optional(it->second);
}

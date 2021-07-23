#include "viucache.h"

#include "logging/logger.h"

void VIUCache::dropOneElement_unsafe()
{
    if (size_unsafe() == 0) {
        return;
    }
    auto it = tipBlockVsCachedObj.begin();
    std::advance(it, randGen() % tipBlockVsCachedObj.size());
    tipBlockVsCachedObj.erase(it);
}

void VIUCache::dropOneElement()
{
    std::lock_guard<decltype(mtx)> lg(mtx);
    dropOneElement_unsafe();
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
    std::lock_guard<decltype(mtx)> lg(mtx);

    if (size_unsafe() + 1 > maxSize) {
        dropOneElement_unsafe();
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

std::size_t VIUCache::size_unsafe() { return tipBlockVsCachedObj.size(); }

std::size_t VIUCache::size()
{
    std::lock_guard<decltype(mtx)> lg(mtx);
    return size_unsafe();
}

boost::optional<ForkSpendSimulatorCachedObj> VIUCache::get(uint256 tipBlockHash) const
{
    std::lock_guard<decltype(mtx)> lg(mtx);

    const auto it = tipBlockVsCachedObj.find(tipBlockHash);
    if (it == tipBlockVsCachedObj.cend()) {
        return boost::none;
    }
    return boost::make_optional(it->second);
}

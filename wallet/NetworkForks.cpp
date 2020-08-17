#include "NetworkForks.h"

#include <string>

NetworkForks::NetworkForks(const boost::container::flat_map<NetworkFork, int>& ForksToBlocks,
                           const boost::atomic<int>&                           BestHeightVar)
    : bestHeight_internal(BestHeightVar)
{
    if (ForksToBlocks.empty()) {
        throw std::logic_error("Forks list");
    }
    forksToBlockMap = ForksToBlocks;
}

bool NetworkForks::isForkActivated(NetworkFork fork) const
{
    auto it = forksToBlockMap.find(fork);
    if (it != forksToBlockMap.cend()) {
        return bestHeight_internal >= it->second;
    } else {
        throw std::runtime_error("Fork number " + std::to_string(fork) + " was not found");
    }
}

int NetworkForks::getFirstBlockOfFork(NetworkFork fork) const { return forksToBlockMap.at(fork); }

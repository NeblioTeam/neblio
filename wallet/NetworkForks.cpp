#include "NetworkForks.h"

NetworkForks::NetworkForks(const std::map<NetworkFork, int>& ForksToBlocks,
                           const boost::atomic<int>&         BestHeightVar)
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

NetworkFork NetworkForks::getForkAtBlockNumber(int blockNumber) const
{
    for (auto it = forksToBlockMap.crbegin(); it != forksToBlockMap.crend(); ++it) {
        const std::pair<NetworkFork, int>& e = *it;
        if (blockNumber >= e.second) {
            return e.first;
        }
    }

    return forksToBlockMap.crbegin()->first;
}

int NetworkForks::getFirstBlockOfFork(NetworkFork fork) const { return forksToBlockMap.at(fork); }

const NetworkForks& GetNetForks()
{
    if (fTestNet)
        return TestnetForks;
    else
        return MainnetForks;
}

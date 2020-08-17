#ifndef NETWORKFORKS_H
#define NETWORKFORKS_H

#include <boost/atomic.hpp>
#include <boost/container/flat_map.hpp>

enum NetworkFork : uint16_t
{
    NETFORK__1_FIRST_ONE = 0,
    NETFORK__2_CONFS_CHANGE,
    NETFORK__3_TACHYON,
    NETFORK__4_RETARGET_CORRECTION,
    NETFORK__5_COLD_STAKING
};

class NetworkForks
{
    boost::container::flat_map<NetworkFork, int> forksToBlockMap;
    boost::container::flat_map<int, NetworkFork> blockToForksMap;
    const boost::atomic<int>&                    bestHeight_internal;

public:
    NetworkForks(const boost::container::flat_map<NetworkFork, int>& ForksToBlocks,
                 const boost::atomic<int>&                           BestHeightVar);

    bool isForkActivated(NetworkFork fork) const;

    NetworkFork getForkAtBlockNumber(int blockNumber) const;

    int getFirstBlockOfFork(NetworkFork fork) const;
};

#endif // NETWORKFORKS_H

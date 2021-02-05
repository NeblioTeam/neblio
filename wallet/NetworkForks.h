#ifndef NETWORKFORKS_H
#define NETWORKFORKS_H

#include "itxdb.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/atomic.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/optional.hpp>
#include <string>
#include <vector>

class ITxDB;

enum NetworkFork : uint16_t
{
    NETFORK__1_FIRST_ONE = 0,
    NETFORK__2_CONFS_CHANGE,
    NETFORK__3_TACHYON,
    NETFORK__4_RETARGET_CORRECTION,
    NETFORK__5_COLD_STAKING
};

boost::container::flat_map<std::string, NetworkFork>
InvertNetworkForkEnumToName(const boost::container::flat_map<NetworkFork, std::string>& theMap);

boost::optional<NetworkFork> GetNetworkForkByName(const std::string& networkFork);

const boost::container::flat_map<NetworkFork, std::string> NetworkForkEnumToName{
    {NETFORK__1_FIRST_ONE, "first"},
    {NETFORK__2_CONFS_CHANGE, "confs_changed"},
    {NETFORK__3_TACHYON, "tachyon"},
    {NETFORK__4_RETARGET_CORRECTION, "retarget_correction"},
    {NETFORK__5_COLD_STAKING, "cold_staking"}};

const boost::container::flat_map<std::string, NetworkFork> NetworkForkNameToEnum =
    InvertNetworkForkEnumToName(NetworkForkEnumToName);

const boost::container::flat_map<NetworkFork, int>
ParseForkHeightsArgs(const std::vector<std::string>& args);

class NetworkForks
{
    boost::container::flat_map<NetworkFork, int> forksToBlockMap;
    boost::container::flat_map<int, NetworkFork> blockToForksMap;

public:
    NetworkForks(const boost::container::flat_map<NetworkFork, int>& ForksToBlocks);

    bool isForkActivated(NetworkFork fork, const ITxDB& txdb) const;

    bool isForkActivated(NetworkFork fork, int height) const;

    NetworkFork getForkAtBlockNumber(int blockNumber) const;

    int getFirstBlockOfFork(NetworkFork fork) const;
};

#endif // NETWORKFORKS_H

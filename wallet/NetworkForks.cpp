#include "NetworkForks.h"

#include <globals.h>
#include <string>

NetworkForks::NetworkForks(const boost::container::flat_map<NetworkFork, int>& ForksToBlocks,
                           const BestChainState&                               BestChainVar)
    : bestChain_internal(BestChainVar)
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
        return bestChain_internal.height() >= it->second;
    } else {
        throw std::runtime_error("Fork number " + std::to_string(fork) + " was not found");
    }
}

int NetworkForks::getFirstBlockOfFork(NetworkFork fork) const { return forksToBlockMap.at(fork); }

boost::container::flat_map<std::string, NetworkFork>
InvertNetworkForkEnumToName(const boost::container::flat_map<NetworkFork, std::string>& theMap)
{
    boost::container::flat_map<std::string, NetworkFork> result;
    for (const auto& p : theMap) {
        result[p.second] = p.first;
    }
    if (result.size() != theMap.size()) {
        throw std::runtime_error("Invalid NetworkForkEnumToName map. There seems to be duplicates");
    }
    return result;
}

boost::optional<NetworkFork> GetNetworkForkByName(const std::string& networkFork)
{
    auto it = NetworkForkNameToEnum.find(networkFork);
    if (it == NetworkForkNameToEnum.cend()) {
        return boost::none;
    }
    return it->second;
}

const boost::container::flat_map<NetworkFork, int>
ParseForkHeightsArgs(const std::vector<std::string>& args)
{
    boost::container::flat_map<NetworkFork, int> result;

    for (const std::string& arg : args) {
        // we expect every arg to be in the form: forkName,forkHeight. For example: tachyon,200

        // split for commas
        std::vector<std::string> splitRes;
        boost::algorithm::split(splitRes, arg, boost::is_any_of(","), boost::token_compress_on);
        if (splitRes.size() != 2) {
            throw std::invalid_argument("Splitting fork height for ',' must have 2 elements");
        }
        const std::string& forkName   = splitRes[0];
        const int          forkHeight = std::stoi(splitRes[1]);

        // get fork from its name that we got from the arg
        const boost::optional<NetworkFork> fork = GetNetworkForkByName(forkName);
        if (!fork) {
            throw std::invalid_argument("Invalid fork name in argument. Fork name '" + forkName +
                                        "' is not recognized.");
        }

        // ensure we have no duplicates and save the result
        if (result.find(*fork) != result.cend()) {
            throw std::invalid_argument("Duplicate fork name; the fork '" + forkName +
                                        "' appeared more than once.");
        }
        result[*fork] = forkHeight;
    }
    return result;
}

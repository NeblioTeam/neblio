#ifndef BOOTSTRAPTOOLS_H
#define BOOTSTRAPTOOLS_H

#include "uint256.h"
#include <atomic>
#include <boost/filesystem/path.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/thread/future.hpp>

using BlockIndexVertexType = uint256;
using BlockIndexGraphType =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, BlockIndexVertexType>;
using DescriptorType             = boost::graph_traits<BlockIndexGraphType>::vertex_descriptor;
using VerticesDescriptorsMapType = std::map<uint256, DescriptorType>;

enum GraphTraverseType
{
    BreadthFirst,
    DepthFirst
};

void ExportBootstrapBlockchain(const boost::filesystem::path& filename, std::atomic<bool>& stopped,
                               std::atomic<double>& progress, boost::promise<void>& result);
void ExportBootstrapBlockchainWithOrphans(const boost::filesystem::path& filename,
                                          std::atomic<bool>& stopped, std::atomic<double>& progress,
                                          boost::promise<void>& result, GraphTraverseType traverseType);

#endif // BOOTSTRAPTOOLS_H

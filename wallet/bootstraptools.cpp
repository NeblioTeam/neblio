#include "bootstraptools.h"

#include "block.h"
#include "blockindex.h"
#include "chainparams.h"
#include "txdb.h"
#include "util.h"
#include <boost/filesystem.hpp>

void ExportBootstrapBlockchain(const boost::filesystem::path& filename, std::atomic<bool>& stopped,
                               std::atomic<double>& progress, boost::promise<void>& result)
{
    RenameThread("Export-blockchain");
    try {
        progress.store(0, std::memory_order_relaxed);

        std::vector<CBlockIndex> chainBlocksIndices;

        const CTxDB txdb;

        if (stopped.load() || fShutdown) {
            throw std::runtime_error("Operation was stopped.");
        }

        boost::filesystem::ofstream outFile(filename, std::ios::binary);
        if (!outFile.good()) {
            throw std::runtime_error("Failed to open file for writing. Make sure you have sufficient "
                                     "permissions and diskspace.");
        }

        size_t threadsholdSize = 1 << 24; // 4 MB

        CDataStream  serializedBlocks(SER_DISK, CLIENT_VERSION);
        size_t       written = 0;
        const size_t total   = static_cast<size_t>(txdb.GetBestChainHeight());
        CBlockIndex  bi      = [&]() {
            boost::optional<CBlockIndex> obi = txdb.ReadBlockIndex(Params().GenesisBlockHash());
            if (!obi) {
                throw std::runtime_error(
                    "Couldn't read genesis block index. This indicates a fatal failure in the database");
            }
            return *obi;
        }();

        while (!bi.hashNext.IsNull()) {
            progress.store(static_cast<double>(written) / static_cast<double>(total == 0 ? 1 : total),
                           std::memory_order_relaxed);
            if (stopped.load() || fShutdown) {
                throw std::runtime_error("Operation was stopped.");
            }

            CBlock block;
            block.ReadFromDisk(&bi, txdb, true);

            // every block starts with pchMessageStart
            unsigned int nSize = block.GetSerializeSize(SER_DISK, CLIENT_VERSION);
            serializedBlocks << FLATDATA(Params().MessageStart()) << nSize;
            serializedBlocks << block;
            if (serializedBlocks.size() > threadsholdSize) {
                outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
                serializedBlocks.clear();
            }
            written++;

            // this works because the loop will break if there's no next
            boost::optional<CBlockIndex> obi = bi.getNext(txdb);
            if (obi) {
                bi = std::move(*obi);
            } else {
                break;
            }
        }
        if (serializedBlocks.size() > 0) {
            outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
            serializedBlocks.clear();
            if (!outFile.good()) {
                throw std::runtime_error("An error was raised while writing the file. Make sure you "
                                         "have sufficient permissions and diskspace.");
            }
        }
        progress.store(1, std::memory_order_seq_cst);
        result.set_value();
    } catch (std::exception& ex) {
        result.set_exception(boost::current_exception());
    } catch (...) {
        result.set_exception(boost::current_exception());
    }
}

class BlockIndexTraversorBase
{
    // shared_ptr is necessary because the visitor is passed by value during traversal
    std::shared_ptr<std::deque<uint256>> hashes;

public:
    BlockIndexTraversorBase() { hashes = std::make_shared<std::deque<uint256>>(); }
    template <typename Vertex, typename Graph>
    void discover_vertex(Vertex u, const Graph& g)
    {
        uint256 hash = boost::get(boost::vertex_bundle, g)[u];
        hashes->push_back(hash);
    }

    std::deque<uint256> getTraversedList() const { return *hashes; }
};

class DFSBlockIndexVisitor : public boost::default_dfs_visitor
{
    BlockIndexTraversorBase base;

public:
    template <typename Vertex, typename Graph>
    void discover_vertex(Vertex u, const Graph& g)
    {
        base.discover_vertex(u, g);
    }

    std::deque<uint256> getTraversedList() const { return base.getTraversedList(); }
};

class BFSBlockIndexVisitor : public boost::default_bfs_visitor
{
    BlockIndexTraversorBase base;

public:
    template <typename Vertex, typename Graph>
    void discover_vertex(Vertex u, const Graph& g)
    {
        base.discover_vertex(u, g);
    }

    std::deque<uint256> getTraversedList() { return base.getTraversedList(); }
};

std::pair<BlockIndexGraphType, VerticesDescriptorsMapType> GetBlockIndexAsGraph(const ITxDB& txdb)
{
    BlockIndexGraphType graph;

    // copy block index to avoid conflicts
    const boost::optional<std::map<uint256, CBlockIndex>> tempBlockIndex =
        txdb.ReadAllBlockIndexEntries();
    if (!tempBlockIndex) {
        throw std::runtime_error("Failed to retrieve the block index from the database");
    }

    VerticesDescriptorsMapType verticesDescriptors;

    // add all vertices, which are block hashes
    for (const auto& bi : *tempBlockIndex) {
        verticesDescriptors[bi.first] = boost::add_vertex(bi.first, graph);
    }

    // add edges, which are previous blocks connected to subsequent blocks
    for (const auto& bi : *tempBlockIndex) {
        if (bi.first != Params().GenesisBlockHash()) {
            boost::add_edge(verticesDescriptors.at(bi.second.hashPrev), verticesDescriptors.at(bi.first),
                            graph);
        }
    }
    return std::make_pair(graph, verticesDescriptors);
}

std::deque<uint256> TraverseBlockIndexGraph(const BlockIndexGraphType&        graph,
                                            const VerticesDescriptorsMapType& descriptors,
                                            GraphTraverseType                 traverseType)
{
    uint256 startBlockHash = Params().GenesisBlockHash();

    if (traverseType == GraphTraverseType::DepthFirst) {
        DFSBlockIndexVisitor vis;
        boost::depth_first_search(graph,
                                  boost::visitor(vis).root_vertex(descriptors.at(startBlockHash)));
        return vis.getTraversedList();
    } else if (traverseType == GraphTraverseType::BreadthFirst) {
        BFSBlockIndexVisitor vis;
        boost::breadth_first_search(graph, descriptors.at(startBlockHash), boost::visitor(vis));
        return vis.getTraversedList();
    } else {
        throw std::runtime_error("Unknown graph traversal type");
    }
}

void ExportBootstrapBlockchainWithOrphans(const boost::filesystem::path& filename,
                                          std::atomic<bool>& stopped, std::atomic<double>& progress,
                                          boost::promise<void>& result, GraphTraverseType traverseType)
{
    RenameThread("Export-blockchain");
    try {
        const CTxDB txdb;

        progress.store(0, std::memory_order_relaxed);

        BlockIndexGraphType        graph;
        VerticesDescriptorsMapType verticesDescriptors;
        std::tie(graph, verticesDescriptors) = GetBlockIndexAsGraph(txdb);

        std::deque<uint256> blocksHashes =
            TraverseBlockIndexGraph(graph, verticesDescriptors, traverseType);

        if (stopped.load() || fShutdown) {
            throw std::runtime_error("Operation was stopped.");
        }

        boost::filesystem::ofstream outFile(filename, std::ios::binary);
        if (!outFile.good()) {
            throw std::runtime_error("Failed to open file for writing. Make sure you have sufficient "
                                     "permissions and diskspace.");
        }

        size_t threadsholdSize = 1 << 24; // 4 MB

        CDataStream          serializedBlocks(SER_DISK, CLIENT_VERSION);
        size_t               written = 0;
        const std::uintmax_t total   = boost::num_vertices(graph);

        for (const uint256& h : blocksHashes) {
            progress.store(static_cast<double>(written) / static_cast<double>(total),
                           std::memory_order_relaxed);
            if (stopped.load() || fShutdown) {
                throw std::runtime_error("Operation was stopped.");
            }
            CBlock block;
            block.ReadFromDisk(h, txdb, true);

            // every block starts with pchMessageStart
            unsigned int nSize = block.GetSerializeSize(SER_DISK, CLIENT_VERSION);
            serializedBlocks << FLATDATA(Params().MessageStart()) << nSize;
            serializedBlocks << block;
            if (serializedBlocks.size() > threadsholdSize) {
                outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
                serializedBlocks.clear();
            }
            written++;
        }
        if (serializedBlocks.size() > 0) {
            outFile.write(serializedBlocks.str().c_str(), serializedBlocks.size());
            serializedBlocks.clear();
            if (!outFile.good()) {
                throw std::runtime_error("An error was raised while writing the file. Make sure you "
                                         "have sufficient permissions and diskspace.");
            }
        }
        progress.store(1, std::memory_order_seq_cst);
        result.set_value();
    } catch (std::exception& ex) {
        result.set_exception(boost::current_exception());
    } catch (...) {
        result.set_exception(boost::current_exception());
    }
}

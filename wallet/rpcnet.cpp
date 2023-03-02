// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinrpc.h"
#include "db.h"
#include "net.h"
#include "wallet.h"
#include "walletdb.h"

using namespace json_spirit;
using namespace std;

Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getconnectioncount\n"
                            "Returns the number of connections to other nodes.");

    LOCK(cs_vNodes);
    return (int)vNodes.size();
}

Value addnode(const Array& params, bool fHelp)
{
    std::string strCommand =
        params.size() > 1 && params[1].type() != null_type ? params[1].get_str() : "";
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw std::runtime_error(
            "addnode \"node\" \"add|remove|onetry\"\n"
            "\nAttempts to add or remove a node from the addnode list.\n"
            "Or try a connection to a node once.\n"
            "Nodes added using addnode (or -connect) are protected from DoS disconnection and are not "
            "required to be\n"
            "full nodes as other outbound peers are (though such peers will not be synced from).\n"
            "\nArguments:\n"
            "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"
            "2. \"command\"  (string, required) 'add' to add a node to the list, 'remove' to remove a "
            "node from the list, 'onetry' to try a connection to the node once\n"
            "\nExamples:\n"
            "addnode \"192.168.0.6:8333\" \"onetry\""
            "addnode \"192.168.0.6:8333\" \"onetry\"");

    std::string strNode = params[0].get_str();

    if (strCommand == "onetry") {
        CAddress addr;
        OpenNetworkConnection(addr, nullptr, strNode.c_str(), false);
        return json_spirit::Value();
    }

    if (strCommand == "add") {
        if (!AddNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
    } else if (strCommand == "remove") {
        if (!RemoveAddedNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    return json_spirit::Value();
}

Value disconnectnode(const Array& params, bool fHelp)
{
    bool validType = params.empty() || (params[0].type() != Value_type::str_type &&
                                        params[0].type() != Value_type::int_type);

    if ((fHelp || params.size() != 1) || validType) {
        throw std::runtime_error("disconnectnode \"node\" \n"
                                 "\nImmediately disconnects from the specified node.\n"

                                 "\nArguments:\n"
                                 "1. \"node\"     (string or int, required) The node identified by "
                                 "address or node id (see getpeerinfo for nodes)\n"

                                 "\nExamples:\n"
                                 "disconnectnode \"192.168.0.6:8333\""
                                 "disconnectnode 521");
    }

    CNode* pNode = nullptr;
    if (params[0].type() == Value_type::str_type) {
        pNode = FindNode(params[0].get_str());
    }
    if (params[0].type() == Value_type::int_type) {
        pNode = FindNode(params[0].get_int64());
    }
    if (pNode == nullptr)
        throw JSONRPCError(RPC_CLIENT_NODE_NOT_CONNECTED,
                           "Node not found in connected nodes. Use getpeerinfo function to get the list "
                           "of available nodes.");

    pNode->CloseSocketDisconnect();

    return json_spirit::Value();
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    for (CNode* pnode : vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}

Value getpeerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getpeerinfo\n"
                            "Returns data about each connected network node.");

    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    Array ret;

    BOOST_FOREACH (const CNodeStats& stats, vstats) {
        Object obj;

        obj.push_back(Pair("id", stats.nodeid));
        obj.push_back(Pair("addr", stats.addrName));
        obj.push_back(Pair("services", fmt::format("{:016x}", stats.nServices)));
        obj.push_back(Pair("lastsend", (int64_t)stats.nLastSend));
        obj.push_back(Pair("lastrecv", (int64_t)stats.nLastRecv));
        obj.push_back(Pair("conntime", (int64_t)stats.nTimeConnected));
        obj.push_back(Pair("version", stats.nVersion));
        obj.push_back(Pair("subver", stats.strSubVer));
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        obj.push_back(Pair("banscore", stats.nMisbehavior));

        ret.push_back(obj);
    }

    return ret;
}

Value setmocktime(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error("setmocktime timestamp\n"
                                 "\nSet the local time to given timestamp (-regtest only)\n"
                                 "\nArguments:\n"
                                 "1. timestamp  (integer, required) Unix seconds-since-epoch timestamp\n"
                                 "   Pass 0 to go back to using the system time.");

    if (!Params().MineBlocksOnDemand())
        throw std::runtime_error("setmocktime for regression testing (-regtest mode) only");

    // For now, don't change mocktime if we're in the middle of validation, as
    // this could have an effect on mempool time-based eviction, as well as
    // IsCurrentForFeeEstimation() and IsInitialBlockDownload().
    // TODO: figure out the right way to synchronize around mocktime, and
    // ensure all call sites of GetTime() are accessing this safely.
    LOCK(cs_main);

    SetMockTime(params[0].get_int64());

    return Value();
}

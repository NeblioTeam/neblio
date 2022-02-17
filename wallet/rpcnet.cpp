// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "alert.h"
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
                                 "1. \"nodeid\"     (int, required) The node identified by "
                                 "node id (see getpeerinfo for nodes)\n"

                                 "\nExamples:\n"
                                 "disconnectnode \"192.168.0.6:8333\""
                                 "disconnectnode 521");
    }

    CNode* pNode = nullptr;
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

    for (const CNodeStats& stats : vstats) {
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

// ppcoin: send alert.
// There is a known deadlock situation with ThreadMessageHandler
// ThreadMessageHandler: holds cs_vSend and acquiring cs_main in SendMessages()
// ThreadRPCServer: holds cs_main and acquiring cs_vSend in alert.RelayTo()/PushMessage()/BeginMessage()
Value sendalert(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 6)
        throw runtime_error(
            "sendalert <message> <privatekey> <minver> <maxver> <priority> <id> [cancelupto]\n"
            "<message> is the alert text message\n"
            "<privatekey> is hex string of alert master private key\n"
            "<minver> is the minimum applicable internal client version\n"
            "<maxver> is the maximum applicable internal client version\n"
            "<priority> is integer priority number\n"
            "<id> is the alert id\n"
            "[cancelupto] cancels all alert id's up to this number\n"
            "Returns true or false.");

    CAlert alert;
    CKey   key;

    alert.strStatusBar = params[0].get_str();
    alert.nMinVer      = params[2].get_int();
    alert.nMaxVer      = params[3].get_int();
    alert.nPriority    = params[4].get_int();
    alert.nID          = params[5].get_int();
    if (params.size() > 6)
        alert.nCancel = params[6].get_int();
    alert.nVersion    = PROTOCOL_VERSION;
    alert.nRelayUntil = GetAdjustedTime() + 365 * 24 * 60 * 60;
    alert.nExpiration = GetAdjustedTime() + 365 * 24 * 60 * 60;

    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedAlert)alert;
    alert.vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());

    vector<unsigned char> vchPrivKey = ParseHex(params[1].get_str());
    key.SetPrivKey(
        CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
        throw runtime_error("Unable to sign alert, check private key?\n");
    if (!alert.ProcessAlert())
        throw runtime_error("Failed to process alert.\n");
    // Relay alert
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            alert.RelayTo(pnode);
    }

    Object result;
    result.push_back(Pair("strStatusBar", alert.strStatusBar));
    result.push_back(Pair("nVersion", alert.nVersion));
    result.push_back(Pair("nMinVer", alert.nMinVer));
    result.push_back(Pair("nMaxVer", alert.nMaxVer));
    result.push_back(Pair("nPriority", alert.nPriority));
    result.push_back(Pair("nID", alert.nID));
    if (alert.nCancel > 0)
        result.push_back(Pair("nCancel", alert.nCancel));
    return result;
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

Value listbanned(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error("List all banned IPs.");

    const std::map<CNetAddr, int64_t> banMap = CNode::GetBanned();

    Array bannedAddresses;
    for (const auto& entry : banMap) {
        Object rec;
        rec.push_back(Pair("address", entry.first.ToString()));
        rec.push_back(Pair("banned_until", entry.second));
        // rec.push_back(Pair("ban_created", banEntry.nCreateTime));

        bannedAddresses.push_back(rec);
    }

    return bannedAddresses;
}

Value setban(const Array& params, bool fHelp)
{
    // clang-format off
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw std::runtime_error("setban <ip-address> <command> <bantime> <absolute>\n"
                                 "\n"
                                 "ip: The IP (see getpeerinfo for nodes IP) with an optional netmask\n"
                                 "command: 'add' to add an IP to the list, 'remove' to remove an IP/Subnet from the list\n"
                                 "bantime: (default: 0) time in seconds how long (or until when if [absolute] is set) the IP is banned (0 or empty means using the default time of 24h which can also be overwritten by the -bantime startup argument)"
                                 "absolute: (default: false) If set, the bantime must be an absolute timestamp expressed in UNIX epoch time\n"
                                 "Attempts to add or remove an IP/Subnet from the banned list.\n"
                                 "\n");
    // clang-format on

    std::string strCommand;
    if (!params[1].is_null())
        strCommand = params[1].get_str();
    if (strCommand != "add" && strCommand != "remove") {
        throw std::runtime_error("Command can be 'add' or 'remove'");
    }

    CNetAddr netAddr;

    std::vector<CNetAddr> allResolved;
    LookupHost(params[0].get_str().c_str(), allResolved, 1, false);
    if (allResolved.empty()) {
        throw std::runtime_error("Resolution of address " + params[0].get_str() +
                                 " failed - Invalid IP/Subnet");
    }
    netAddr = allResolved.front();

    if (!netAddr.IsValid())
        throw JSONRPCError(RPC_CLIENT_INVALID_IP_OR_SUBNET, "Error: Invalid IP/Subnet");

    if (strCommand == "add") {
        if (CNode::IsBanned(netAddr)) {
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: IP/Subnet already banned");
        }

        int64_t banTime = 0; // use standard bantime if not specified
        if (params.size() > 2 && !params[2].is_null())
            banTime = params[2].get_int64();

        bool absolute = params.size() > 3 && params[3].get_bool();

        CNode::Ban(netAddr, banTime, absolute);
        {
            while (true) {
                CNode* pNode = FindNode(netAddr);
                if (pNode == nullptr) {
                    break;
                }
                pNode->CloseSocketDisconnect();
            }
        }
    } else if (strCommand == "remove") {
        if (!CNode::Unban(netAddr)) {
            throw JSONRPCError(
                RPC_CLIENT_INVALID_IP_OR_SUBNET,
                "Error: Unban failed. Requested address/subnet was not previously manually banned.");
        }
    }

    return Value();
}

Value clearbanned(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error("Clear all banned IPs and persistent DOS counters.");

    CNode::ClearBanned();

    return Value();
}

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h"
#include "addrman.h"
#include "db.h"
#include "globals.h"
#include "init.h"
#include "main.h"
#include "ui_interface.h"

#include <chrono>
#include <thread>

#ifdef WIN32
#include <string.h>
#endif

#ifdef USE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

using namespace std;
using namespace boost;

static const int MAX_OUTBOUND_CONNECTIONS = 16;

void ThreadMessageHandler2(void* parg);
void ThreadSocketHandler2(void* parg);
void ThreadOpenConnections2(void* parg);
void ThreadOpenAddedConnections2(void* parg);
#ifdef USE_UPNP
void ThreadMapPort2(void* parg);
#endif
void ThreadDNSAddressSeed2(void* parg);

struct LocalServiceInfo
{
    int nScore;
    int nPort;
};

//
// Global state variables
//
bool                                   fDiscover = true;
bool                                   fUseUPnP  = false;
boost::atomic<uint64_t>                nLocalServices(NODE_NETWORK);
static CCriticalSection                cs_mapLocalHost;
static map<CNetAddr, LocalServiceInfo> mapLocalHost;
static bool                            vfReachable[NET_MAX] = {};
static bool                            vfLimited[NET_MAX]   = {};
static CNode*                          pnodeLocalHost       = nullptr;
LockedVar<CAddress>                    addrSeenByPeer(CAddress(CService("0.0.0.0", 0), nLocalServices));
boost::atomic<uint64_t>                nLocalHostNonce(0);
boost::array<boost::atomic_int, THREAD_MAX> vnThreadsRunning;
static std::vector<SOCKET>                  vhListenSocket;
LockedVar<CAddrMan>                         addrman;

vector<CNode*>                      vNodes;
CCriticalSection                    cs_vNodes;
LockedVar<std::vector<std::string>> vAddedNodes;
map<CInv, CDataStream>              mapRelay;
deque<pair<int64_t, CInv>>          vRelayExpiration;
CCriticalSection                    cs_mapRelay;
ThreadSafeHashMap<CInv, int64_t>    mapAlreadyAskedFor;

static deque<string> vOneShots;
CCriticalSection     cs_vOneShots;

static CSemaphore* semOutbound = nullptr;

void AddOneShot(string strDest)
{
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort() { return (unsigned short)(GetArg("-port", Params().GetDefaultPort())); }

void CNode::PushGetBlocks(CBlockIndex* pindexBegin, uint256 hashEnd)
{
    // Filter out duplicate requests
    if (pindexBegin == pindexLastGetBlocksBegin && hashEnd == hashLastGetBlocksEnd)
        return;
    pindexLastGetBlocksBegin = pindexBegin;
    hashLastGetBlocksEnd     = hashEnd;

    PushMessage("getblocks", CBlockLocator(pindexBegin), hashEnd);
}

// find 'best' local address for a particular peer
bool GetLocal(CService& addr, const CNetAddr* paddrPeer)
{
    if (fNoListen)
        return false;

    int nBestScore        = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (map<CNetAddr, LocalServiceInfo>::iterator it = mapLocalHost.begin();
             it != mapLocalHost.end(); it++) {
            int nScore        = (*it).second.nScore;
            int nReachability = (*it).first.GetReachabilityFrom(paddrPeer);
            if (nReachability > nBestReachability ||
                (nReachability == nBestReachability && nScore > nBestScore)) {
                addr              = CService((*it).first, (*it).second.nPort);
                nBestReachability = nReachability;
                nBestScore        = nScore;
            }
        }
    }
    return nBestScore >= 0;
}

// get best local address for a particular peer as a CAddress
CAddress GetLocalAddress(const CNetAddr* paddrPeer)
{
    CAddress ret(CService("0.0.0.0", 0), 0);
    CService addr;
    if (GetLocal(addr, paddrPeer)) {
        ret           = CAddress(addr);
        ret.nServices = nLocalServices;
        ret.nTime     = GetAdjustedTime();
    }
    return ret;
}

bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    while (true) {
        char c;
        int  nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0) {
            if (c == '\n')
                continue;
            if (c == '\r')
                return true;
            strLine += c;
            if (strLine.size() >= 9000)
                return true;
        } else if (nBytes <= 0) {
            if (fShutdown)
                return false;
            if (nBytes < 0) {
                int nErr = WSAGetLastError();
                if (nErr == WSAEMSGSIZE)
                    continue;
                if (nErr == WSAEWOULDBLOCK || nErr == WSAEINTR || nErr == WSAEINPROGRESS) {
                    MilliSleep(10);
                    continue;
                }
            }
            if (!strLine.empty())
                return true;
            if (nBytes == 0) {
                // socket closed
                NLog.write(b_sev::info, "socket closed");
                return false;
            } else {
                // socket error
                int nErr = WSAGetLastError();
                NLog.write(b_sev::err, "recv failed: {}", nErr);
                return false;
            }
        }
    }
}

// used when scores of local addresses may have changed
// pushes better local address to peers
void static AdvertizeLocal()
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (pnode->fSuccessfullyConnected) {
            CAddress addrLocal = GetLocalAddress(&pnode->addr);
            if (addrLocal.IsRoutable() && (CService)addrLocal != (CService)pnode->addrLocal) {
                pnode->PushAddress(addrLocal);
                pnode->addrLocal = addrLocal;
            }
        }
    }
}

void SetReachable(enum Network net, bool fFlag)
{
    LOCK(cs_mapLocalHost);
    vfReachable[net] = fFlag;
    if (net == NET_IPV6 && fFlag)
        vfReachable[NET_IPV4] = true;
}

// learn a new local address
bool AddLocal(const CService& addr, int nScore)
{
    if (!addr.IsRoutable())
        return false;

    if (!fDiscover && nScore < LOCAL_MANUAL)
        return false;

    if (IsLimited(addr))
        return false;

    NLog.write(b_sev::info, "AddLocal({},{})", addr.ToString(), nScore);

    {
        LOCK(cs_mapLocalHost);
        bool              fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo& info     = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort  = addr.GetPort();
        }
        SetReachable(addr.GetNetwork());
    }

    AdvertizeLocal();

    return true;
}

bool AddLocal(const CNetAddr& addr, int nScore)
{
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

/** Make a particular network entirely off-limits (no automatic connects to it) */
void SetLimited(enum Network net, bool fLimited)
{
    if (net == NET_UNROUTABLE)
        return;
    LOCK(cs_mapLocalHost);
    vfLimited[net] = fLimited;
}

bool IsLimited(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfLimited[net];
}

bool IsLimited(const CNetAddr& addr) { return IsLimited(addr.GetNetwork()); }

/** vote for a local address */
bool SeenLocal(const CService& addr)
{
    {
        LOCK(cs_mapLocalHost);
        if (mapLocalHost.count(addr) == 0)
            return false;
        mapLocalHost[addr].nScore++;
    }

    AdvertizeLocal();

    return true;
}

/** check whether a given address is potentially local */
bool IsLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

/** check whether a given address is in a network we can probably connect to */
bool IsReachable(const CNetAddr& addr)
{
    LOCK(cs_mapLocalHost);
    enum Network net = addr.GetNetwork();
    return vfReachable[net] && !vfLimited[net];
}

bool GetMyExternalIP2(const CService& addrConnect, const char* pszGet, const char* pszKeyword,
                      CNetAddr& ipRet)
{
    SOCKET hSocket;
    if (!ConnectSocket(addrConnect, hSocket)) {
        NLog.write(b_sev::err, "GetMyExternalIP() : connection to {} failed", addrConnect.ToString());
        return false;
    }

    send(hSocket, pszGet, strlen(pszGet), MSG_NOSIGNAL);

    string strLine;
    while (RecvLine(hSocket, strLine)) {
        if (strLine.empty()) // HTTP response is separated from headers by blank line
        {
            while (true) {
                if (!RecvLine(hSocket, strLine)) {
                    closesocket(hSocket);
                    return false;
                }
                if (pszKeyword == nullptr)
                    break;
                if (strLine.find(pszKeyword) != string::npos) {
                    strLine = strLine.substr(strLine.find(pszKeyword) + strlen(pszKeyword));
                    break;
                }
            }
            closesocket(hSocket);
            if (strLine.find("<") != string::npos)
                strLine = strLine.substr(0, strLine.find("<"));
            strLine = strLine.substr(strspn(strLine.c_str(), " \t\n\r"));
            while (strLine.size() > 0 && isspace(strLine[strLine.size() - 1]))
                strLine.resize(strLine.size() - 1);
            CService addr(strLine, 0, true);
            NLog.write(b_sev::info, "GetMyExternalIP() received [{}] {}", strLine, addr.ToString());
            if (!addr.IsValid() || !addr.IsRoutable())
                return false;
            ipRet.SetIP(addr);
            return true;
        }
    }
    closesocket(hSocket);
    NLog.write(b_sev::warn, "GetMyExternalIP() : connection closed");
    return false;
}

bool GetMyExternalIP(CNetAddr& ipRet)
{
    CService    addrConnect;
    const char* pszGet;
    const char* pszKeyword;

    for (int nLookup = 0; nLookup <= 1; nLookup++)
        for (int nHost = 1; nHost <= 2; nHost++) {
            // We should be phasing out our use of sites like these.  If we need
            // replacements, we should ask for volunteers to put this simple
            // php file on their web server that prints the client IP:
            //  <?php echo $_SERVER["REMOTE_ADDR"]; ?>
            if (nHost == 1) {
                addrConnect = CService("216.146.43.70", 80); // checkip.dyndns.org

                if (nLookup == 1) {
                    CService addrIP("checkip.dyndns.org", 80, true);
                    if (addrIP.IsValid())
                        addrConnect = addrIP;
                }

                pszGet = "GET / HTTP/1.1\r\n"
                         "Host: checkip.dyndns.org\r\n"
                         "User-Agent: neblio\r\n"
                         "Connection: close\r\n"
                         "\r\n";

                pszKeyword = "Address:";
            } else if (nHost == 2) {
                addrConnect = CService("74.208.43.192", 80); // www.showmyip.com

                if (nLookup == 1) {
                    CService addrIP("www.showmyip.com", 80, true);
                    if (addrIP.IsValid())
                        addrConnect = addrIP;
                }

                pszGet = "GET /simple/ HTTP/1.1\r\n"
                         "Host: www.showmyip.com\r\n"
                         "User-Agent: neblio\r\n"
                         "Connection: close\r\n"
                         "\r\n";

                pszKeyword = nullptr; // Returns just IP address
            }

            if (GetMyExternalIP2(addrConnect, pszGet, pszKeyword, ipRet))
                return true;
        }

    return false;
}

void ThreadGetMyExternalIP(void* /*parg*/)
{
    // Make this thread recognisable as the external IP detection thread
    RenameThread("neblio-ext-ip");

    CNetAddr addrLocalHost;
    if (GetMyExternalIP(addrLocalHost)) {
        NLog.write(b_sev::info, "GetMyExternalIP() returned {}", addrLocalHost.ToStringIP());
        AddLocal(addrLocalHost, LOCAL_HTTP);
    }
}

void AddressCurrentlyConnected(const CService& addr) { addrman.get().Connected(addr); }

CNode* FindNode(const CNetAddr& ip)
{
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if ((CNetAddr)pnode->addr == ip)
                return (pnode);
    }
    return nullptr;
}

CNode* FindNode(const std::string& addrName)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
        if (pnode->addrName.get() == addrName)
            return (pnode);
    return nullptr;
}

CNode* FindNode(const CService& addr)
{
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if ((CService)pnode->addr == addr)
                return (pnode);
    }
    return nullptr;
}

CNode* FindNode(const int64_t& nodeID)
{
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (pnode->nodeid == nodeID)
                return (pnode);
    }
    return nullptr;
}

CNode* ConnectNode(CAddress addrConnect, const char* pszDest)
{
    if (pszDest == nullptr) {
        if (IsLocal(addrConnect))
            return nullptr;

        // Look for an existing connection
        CNode* pnode = FindNode((CService)addrConnect);
        if (pnode) {
            pnode->AddRef();
            return pnode;
        }
    }

    /// debug print
    NLog.write(b_sev::debug, "trying connection {} lastseen={} hrs",
               pszDest ? pszDest : addrConnect.ToString(),
               pszDest ? 0 : (double)(GetAdjustedTime() - addrConnect.nTime) / 3600.0);

    // Connect
    SOCKET hSocket;
    if (pszDest ? ConnectSocketByName(addrConnect, hSocket, pszDest, Params().GetDefaultPort())
                : ConnectSocket(addrConnect, hSocket)) {
        addrman.get().Attempt(addrConnect);

        /// debug print
        NLog.write(b_sev::debug, "connected {}", pszDest ? pszDest : addrConnect.ToString());

        // Set to non-blocking
#ifdef WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR)
            NLog.write(b_sev::err, "ConnectSocket() : ioctlsocket non-blocking setting failed, error {}",
                       WSAGetLastError());
#else
        if (fcntl(hSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
            NLog.write(b_sev::err, "ConnectSocket() : fcntl non-blocking setting failed, error {}",
                       errno);
#endif

        // Add node
        CNode* pnode = new CNode(NodeIDCounter++, hSocket, addrConnect, pszDest ? pszDest : "", false);
        pnode->AddRef();

        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }

        pnode->nTimeConnected = GetTime();
        return pnode;
    } else {
        return nullptr;
    }
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;
    if (hSocket != INVALID_SOCKET) {
        NLog.write(b_sev::info, "disconnecting node {}", addrName.get());
        closesocket(hSocket);
        hSocket = INVALID_SOCKET;

        // in case this fails, we'll empty the recv buffer when the CNode is deleted
        TRY_LOCK(cs_vRecvMsg, lockRecv);
        if (lockRecv)
            vRecvMsg.clear();
    }
}

void CNode::PushVersion()
{
    /// when NTP implemented, change to just nTime = GetAdjustedTime()
    int64_t  nTime   = (fInbound ? GetAdjustedTime() : GetTime());
    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(CService("0.0.0.0", 0)));
    CAddress addrMe  = GetLocalAddress(&addr);
    RAND_bytes((unsigned char*)&nLocalHostNonce, sizeof(nLocalHostNonce));
    const int bestHeight = CTxDB().GetBestChainHeight().value_or(0);
    NLog.write(b_sev::info, "send version message: version {}, blocks={}, us={}, them={}, peer={}",
               PROTOCOL_VERSION, bestHeight, addrMe.ToString(), addrYou.ToString(), addr.ToString());

    PushMessage("version", PROTOCOL_VERSION, nLocalServices.load(), nTime, addrYou, addrMe,
                nLocalHostNonce.load(), strSubVersion, bestHeight);
}

std::map<CNetAddr, int64_t> CNode::setBanned;
CCriticalSection            CNode::cs_setBanned;

void CNode::ClearBanned() { setBanned.clear(); }

bool CNode::IsBanned(CNetAddr ip)
{
    bool fResult = false;
    {
        LOCK(cs_setBanned);
        std::map<CNetAddr, int64_t>::iterator i = setBanned.find(ip);
        if (i != setBanned.end()) {
            int64_t t = (*i).second;
            if (GetTime() < t)
                fResult = true;
        }
    }
    return fResult;
}

bool CNode::Misbehaving(int howmuch)
{
    if (addr.IsLocal()) {
        NLog.write(b_sev::warn, "Warning: Local node {} misbehaving (delta: {})!", addrName.get(),
                   howmuch);
        return false;
    }

    nMisbehavior += howmuch;
    if (nMisbehavior >= GetArg("-banscore", 100)) {
        int64_t banTime = GetTime() + GetArg("-bantime", 60 * 60 * 24); // Default 24-hour ban
        NLog.write(b_sev::warn, "Misbehaving: {} ({} -> {}) DISCONNECTING", addr.ToString(),
                   nMisbehavior - howmuch, nMisbehavior);
        {
            LOCK(cs_setBanned);
            if (setBanned[addr] < banTime)
                setBanned[addr] = banTime;
        }
        CloseSocketDisconnect();
        return true;
    } else
        NLog.write(b_sev::warn, "Misbehaving: {} ({} -> {})", addr.ToString(), nMisbehavior - howmuch,
                   nMisbehavior);
    return false;
}

#undef X
#define X(name) stats.name = name
void CNode::copyStats(CNodeStats& stats)
{
    X(nodeid);
    X(nServices);
    X(nLastSend);
    X(nLastRecv);
    X(nTimeConnected);
    stats.addrName = GetAddrName();
    X(nVersion);
    X(strSubVer);
    X(fInbound);
    X(nStartingHeight);
    X(nMisbehavior);
}
#undef X

// requires LOCK(cs_vRecvMsg)
bool CNode::ReceiveMsgBytes(const char* pch, unsigned int nBytes)
{
    while (nBytes > 0) {

        // get current incomplete message, or create a new one
        if (vRecvMsg.empty() || vRecvMsg.back().complete())
            vRecvMsg.push_back(CNetMessage(Params().MessageStart(), SER_NETWORK, nRecvVersion));

        CNetMessage& msg = vRecvMsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
            handled = msg.readHeader(pch, nBytes);
        else
            handled = msg.readData(pch, nBytes);

        if (handled < 0)
            return false;

        pch += handled;
        nBytes -= handled;
    }

    return true;
}

int CNetMessage::readHeader(const char* pch, unsigned int nBytes)
{
    // copy data to temporary parsing buffer
    unsigned int nRemaining = 24 - nHdrPos;
    unsigned int nCopy      = std::min(nRemaining, nBytes);

    memcpy(&hdrbuf[nHdrPos], pch, nCopy);
    nHdrPos += nCopy;

    // if header incomplete, exit
    if (nHdrPos < 24)
        return nCopy;

    // deserialize to CMessageHeader
    try {
        hdrbuf >> hdr;
    } catch (std::exception& e) {
        return -1;
    }

    // reject messages larger than MAX_SIZE
    if (hdr.nMessageSize > MAX_SIZE)
        return -1;

    // switch state to reading message data
    in_data = true;

    return nCopy;
}

int CNetMessage::readData(const char* pch, unsigned int nBytes)
{
    unsigned int nRemaining = hdr.nMessageSize - nDataPos;
    unsigned int nCopy      = std::min(nRemaining, nBytes);

    if (vRecv.size() < nDataPos + nCopy) {
        // Allocate up to 256 KiB ahead, but never more than the total message size.
        vRecv.resize(std::min(hdr.nMessageSize, nDataPos + nCopy + 256 * 1024));
    }

    memcpy(&vRecv[nDataPos], pch, nCopy);
    nDataPos += nCopy;

    return nCopy;
}

// requires LOCK(cs_vSend)
void SocketSendData(CNode* pnode)
{
    std::deque<CSerializeData>::iterator it = pnode->vSendMsg.begin();

    while (it != pnode->vSendMsg.end()) {
        const CSerializeData& data = *it;
        assert(data.size() > pnode->nSendOffset);
        int nBytes = send(pnode->hSocket, &data[pnode->nSendOffset], data.size() - pnode->nSendOffset,
                          MSG_NOSIGNAL | MSG_DONTWAIT);
        if (nBytes > 0) {
            pnode->nLastSend = GetTime();
            pnode->nSendOffset += nBytes;
            if (pnode->nSendOffset == data.size()) {
                pnode->nSendOffset = 0;
                pnode->nSendSize -= data.size();
                it++;
            } else {
                // could not send full message; stop sending more
                break;
            }
        } else {
            if (nBytes < 0) {
                // error
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR &&
                    nErr != WSAEINPROGRESS) {
                    NLog.write(b_sev::err, "socket send error {}", nErr);
                    pnode->CloseSocketDisconnect();
                }
            }
            // couldn't send anything at all
            break;
        }
    }

    if (it == pnode->vSendMsg.end()) {
        assert(pnode->nSendOffset == 0);
        assert(pnode->nSendSize == 0);
    }
    pnode->vSendMsg.erase(pnode->vSendMsg.begin(), it);
}

void ThreadSocketHandler(void* parg)
{
    // Make this thread recognisable as the networking thread
    RenameThread("neblio-net");

    try {
        vnThreadsRunning[THREAD_SOCKETHANDLER]++;
        ThreadSocketHandler2(parg);
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
    } catch (std::exception& e) {
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        PrintException(&e, "ThreadSocketHandler()");
    } catch (...) {
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        throw; // support pthread_cancel()
    }
    NLog.write(b_sev::info, "ThreadSocketHandler exited");
}

void ThreadSocketHandler2(void* /*parg*/)
{
    NLog.write(b_sev::info, "ThreadSocketHandler started");
    list<CNode*> vNodesDisconnected;
    unsigned int nPrevNodeCount = 0;

    while (true) {
        //
        // Disconnect nodes
        //
        {
            LOCK(cs_vNodes);
            // Disconnect unused nodes
            vector<CNode*> vNodesCopy = vNodes;
            BOOST_FOREACH (CNode* pnode, vNodesCopy) {
                if (pnode->fDisconnect || (pnode->GetRefCount() <= 0 && pnode->vRecvMsg.empty() &&
                                           pnode->nSendSize == 0 && pnode->ssSend.empty())) {
                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // release outbound grant (if any)
                    pnode->grantOutbound.Release();

                    // close socket and cleanup
                    pnode->CloseSocketDisconnect();

                    // hold in disconnected pool until all refs are released
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }

            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            BOOST_FOREACH (CNode* pnode, vNodesDisconnectedCopy) {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0) {
                    bool fDelete = false;
                    {
                        TRY_LOCK4(pnode->cs_vSend, pnode->cs_vRecvMsg, pnode->cs_mapRequests,
                                  pnode->cs_inventory, lock);
                        if (lock) {
                            fDelete = true;
                        }
                    }
                    if (fDelete) {
                        vNodesDisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
        }
        std::size_t vNodesSize = 0;
        {
            LOCK(cs_vNodes);
            vNodesSize = vNodes.size();
        }
        if (vNodesSize != nPrevNodeCount) {
            nPrevNodeCount = vNodesSize;
            uiInterface.NotifyNumConnectionsChanged(vNodesSize);
        }

        //
        // Find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vSend

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;
        bool   have_fds   = false;

        BOOST_FOREACH (SOCKET hListenSocket, vhListenSocket) {
            FD_SET(hListenSocket, &fdsetRecv);
            hSocketMax = max(hSocketMax, hListenSocket);
            have_fds   = true;
        }
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH (CNode* pnode, vNodes) {
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend) {
                        // do not read, if draining write queue
                        if (!pnode->vSendMsg.empty())
                            FD_SET(pnode->hSocket, &fdsetSend);
                        else
                            FD_SET(pnode->hSocket, &fdsetRecv);
                        FD_SET(pnode->hSocket, &fdsetError);
                        hSocketMax = max(hSocketMax, pnode->hSocket);
                        have_fds   = true;
                    }
                }
            }
        }

        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        int nSelect =
            select(have_fds ? hSocketMax + 1 : 0, &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        vnThreadsRunning[THREAD_SOCKETHANDLER]++;
        if (fShutdown)
            return;
        if (nSelect == SOCKET_ERROR) {
            if (have_fds) {
                int nErr = WSAGetLastError();
                NLog.write(b_sev::err, "socket select error {}", nErr);
                for (unsigned int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            MilliSleep(timeout.tv_usec / 1000);
        }

        //
        // Accept new connections
        //
        for (SOCKET hListenSocket : vhListenSocket)
            if (hListenSocket != INVALID_SOCKET && FD_ISSET(hListenSocket, &fdsetRecv)) {
                struct sockaddr_storage sockaddr;
                socklen_t               len = sizeof(sockaddr);
                SOCKET   hSocket            = accept(hListenSocket, (struct sockaddr*)&sockaddr, &len);
                CAddress addr;
                int      nInbound = 0;

                if (hSocket != INVALID_SOCKET)
                    if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
                        NLog.write(b_sev::warn, "Warning: Unknown socket family");

                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH (CNode* pnode, vNodes)
                        if (pnode->fInbound)
                            nInbound++;
                }

                if (hSocket == INVALID_SOCKET) {
                    int nErr = WSAGetLastError();
                    if (nErr != WSAEWOULDBLOCK)
                        NLog.write(b_sev::err, "socket error accept failed: {}", nErr);
                } else if (nInbound >= GetArg("-maxconnections", 125) - MAX_OUTBOUND_CONNECTIONS) {
                    closesocket(hSocket);
                } else if (CNode::IsBanned(addr)) {
                    NLog.write(b_sev::warn, "connection from {} dropped (banned)", addr.ToString());
                    closesocket(hSocket);
                } else {
                    NLog.write(b_sev::info, "accepted connection {}", addr.ToString());
                    CNode* pnode = new CNode(NodeIDCounter++, hSocket, addr, "", true);
                    pnode->AddRef();
                    {
                        LOCK(cs_vNodes);
                        vNodes.push_back(pnode);
                    }
                }
            }

        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH (CNode* pnode, vNodesCopy)
                pnode->AddRef();
        }
        BOOST_FOREACH (CNode* pnode, vNodesCopy) {
            if (fShutdown)
                return;

            //
            // Receive
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetRecv) || FD_ISSET(pnode->hSocket, &fdsetError)) {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv) {
                    if (pnode->GetTotalRecvSize() > ReceiveFloodSize()) {
                        if (!pnode->fDisconnect)
                            NLog.write(b_sev::warn, "socket recv flood control disconnect ({} bytes)",
                                       pnode->GetTotalRecvSize());
                        pnode->CloseSocketDisconnect();
                    } else {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int  nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        if (nBytes > 0) {
                            if (!pnode->ReceiveMsgBytes(pchBuf, nBytes))
                                pnode->CloseSocketDisconnect();
                            pnode->nLastRecv = GetTime();
                        } else if (nBytes == 0) {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                                NLog.write(b_sev::info, "socket closed");
                            pnode->CloseSocketDisconnect();
                        } else if (nBytes < 0) {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR &&
                                nErr != WSAEINPROGRESS) {
                                if (!pnode->fDisconnect)
                                    NLog.write(b_sev::err, "socket recv error {}", nErr);
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetSend)) {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SocketSendData(pnode);
            }

            //
            // Inactivity checking
            //
            {
                LOCK(pnode->cs_vSend);
                if (pnode->vSendMsg.empty())
                    pnode->nLastSendEmpty = GetTime();
            }
            if (GetTime() - pnode->nTimeConnected > 60) {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0) {
                    NLog.write(b_sev::warn, "socket no message in first 60 seconds, {} {}",
                               pnode->nLastRecv != 0, pnode->nLastSend != 0);
                    pnode->fDisconnect = true;
                } else if (GetTime() - pnode->nLastSend > 90 * 60 &&
                           GetTime() - pnode->nLastSendEmpty > 90 * 60) {
                    NLog.write(b_sev::warn, "socket not sending");
                    pnode->fDisconnect = true;
                } else if (GetTime() - pnode->nLastRecv > 90 * 60) {
                    NLog.write(b_sev::err, "socket inactivity timeout");
                    pnode->fDisconnect = true;
                }
            }
        }
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodesCopy)
                pnode->Release();
        }

        MilliSleep(10);
    }
}

#ifdef USE_UPNP
void ThreadMapPort(void* parg)
{
    // Make this thread recognisable as the UPnP thread
    RenameThread("neblio-UPnP");

    try {
        vnThreadsRunning[THREAD_UPNP]++;
        ThreadMapPort2(parg);
        vnThreadsRunning[THREAD_UPNP]--;
    } catch (std::exception& e) {
        vnThreadsRunning[THREAD_UPNP]--;
        PrintException(&e, "ThreadMapPort()");
    } catch (...) {
        vnThreadsRunning[THREAD_UPNP]--;
        PrintException(nullptr, "ThreadMapPort()");
    }
    NLog.write(b_sev::info, "ThreadMapPort exited");
}

void ThreadMapPort2(void* /*parg*/)
{
    NLog.write(b_sev::info, "ThreadMapPort started");

    std::string     port          = fmt::format("{}", GetListenPort());
    const char*     multicastif   = 0;
    const char*     minissdpdpath = 0;
    struct UPNPDev* devlist       = 0;
    char            lanaddr[64];

#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);
#elif MINIUPNPC_API_VERSION < 14
    /* miniupnpc 1.6 */
    int error = 0;
    devlist   = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#else
    /* miniupnpc > 1.6 */
    int error = 0;
    devlist   = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, 2, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int             r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1) {
        if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if (r != UPNPCOMMAND_SUCCESS)
                NLog.write(b_sev::info, "UPnP: GetExternalIPAddress() returned {}", r);
            else {
                if (externalIPAddress[0]) {
                    NLog.write(b_sev::info, "UPnP: ExternalIPAddress = {}", externalIPAddress);
                    AddLocal(CNetAddr(externalIPAddress), LOCAL_UPNP);
                } else
                    NLog.write(b_sev::err, "UPnP: GetExternalIPAddress failed.");
            }
        }

        string strDesc = "neblio " + FormatFullVersion();
#ifndef UPNPDISCOVER_SUCCESS
        /* miniupnpc 1.5 */
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(), port.c_str(),
                                lanaddr, strDesc.c_str(), "TCP", 0);
#else
        /* miniupnpc 1.6 */
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(), port.c_str(),
                                lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

        if (r != UPNPCOMMAND_SUCCESS)
            NLog.write(b_sev::err, "AddPortMapping({}, {}, {}) failed with code {} ({})", port, port,
                       lanaddr, r, strupnperror(r));
        else
            NLog.write(b_sev::info, "UPnP Port Mapping successful.");
        int i = 1;
        while (true) {
            if (fShutdown || !fUseUPnP) {
                r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP",
                                           0);
                NLog.write(b_sev::info, "UPNP_DeletePortMapping() returned : {}", r);
                freeUPNPDevlist(devlist);
                devlist = 0;
                FreeUPNPUrls(&urls);
                return;
            }
            if (i % 600 == 0) // Refresh every 20 minutes
            {
#ifndef UPNPDISCOVER_SUCCESS
                /* miniupnpc 1.5 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(),
                                        port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
                /* miniupnpc 1.6 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(),
                                        port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

                if (r != UPNPCOMMAND_SUCCESS)
                    NLog.write(b_sev::err, "AddPortMapping({}, {}, {}) failed with code {} ({})", port,
                               port, lanaddr, r, strupnperror(r));
                else
                    NLog.write(b_sev::info, "UPnP Port Mapping successful.");
                ;
            }
            MilliSleep(2000);
            i++;
        }
    } else {
        NLog.write(b_sev::err, "No valid UPnP IGDs found");
        freeUPNPDevlist(devlist);
        devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&urls);
        while (true) {
            if (fShutdown || !fUseUPnP)
                return;
            MilliSleep(2000);
        }
    }
}

void MapPort()
{
    if (fUseUPnP && vnThreadsRunning[THREAD_UPNP] < 1) {
        if (!NewThread(ThreadMapPort, nullptr))
            NLog.write(b_sev::err, "Error: ThreadMapPort(ThreadMapPort) failed");
    }
}
#else
void MapPort()
{
    // Intentionally left blank.
}
#endif

void ThreadDNSAddressSeed(void* parg)
{
    // Make this thread recognisable as the DNS seeding thread
    RenameThread("neblio-dnsseed");

    try {
        vnThreadsRunning[THREAD_DNSSEED]++;
        ThreadDNSAddressSeed2(parg);
        vnThreadsRunning[THREAD_DNSSEED]--;
    } catch (std::exception& e) {
        vnThreadsRunning[THREAD_DNSSEED]--;
        PrintException(&e, "ThreadDNSAddressSeed()");
    } catch (...) {
        vnThreadsRunning[THREAD_DNSSEED]--;
        throw; // support pthread_cancel()
    }
    NLog.write(b_sev::info, "ThreadDNSAddressSeed exited");
}

void ThreadDNSAddressSeed2(void* /*parg*/)
{
    NLog.write(b_sev::info, "ThreadDNSAddressSeed started");
    int found = 0;

    //    if (IsMainnet()) {
    if (true) {
        NLog.write(b_sev::info, "Loading addresses from DNS seeds (could take a while)");

        const std::vector<std::string> dnsSeeds = Params().DNSSeeds();
        for (unsigned int seed_idx = 0; seed_idx < dnsSeeds.size(); seed_idx++) {
            if (HaveNameProxy()) {
                AddOneShot(dnsSeeds[seed_idx]);
            } else {
                vector<CNetAddr> vaddr;
                vector<CAddress> vAdd;
                if (LookupHost(dnsSeeds[seed_idx].c_str(), vaddr)) {
                    for (CNetAddr& ip : vaddr) {
                        int      nOneDay = 24 * 3600;
                        CAddress addr    = CAddress(CService(ip, Params().GetDefaultPort()));
                        // use a random age between 3 and 7 days old
                        addr.nTime = GetTime() - 3 * nOneDay - GetRand(4 * nOneDay);
                        vAdd.push_back(addr);
                        found++;
                    }
                }
                addrman.get().Add(vAdd, CNetAddr(dnsSeeds[seed_idx], true));
            }
        }
    }

    NLog.write(b_sev::info, "{} addresses found from DNS seeds", found);
}

unsigned int pnSeed[] = {};

void DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

    CAddrDB adb;
    adb.Write(addrman.get());

    NLog.write(b_sev::info, "Flushed {} addresses to peers.dat  {} ms", addrman.get().size(),
               GetTimeMillis() - nStart);
}

void ThreadDumpAddress2(void* /*parg*/)
{
    vnThreadsRunning[THREAD_DUMPADDRESS]++;
    while (!fShutdown) {
        DumpAddresses();
        vnThreadsRunning[THREAD_DUMPADDRESS]--;
        MilliSleep(600000);
        vnThreadsRunning[THREAD_DUMPADDRESS]++;
    }
    vnThreadsRunning[THREAD_DUMPADDRESS]--;
}

void ThreadDumpAddress(void* parg)
{
    // Make this thread recognisable as the address dumping thread
    RenameThread("neblio-adrdump");

    try {
        ThreadDumpAddress2(parg);
    } catch (std::exception& e) {
        PrintException(&e, "ThreadDumpAddress()");
    }
    NLog.write(b_sev::info, "ThreadDumpAddress exited");
}

void ThreadOpenConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("neblio-opencon");

    try {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        ThreadOpenConnections2(parg);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
    } catch (std::exception& e) {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        PrintException(&e, "ThreadOpenConnections()");
    } catch (...) {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        PrintException(nullptr, "ThreadOpenConnections()");
    }
    NLog.write(b_sev::info, "ThreadOpenConnections exited");
}

void static ProcessOneShot()
{
    string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty())
            return;
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress        addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        if (!OpenNetworkConnection(addr, &grant, strDest.c_str(), true))
            AddOneShot(strDest);
    }
}

void static ThreadStakeMiner(void* parg)
{
    NLog.write(b_sev::info, "ThreadStakeMiner started");
    CWallet* pwallet = (CWallet*)parg;
    try {
        vnThreadsRunning[THREAD_STAKE_MINER]++;
        StakeMiner(pwallet);
        vnThreadsRunning[THREAD_STAKE_MINER]--;
    } catch (std::exception& e) {
        vnThreadsRunning[THREAD_STAKE_MINER]--;
        PrintException(&e, "ThreadStakeMiner()");
    } catch (...) {
        vnThreadsRunning[THREAD_STAKE_MINER]--;
        PrintException(nullptr, "ThreadStakeMiner()");
    }
    std::string threadsRemainingStr = "{";
    {
        for (unsigned i = 0; i < vnThreadsRunning.size(); i++) {
            threadsRemainingStr += std::to_string(vnThreadsRunning[i].load());
            if (i + 1 < vnThreadsRunning.size()) {
                threadsRemainingStr += ", ";
            }
        }
        threadsRemainingStr += "}";
    }
    NLog.write(b_sev::info, "ThreadStakeMiner exiting, {} threads remaining", threadsRemainingStr);
}

void ThreadOpenConnections2(void* /*parg*/)
{
    NLog.write(b_sev::info, "ThreadOpenConnections started");

    // Connect to specific addresses
    std::vector<std::string> connectVals =
        mapMultiArgs.get("-connect").value_or(std::vector<std::string>());
    if (connectVals.size() > 0) {
        for (int64_t nLoop = 0;; nLoop++) {
            ProcessOneShot();
            std::vector<std::string> connectVals =
                mapMultiArgs.get("-connect").value_or(std::vector<std::string>());
            BOOST_FOREACH (string strAddr, connectVals) {
                CAddress addr;
                OpenNetworkConnection(addr, nullptr, strAddr.c_str());
                for (int i = 0; i < 10 && i < nLoop; i++) {
                    MilliSleep(500);
                    if (fShutdown)
                        return;
                }
            }
            MilliSleep(500);
        }
    }

    // Initiate network connections
    int64_t nStart = GetTime();
    while (!fShutdown) {
        ProcessOneShot();

        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        MilliSleep(500);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        if (fShutdown)
            return;

        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        CSemaphoreGrant grant(*semOutbound);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        if (fShutdown)
            return;

        {
            auto addrman_lock = addrman.get_lock();

            if (addrman.get_unsafe().size() == 0 && (GetTime() - nStart > 60) /*&& IsMainnet()*/) {
                std::vector<CAddress> vAdd;
                for (unsigned int i = 0; i < ARRAYLEN(pnSeed); i++) {
                    // It'll only connect to one or two seed nodes because once it connects,
                    // it'll get a pile of addresses with newer timestamps.
                    // Seed nodes are given a random 'last seen time' of between one and two
                    // weeks ago.
                    const int64_t  nOneWeek = 7 * 24 * 60 * 60;
                    struct in_addr ip;
                    memcpy(&ip, &pnSeed[i], sizeof(ip));
                    CAddress addr(CService(ip, Params().GetDefaultPort()));
                    addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
                    vAdd.push_back(addr);
                }
                addrman.get_unsafe().Add(vAdd, CNetAddr("127.0.0.1"));
            }
        }

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        // Only connect out to one peer per network group (/16 for IPv4).
        // Do this here so we don't have to critsect vNodes inside mapAddresses critsect.
        int                        nOutbound = 0;
        set<vector<unsigned char>> setConnected;
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH (CNode* pnode, vNodes) {
                if (!pnode->fInbound) {
                    setConnected.insert(pnode->addr.GetGroup());
                    nOutbound++;
                }
            }
        }

        int64_t nANow = GetAdjustedTime();

        int nTries = 0;
        while (true) {
            // use an nUnkBias between 10 (no outgoing connections) and 90 (8 outgoing connections)
            CAddress addr = addrman.get().Select(10 + min(nOutbound, 8) * 10);

            // if we selected an invalid address, restart
            if (!addr.IsValid() || setConnected.count(addr.GetGroup()) || IsLocal(addr))
                break;

            // If we didn't find an appropriate destination after trying 100 addresses fetched from
            // addrman, stop this loop, and let the outer loop run again (which sleeps, adds seed nodes,
            // recalculates already-connected network ranges, ...) before trying new addrman addresses.
            nTries++;
            if (nTries > 100)
                break;

            if (IsLimited(addr))
                continue;

            // only consider very recently tried nodes after 30 failed attempts
            if (nANow - addr.nLastTry < 600 && nTries < 30)
                continue;

            // do not allow non-default ports, unless after 50 invalid addresses selected already
            if (addr.GetPort() != Params().GetDefaultPort() && nTries < 50)
                continue;

            addrConnect = addr;
            break;
        }

        if (addrConnect.IsValid())
            OpenNetworkConnection(addrConnect, &grant);
    }
}

void ThreadOpenAddedConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("neblio-opencon");

    try {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        ThreadOpenAddedConnections2(parg);
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
    } catch (std::exception& e) {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        PrintException(&e, "ThreadOpenAddedConnections()");
    } catch (...) {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        PrintException(nullptr, "ThreadOpenAddedConnections()");
    }
    NLog.write(b_sev::info, "ThreadOpenAddedConnections exited");
}

void SleepOrWaitForAddedNodesToChange(std::size_t prevNodesCount)
{
    // we sleep for two minutes
    static constexpr const std::size_t totalSleepSeconds = 120;
    static constexpr const std::size_t timeStep          = 2;
    for (std::size_t i = 0; i < totalSleepSeconds / timeStep && !fShutdown; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(timeStep)); // Retry every 2 minutes
        if (!fShutdown && vAddedNodes.get().size() != prevNodesCount) {
            break;
        }
    }
}

void ThreadOpenAddedConnections2(void* /*parg*/)
{
    NLog.write(b_sev::info, "ThreadOpenAddedConnections started");

    // we make this a set because we don't want additional nodes to keep filling the memory (using the
    // function AddNode())
    std::set<std::string> addnodeVals;

    // we add the nodes from the command line arguments
    {
        std::vector<std::string> addnodeTempVec =
            mapMultiArgs.get("-addnode").value_or(std::vector<std::string>());
    }

    // we add the nodes that are preset in the chain params
    const std::vector<std::string>& moreNodes = Params().AdditionalNodes();
    addnodeVals.insert(moreNodes.cbegin(), moreNodes.cend());

    const std::set<std::string> OriginalAddedNodes = addnodeVals;

    if (HaveNameProxy()) {
        while (!fShutdown) {
            std::size_t prevNodesCount;
            {
                // since nodes can be removed from vAddedNodes, we have to restore the original state of
                // addnodeVals every time
                addnodeVals = OriginalAddedNodes;

                // we add the nodes that are added with AddNode()
                auto lock = vAddedNodes.get_lock();

                const std::vector<std::string>& nodes = vAddedNodes.get_unsafe();
                addnodeVals.insert(nodes.cbegin(), nodes.cend());

                prevNodesCount = nodes.size();
            }
            for (const string& strAddNode : addnodeVals) {
                CAddress        addr;
                CSemaphoreGrant grant(*semOutbound);
                OpenNetworkConnection(addr, &grant, strAddNode.c_str());
                MilliSleep(500);
            }
            vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
            SleepOrWaitForAddedNodesToChange(prevNodesCount);
            vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        }
        return;
    }

    vector<vector<CService>> vservAddressesToAdd(0);
    for (const string& strAddNode : addnodeVals) {
        vector<CService> vservNode(0);
        if (Lookup(strAddNode.c_str(), vservNode, Params().GetDefaultPort(), fNameLookup, 0)) {
            vservAddressesToAdd.push_back(vservNode);
        }
    }
    while (true) {
        vector<vector<CService>> vservConnectAddresses = vservAddressesToAdd;

        std::size_t prevNodesCount;

        // Attempt to connect to each IP for each addnode entry until at least one is successful per
        // addnode entry (keeping in mind that addnode entries can have many IPs if fNameLookup)
        {
            {
                // since nodes can be removed from vAddedNodes, we have to restore the original state of
                // addnodeVals every time
                addnodeVals = OriginalAddedNodes;

                // we add the nodes that are added with AddNode()
                auto lock = vAddedNodes.get_lock();

                const std::vector<std::string>& nodes = vAddedNodes.get_unsafe();
                addnodeVals.insert(nodes.cbegin(), nodes.cend());

                prevNodesCount = nodes.size();
            }

            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
                for (vector<vector<CService>>::iterator it = vservConnectAddresses.begin();
                     it != vservConnectAddresses.end(); it++)
                    for (CService& addrNode : *(it))
                        if (pnode->addr == addrNode) {
                            it = vservConnectAddresses.erase(it);
                            it--;
                            break;
                        }
        }
        for (vector<CService>& vserv : vservConnectAddresses) {
            CSemaphoreGrant grant(*semOutbound);
            OpenNetworkConnection(CAddress(*(vserv.begin())), &grant);
            MilliSleep(500);
            if (fShutdown)
                return;
        }
        if (fShutdown)
            return;
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        SleepOrWaitForAddedNodesToChange(prevNodesCount);
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        if (fShutdown)
            return;
    }
}

// if successful, this moves the passed grant to the constructed node
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant* grantOutbound,
                           const char* strDest, bool fOneShot)
{
    //
    // Initiate outbound network connection
    //
    if (fShutdown)
        return false;
    if (!strDest)
        if (IsLocal(addrConnect) || FindNode((CNetAddr)addrConnect) || CNode::IsBanned(addrConnect) ||
            FindNode(addrConnect.ToStringIPPort().c_str()))
            return false;
    if (strDest && FindNode(strDest))
        return false;

    vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
    CNode* pnode = ConnectNode(addrConnect, strDest);
    vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
    if (fShutdown)
        return false;
    if (!pnode)
        return false;
    if (grantOutbound)
        grantOutbound->MoveTo(pnode->grantOutbound);
    pnode->fNetworkNode = true;
    if (fOneShot)
        pnode->fOneShot = true;

    return true;
}

void ThreadMessageHandler(void* parg)
{
    // Make this thread recognisable as the message handling thread
    RenameThread("neblio-msghand");

    try {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]++;
        ThreadMessageHandler2(parg);
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
    } catch (std::exception& e) {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        PrintException(&e, "ThreadMessageHandler()");
    } catch (...) {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        PrintException(nullptr, "ThreadMessageHandler()");
    }
    NLog.write(b_sev::info, "ThreadMessageHandler exited");
}

void ThreadMessageHandler2(void* /*parg*/)
{
    NLog.write(b_sev::info, "ThreadMessageHandler started");
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (!fShutdown) {
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (CNode* pnode : vNodesCopy)
                pnode->AddRef();
        }

        // Poll the connected nodes for messages
        CNode* pnodeTrickle = nullptr;
        if (!vNodesCopy.empty())
            pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];
        for (CNode* pnode : vNodesCopy) {
            if (pnode->fDisconnect)
                continue;

            // Receive messages
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                    if (!ProcessMessages(pnode))
                        pnode->CloseSocketDisconnect();
            }
            if (fShutdown)
                return;

            // Send messages
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SendMessages(pnode, pnode == pnodeTrickle);
            }
            if (fShutdown)
                return;
        }

        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodesCopy)
                pnode->Release();
        }

        // Wait and allow messages to bunch up.
        // Reduce vnThreadsRunning so StopNode has permission to exit while
        // we're sleeping, but we must always check fShutdown after doing this.
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        MilliSleep(100);
        if (fRequestShutdown)
            StartShutdown();
        vnThreadsRunning[THREAD_MESSAGEHANDLER]++;
        if (fShutdown)
            return;
    }
}

bool BindListenPort(const CService& addrBind, string& strError)
{
    strError = "";
    int nOne = 1;

#ifdef WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int     ret = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (ret != NO_ERROR) {
        strError = fmt::format(
            "Error: TCP/IP socket library failed to start (WSAStartup returned error {})", ret);
        NLog.write(b_sev::err, "{}", strError);
        return false;
    }
#endif

    // Create socket for listening for incoming connections
    struct sockaddr_storage sockaddr;
    socklen_t               len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len)) {
        strError = fmt::format("Error: bind address family for {} not supported", addrBind.ToString());
        NLog.write(b_sev::err, "{}", strError);
        return false;
    }

    SOCKET hListenSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET) {
        strError = fmt::format(
            "Error: Couldn't open socket for incoming connections (socket returned error {})",
            WSAGetLastError());
        NLog.write(b_sev::err, "{}", strError);
        return false;
    }

#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif

#ifndef WIN32
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.  Not an issue on windows.
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int));
#endif

#ifdef WIN32
    // Set to non-blocking, incoming connections will also inherit this
    if (ioctlsocket(hListenSocket, FIONBIO, (u_long*)&nOne) == SOCKET_ERROR)
#else
    if (fcntl(hListenSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
#endif
    {
        strError =
            fmt::format("Error: Couldn't set properties on socket for incoming connections (error {})",
                        WSAGetLastError());
        NLog.write(b_sev::err, "{}", strError);
        return false;
    }

    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
#ifdef WIN32
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&nOne, sizeof(int));
#else
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&nOne, sizeof(int));
#endif
#endif
#ifdef WIN32
        int nProtLevel   = 10 /* PROTECTION_LEVEL_UNRESTRICTED */;
        int nParameterId = 23 /* IPV6_PROTECTION_LEVEl */;
        // this call is allowed to fail
        setsockopt(hListenSocket, IPPROTO_IPV6, nParameterId, (const char*)&nProtLevel, sizeof(int));
#endif
    }

    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR) {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = fmt::format(
                _("Unable to bind to {} on this computer. neblio is probably already running."),
                addrBind.ToString());
        else
            strError =
                fmt::format(_("Unable to bind to {} on this computer (bind returned error {}, {})"),
                            addrBind.ToString(), nErr, strerror(nErr));
        NLog.write(b_sev::err, "{}", strError);
        return false;
    }
    NLog.write(b_sev::info, "Bound to {}", addrBind.ToString());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        strError =
            fmt::format("Error: Listening for incoming connections failed (listen returned error {})",
                        WSAGetLastError());
        NLog.write(b_sev::err, "{}", strError);
        return false;
    }

    vhListenSocket.push_back(hListenSocket);

    if (addrBind.IsRoutable() && fDiscover)
        AddLocal(addrBind, LOCAL_BIND);

    return true;
}

void static Discover()
{
    if (!fDiscover)
        return;

#ifdef WIN32
    // Get local host IP
    char pszHostName[1000] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR) {
        vector<CNetAddr> vaddr;
        if (LookupHost(pszHostName, vaddr)) {
            BOOST_FOREACH (const CNetAddr& addr, vaddr) {
                AddLocal(addr, LOCAL_IF);
            }
        }
    }
#else
    // Get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0) {
        for (struct ifaddrs* ifa = myaddrs; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr)
                continue;
            if ((ifa->ifa_flags & IFF_UP) == 0)
                continue;
            if (strcmp(ifa->ifa_name, "lo") == 0)
                continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0)
                continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                CNetAddr addr(s4->sin_addr);
                if (AddLocal(addr, LOCAL_IF))
                    NLog.write(b_sev::info, "IPv4 {}: {}", ifa->ifa_name, addr.ToString());
            } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                CNetAddr addr(s6->sin6_addr);
                if (AddLocal(addr, LOCAL_IF))
                    NLog.write(b_sev::info, "IPv6 {}: {}", ifa->ifa_name, addr.ToString());
            }
        }
        freeifaddrs(myaddrs);
    }
#endif

    // Don't use external IPv4 discovery, when -onlynet="IPv6"
    if (!IsLimited(NET_IPV4))
        NewThread(ThreadGetMyExternalIP, nullptr);
}

void StartNode(void* /*parg*/)
{
    // Make this thread recognisable as the startup thread
    RenameThread("neblio-start");

    if (semOutbound == nullptr) {
        // initialize semaphore
        int nMaxOutbound = min(MAX_OUTBOUND_CONNECTIONS, (int)GetArg("-maxconnections", 125));
        semOutbound      = new CSemaphore(nMaxOutbound);
    }

    if (pnodeLocalHost == nullptr)
        pnodeLocalHost = new CNode(NodeIDCounter++, INVALID_SOCKET,
                                   CAddress(CService("127.0.0.1", 0), nLocalServices));

    Discover();

    //
    // Start threads
    //

    if (!GetBoolArg("-dnsseed", true))
        NLog.write(b_sev::info, "DNS seeding disabled");
    else if (!NewThread(ThreadDNSAddressSeed, nullptr))
        NLog.write(b_sev::err, "Error: NewThread(ThreadDNSAddressSeed) failed");

    // Map ports with UPnP
    if (fUseUPnP)
        MapPort();

    // Send and receive from sockets, accept connections
    if (!NewThread(ThreadSocketHandler, nullptr))
        NLog.write(b_sev::err, "Error: NewThread(ThreadSocketHandler) failed");

    // Initiate outbound connections from -addnode
    if (!NewThread(ThreadOpenAddedConnections, nullptr))
        NLog.write(b_sev::err, "Error: NewThread(ThreadOpenAddedConnections) failed");

    // Initiate outbound connections
    if (!NewThread(ThreadOpenConnections, nullptr))
        NLog.write(b_sev::err, "Error: NewThread(ThreadOpenConnections) failed");

    // Process messages
    if (!NewThread(ThreadMessageHandler, nullptr))
        NLog.write(b_sev::err, "Error: NewThread(ThreadMessageHandler) failed");

    // Dump network addresses
    if (!NewThread(ThreadDumpAddress, nullptr))
        NLog.write(b_sev::err, "Error; NewThread(ThreadDumpAddress) failed");

    // Mine proof-of-stake blocks in the background
    if (!GetBoolArg("-staking", true))
        NLog.write(b_sev::info, "Staking disabled");
    else if (!NewThread(ThreadStakeMiner, pwalletMain.get()))
        NLog.write(b_sev::err, "Error: NewThread(ThreadStakeMiner) failed");
}

bool StopNode()
{
    NLog.write(b_sev::debug, "StopNode()");
    fShutdown = true;
    nTransactionsUpdated++;
    int64_t nStart = GetTime();
    if (semOutbound)
        for (int i = 0; i < MAX_OUTBOUND_CONNECTIONS; i++)
            semOutbound->post();
    do {
        int nThreadsRunning = 0;
        for (int n = 0; n < THREAD_MAX; n++)
            nThreadsRunning += vnThreadsRunning[n];
        if (nThreadsRunning == 0)
            break;
        if (GetTime() - nStart > 20)
            break;
        MilliSleep(20);
    } while (true);
    if (vnThreadsRunning[THREAD_SOCKETHANDLER] > 0)
        NLog.write(b_sev::info, "ThreadSocketHandler still running");
    if (vnThreadsRunning[THREAD_OPENCONNECTIONS] > 0)
        NLog.write(b_sev::warn, "ThreadOpenConnections still running");
    if (vnThreadsRunning[THREAD_MESSAGEHANDLER] > 0)
        NLog.write(b_sev::warn, "ThreadMessageHandler still running");
    if (vnThreadsRunning[THREAD_RPCLISTENER] > 0)
        NLog.write(b_sev::warn, "ThreadRPCListener still running");
    if (vnThreadsRunning[THREAD_RPCHANDLER] > 0)
        NLog.write(b_sev::warn, "ThreadsRPCServer still running");
#ifdef USE_UPNP
    if (vnThreadsRunning[THREAD_UPNP] > 0)
        NLog.write(b_sev::warn, "ThreadMapPort still running");
#endif
    if (vnThreadsRunning[THREAD_DNSSEED] > 0)
        NLog.write(b_sev::warn, "ThreadDNSAddressSeed still running");
    if (vnThreadsRunning[THREAD_ADDEDCONNECTIONS] > 0)
        NLog.write(b_sev::warn, "ThreadOpenAddedConnections still running");
    if (vnThreadsRunning[THREAD_DUMPADDRESS] > 0)
        NLog.write(b_sev::warn, "ThreadDumpAddresses still running");
    if (vnThreadsRunning[THREAD_STAKE_MINER] > 0)
        NLog.write(b_sev::warn, "ThreadStakeMiner still running");
    while (vnThreadsRunning[THREAD_MESSAGEHANDLER] > 0 || vnThreadsRunning[THREAD_RPCHANDLER] > 0)
        MilliSleep(20);

    MilliSleep(50);
    DumpAddresses();
    return true;
}

class CNetCleanup
{
public:
    CNetCleanup() {}
    ~CNetCleanup()
    {
        // Close sockets
        BOOST_FOREACH (CNode* pnode, vNodes)
            if (pnode->hSocket != INVALID_SOCKET)
                closesocket(pnode->hSocket);
        BOOST_FOREACH (SOCKET hListenSocket, vhListenSocket)
            if (hListenSocket != INVALID_SOCKET)
                if (closesocket(hListenSocket) == SOCKET_ERROR)
                    NLog.write(b_sev::err, "closesocket(hListenSocket) failed with error {}",
                               WSAGetLastError());

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
} instance_of_cnetcleanup;

void RelayTransaction(const CTransaction& tx)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, ss);
}

void RelayTransaction(const CTransaction& tx, const CDataStream& ss)
{
    CInv inv(MSG_TX, tx.GetHash());
    {
        LOCK(cs_mapRelay);
        // Expire old relay messages
        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < GetTime()) {
            mapRelay.erase(vRelayExpiration.front().second);
            vRelayExpiration.pop_front();
        }

        // Save original serialized message so newer versions are preserved
        mapRelay.insert(std::make_pair(inv, ss));
        vRelayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
    }
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (!pnode->fRelayTxes)
            continue;
        LOCK(pnode->cs_filter);
        if (pnode->pfilter) {
            if (pnode->pfilter->IsRelevantAndUpdate(tx))
                pnode->PushInventory(inv);
        } else
            pnode->PushInventory(inv);
    }
}

bool AddNode(const std::string& strNode)
{
    auto lock = vAddedNodes.get_lock();
    for (const std::string& it : vAddedNodes.get_unsafe()) {
        if (strNode == it)
            return false;
    }

    vAddedNodes.get_unsafe().push_back(strNode);
    return true;
}

bool RemoveAddedNode(const std::string& strNode)
{
    auto lock = vAddedNodes.get_lock();
    for (std::vector<std::string>::iterator it = vAddedNodes.get_unsafe().begin();
         it != vAddedNodes.get_unsafe().end(); ++it) {
        if (strNode == *it) {
            vAddedNodes.get_unsafe().erase(it);
            return true;
        }
    }
    return false;
}

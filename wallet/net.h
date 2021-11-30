// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include <boost/array.hpp>
#include <boost/atomic.hpp>
#include <boost/foreach.hpp>
#include <chainparams.h>
#include <deque>
#include <openssl/rand.h>

#ifndef WIN32
#include <arpa/inet.h>
#endif

#include "ThreadSafeHashMap.h"
#include "addrman.h"
#include "bloom.h"
#include "hash.h"
#include "mruset.h"
#include "netbase.h"
#include "protocol.h"

class CRequestTracker;
class CNode;
class CBlockIndex;

/** The maximum number of entries in a locator */
static const unsigned int MAX_LOCATOR_SZ = 101;

inline unsigned int ReceiveFloodSize() { return 1000 * GetArg("-maxreceivebuffer", 5 * 1000); }
inline unsigned int SendBufferSize() { return 1000 * GetArg("-maxsendbuffer", 1 * 1000); }

void           AddOneShot(std::string strDest);
bool           RecvLine(SOCKET hSocket, std::string& strLine);
bool           GetMyExternalIP(CNetAddr& ipRet);
void           AddressCurrentlyConnected(const CService& addr);
CNode*         FindNode(const CNetAddr& ip);
CNode*         FindNode(const std::string& ip);
CNode*         FindNode(const CService& ip);
CNode*         FindNode(const int64_t& nodeID);
CNode*         ConnectNode(CAddress addrConnect, const char* strDest = NULL);
void           MapPort();
unsigned short GetListenPort();
bool           BindListenPort(const CService& bindAddr, std::string& strError = REF(std::string()));
void           StartNode();
bool           StopNode();
void           SocketSendData(CNode* pnode);

enum
{
    LOCAL_NONE,   // unknown
    LOCAL_IF,     // address a local interface listens on
    LOCAL_BIND,   // address explicit bound to
    LOCAL_UPNP,   // address reported by UPnP
    LOCAL_HTTP,   // address reported by whatismyip.com and similar
    LOCAL_MANUAL, // address explicitly specified (-externalip=)

    LOCAL_MAX
};

void     SetLimited(enum Network net, bool fLimited = true);
bool     IsLimited(enum Network net);
bool     IsLimited(const CNetAddr& addr);
bool     AddLocal(const CService& addr, int nScore = LOCAL_NONE);
bool     AddLocal(const CNetAddr& addr, int nScore = LOCAL_NONE);
bool     SeenLocal(const CService& addr);
bool     IsLocal(const CService& addr);
bool     GetLocal(CService& addr, const CNetAddr* paddrPeer = NULL);
bool     IsReachable(const CNetAddr& addr);
void     SetReachable(enum Network net, bool fFlag = true);
CAddress GetLocalAddress(const CNetAddr* paddrPeer = NULL);

// enum
// {
//     MSG_TX = 1,
//     MSG_BLOCK,
// };

struct AddedNodeInfo
{
    std::string strAddedNode;
    CService    resolvedAddress;
    bool        fConnected;
    bool        fInbound;
};

class CRequestTracker
{
public:
    void (*fn)(void*, CDataStream&);
    void* param1;

    explicit CRequestTracker(void (*fnIn)(void*, CDataStream&) = NULL, void* param1In = NULL)
    {
        fn     = fnIn;
        param1 = param1In;
    }

    bool IsNull() { return fn == NULL; }
};

/** Thread types */
enum threadId
{
    THREAD_SOCKETHANDLER,
    THREAD_OPENCONNECTIONS,
    THREAD_MESSAGEHANDLER,
    THREAD_RPCLISTENER,
    THREAD_UPNP,
    THREAD_DNSSEED,
    THREAD_ADDEDCONNECTIONS,
    THREAD_DUMPADDRESS,
    THREAD_RPCHANDLER,
    THREAD_STAKE_MINER,
    THREAD_IMPORT,

    THREAD_MAX
};

extern bool                                        fDiscover;
extern bool                                        fUseUPnP;
extern boost::atomic<uint64_t>                     nLocalServices;
extern boost::atomic<uint64_t>                     nLocalHostNonce;
extern LockedVar<CAddress>                         addrSeenByPeer;
extern boost::array<boost::atomic_int, THREAD_MAX> vnThreadsRunning;
extern LockedVar<CAddrMan>                         addrman;

extern std::vector<CNode*>                  vNodes;
extern CCriticalSection                     cs_vNodes;
extern LockedVar<std::vector<std::string>>  vAddedNodes;
extern std::map<CInv, CDataStream>          mapRelay;
extern std::deque<std::pair<int64_t, CInv>> vRelayExpiration;
extern CCriticalSection                     cs_mapRelay;
extern ThreadSafeHashMap<CInv, int64_t>     mapAlreadyAskedFor;

class CNodeStats
{
public:
    int64_t     nodeid;
    uint64_t    nServices;
    int64_t     nLastSend;
    int64_t     nLastRecv;
    int64_t     nTimeConnected;
    std::string addrName;
    int         nVersion;
    std::string strSubVer;
    bool        fInbound;
    int         nStartingHeight;
    int         nMisbehavior;
};

class CNetMessage
{
public:
    bool in_data; // parsing header (false) or data (true)

    CDataStream    hdrbuf; // partially received header
    CMessageHeader hdr;    // complete header
    unsigned int   nHdrPos;

    CDataStream  vRecv; // received message data
    unsigned int nDataPos;

    CNetMessage(const CMessageHeader::MessageStartChars& pchMessageStartIn, int nTypeIn, int nVersionIn)
        : hdrbuf(nTypeIn, nVersionIn), hdr(pchMessageStartIn), vRecv(nTypeIn, nVersionIn)
    {
        hdrbuf.resize(24);
        in_data  = false;
        nHdrPos  = 0;
        nDataPos = 0;
    }

    bool complete() const
    {
        if (!in_data)
            return false;
        return (hdr.nMessageSize == nDataPos);
    }

    void SetVersion(int nVersionIn)
    {
        hdrbuf.SetVersion(nVersionIn);
        vRecv.SetVersion(nVersionIn);
    }

    int readHeader(const char* pch, unsigned int nBytes);
    int readData(const char* pch, unsigned int nBytes);
};

/** Information about a peer */
class CNode
{
public:
    // socket
    const int64_t              nodeid;
    uint64_t                   nServices;
    SOCKET                     hSocket;
    CDataStream                ssSend;
    size_t                     nSendSize;   // total size of all vSendMsg entries
    size_t                     nSendOffset; // offset inside the first vSendMsg already sent
    std::deque<CSerializeData> vSendMsg;
    CCriticalSection           cs_vSend;

    std::deque<CNetMessage> vRecvMsg;
    CCriticalSection        cs_vRecvMsg;
    int                     nRecvVersion;

    boost::atomic<int64_t> nLastSend;
    boost::atomic<int64_t> nLastRecv;
    boost::atomic<int64_t> nLastSendEmpty;
    boost::atomic<int64_t> nTimeConnected;
    CAddress               addr;
    LockedVar<std::string> addrName;
    CService               addrLocal;
    int                    nVersion;
    std::string            strSubVer;
    bool                   fOneShot;
    bool                   fClient;
    bool                   fInbound;
    boost::atomic<bool>    fNetworkNode;
    bool                   fSuccessfullyConnected;
    bool                   fDisconnect;
    // We use fRelayTxes for two purposes -
    // a) it allows us to not relay tx invs before receiving the peer's version message
    // b) the peer may tell us in their version message that we should not relay tx invs
    //    until they have initialized their bloom filter.
    bool             fRelayTxes;
    CSemaphoreGrant  grantOutbound;
    CCriticalSection cs_filter;
    CBloomFilter*    pfilter;
    int              nRefCount;

protected:
    // Denial-of-service detection/prevention
    // Key is IP address, value is banned-until-time
    static std::map<CNetAddr, int64_t> setBanned;
    static CCriticalSection            cs_setBanned;
    int                                nMisbehavior;

public:
    std::map<uint256, CRequestTracker> mapRequests;
    CCriticalSection                   cs_mapRequests;
    uint256                            hashContinue;
    const CBlockIndex*                 pindexLastGetBlocksBegin;
    uint256                            hashLastGetBlocksEnd;
    int                                nStartingHeight;

    // flood relay
    std::vector<CAddress> vAddrToSend;
    mruset<CAddress>      setAddrKnown;
    bool                  fGetAddr;
    std::set<uint256>     setKnown;
    uint256               hashCheckpointKnown; // ppcoin: known sent sync-checkpoint

    // inventory based relay
    mruset<CInv>                 setInventoryKnown;
    std::vector<CInv>            vInventoryToSend;
    CCriticalSection             cs_inventory;
    std::multimap<int64_t, CInv> mapAskFor;

    CNode(int64_t nodeId, SOCKET hSocketIn, CAddress addrIn, std::string addrNameIn = "",
          bool fInboundIn = false)
        : nodeid(nodeId), ssSend(SER_NETWORK, INIT_PROTO_VERSION), setAddrKnown(5000)
    {
        nServices                = 0;
        hSocket                  = hSocketIn;
        nRecvVersion             = INIT_PROTO_VERSION;
        nLastSend                = 0;
        nLastRecv                = 0;
        nLastSendEmpty           = GetTime();
        nTimeConnected           = GetTime();
        addr                     = addrIn;
        addrName.get()           = addrNameIn == "" ? addr.ToStringIPPort() : addrNameIn;
        nVersion                 = 0;
        strSubVer                = "";
        fOneShot                 = false;
        fClient                  = false; // set by version message
        fInbound                 = fInboundIn;
        fNetworkNode             = false;
        fSuccessfullyConnected   = false;
        fDisconnect              = false;
        nRefCount                = 0;
        nSendSize                = 0;
        nSendOffset              = 0;
        hashContinue             = 0;
        pindexLastGetBlocksBegin = 0;
        hashLastGetBlocksEnd     = 0;
        nStartingHeight          = -1;
        fGetAddr                 = false;
        nMisbehavior             = 0;
        hashCheckpointKnown      = 0;
        fRelayTxes               = false;
        setInventoryKnown.max_size(SendBufferSize() / 1000);
        pfilter = NULL;

        // Be shy and don't send version until we hear
        if (hSocket != INVALID_SOCKET && !fInbound)
            PushVersion();
    }

    ~CNode()
    {
        if (hSocket != INVALID_SOCKET) {
            closesocket(hSocket);
            hSocket = INVALID_SOCKET;
        }
        if (pfilter)
            delete pfilter;
    }

private:
    CNode(const CNode&);
    void operator=(const CNode&);

public:
    std::string GetAddrName() const { return addrName.get(); }

    int GetRefCount()
    {
        assert(nRefCount >= 0);
        return nRefCount;
    }

    // requires LOCK(cs_vRecvMsg)
    unsigned int GetTotalRecvSize()
    {
        unsigned int total = 0;
        BOOST_FOREACH (const CNetMessage& msg, vRecvMsg)
            total += msg.vRecv.size() + 24;
        return total;
    }

    // requires LOCK(cs_vRecvMsg)
    bool ReceiveMsgBytes(const char* pch, unsigned int nBytes);

    // requires LOCK(cs_vRecvMsg)
    void SetRecvVersion(int nVersionIn)
    {
        nRecvVersion = nVersionIn;
        BOOST_FOREACH (CNetMessage& msg, vRecvMsg)
            msg.SetVersion(nVersionIn);
    }

    CNode* AddRef()
    {
        nRefCount++;
        return this;
    }

    void Release() { nRefCount--; }

    void AddAddressKnown(const CAddress& addrIn) { setAddrKnown.insert(addrIn); }

    void PushAddress(const CAddress& addrIn)
    {
        // Known checking here is only to save space from duplicates.
        // SendMessages will filter it again for knowns that were added
        // after addresses were pushed.
        if (addrIn.IsValid() && !setAddrKnown.count(addrIn))
            vAddrToSend.push_back(addrIn);
    }

    void AddInventoryKnown(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            setInventoryKnown.insert(inv);
        }
    }

    void PushInventory(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            if (!setInventoryKnown.count(inv))
                vInventoryToSend.push_back(inv);
        }
    }

    void AskFor(const CInv& inv)
    {
        // We're using mapAskFor as a priority queue,
        // the key is the earliest time the request can be sent
        int64_t nRequestTime = 0;

        if (fDebugNet)
            NLog.write(b_sev::debug, "askfor {}   {} ({})", inv.ToString(), nRequestTime,
                       DateTimeStrFormat("%H:%M:%S", nRequestTime / 1000000));

        // Make sure not to reuse time indexes to keep things in the same order
        int64_t        nNow = (GetTime() - 1) * 1000000;
        static int64_t nLastTime;
        ++nLastTime;
        nNow      = std::max(nNow, nLastTime);
        nLastTime = nNow;

        // Each retry is 2 minutes after the last
        nRequestTime = std::max(nRequestTime + 2 * 60 * 1000000, nNow);
        mapAlreadyAskedFor.set(inv, nRequestTime);
        mapAskFor.insert(std::make_pair(nRequestTime, inv));
    }

    void BeginMessage(const char* pszCommand)
    {
        ENTER_CRITICAL_SECTION(cs_vSend);
        assert(ssSend.size() == 0);
        ssSend << CMessageHeader(Params().MessageStart(), pszCommand, 0);
        if (fDebug)
            NLog.write(b_sev::debug, "sending: {} ", pszCommand);
    }

    void AbortMessage()
    {
        ssSend.clear();

        LEAVE_CRITICAL_SECTION(cs_vSend);

        if (fDebug)
            NLog.write(b_sev::debug, "(aborted)");
    }

    void EndMessage()
    {
        const boost::optional<std::string> dropMessageTest = mapArgs.get("-dropmessagestest");
        if (dropMessageTest && GetRand(atoi(*dropMessageTest)) == 0) {
            NLog.write(b_sev::warn, "dropmessages DROPPING SEND MESSAGE");
            AbortMessage();
            return;
        }

        if (ssSend.size() == 0)
            return;

        // Set the size
        unsigned int nSize = ssSend.size() - CMessageHeader::HEADER_SIZE;
        memcpy((char*)&ssSend[CMessageHeader::MESSAGE_SIZE_OFFSET], &nSize, sizeof(nSize));

        // Set the checksum
        uint256      hash      = Hash(ssSend.begin() + CMessageHeader::HEADER_SIZE, ssSend.end());
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        assert(ssSend.size() >= CMessageHeader::CHECKSUM_OFFSET + sizeof(nChecksum));
        memcpy((char*)&ssSend[CMessageHeader::CHECKSUM_OFFSET], &nChecksum, sizeof(nChecksum));

        if (fDebug) {
            NLog.write(b_sev::debug, "({} bytes)", nSize);
        }

        std::deque<CSerializeData>::iterator it = vSendMsg.insert(vSendMsg.end(), CSerializeData());
        ssSend.GetAndClear(*it);
        nSendSize += (*it).size();

        // If write queue empty, attempt "optimistic write"
        if (it == vSendMsg.begin())
            SocketSendData(this);

        LEAVE_CRITICAL_SECTION(cs_vSend);
    }

    void PushVersion();

    void PushMessage(const char* pszCommand)
    {
        try {
            BeginMessage(pszCommand);
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1>
    void PushMessage(const char* pszCommand, const T1& a1)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4,
                     const T5& a5)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4,
                     const T5& a5, const T6& a6)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4,
                     const T5& a5, const T6& a6, const T7& a7)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7,
              typename T8>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4,
                     const T5& a5, const T6& a6, const T7& a7, const T8& a8)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7,
              typename T8, typename T9>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4,
                     const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    void PushRequest(const char* pszCommand, void (*fn)(void*, CDataStream&), void* param1)
    {
        uint256 hashReply;
        randombytes_buf((unsigned char*)&hashReply, sizeof(hashReply));

        {
            LOCK(cs_mapRequests);
            mapRequests[hashReply] = CRequestTracker(fn, param1);
        }

        PushMessage(pszCommand, hashReply);
    }

    template <typename T1>
    void PushRequest(const char* pszCommand, const T1& a1, void (*fn)(void*, CDataStream&), void* param1)
    {
        uint256 hashReply;
        randombytes_buf((unsigned char*)&hashReply, sizeof(hashReply));

        {
            LOCK(cs_mapRequests);
            mapRequests[hashReply] = CRequestTracker(fn, param1);
        }

        PushMessage(pszCommand, hashReply, a1);
    }

    template <typename T1, typename T2>
    void PushRequest(const char* pszCommand, const T1& a1, const T2& a2, void (*fn)(void*, CDataStream&),
                     void* param1)
    {
        uint256 hashReply;
        randombytes_buf((unsigned char*)&hashReply, sizeof(hashReply));

        {
            LOCK(cs_mapRequests);
            mapRequests[hashReply] = CRequestTracker(fn, param1);
        }

        PushMessage(pszCommand, hashReply, a1, a2);
    }

    void PushGetBlocks(const CBlockIndex* pindexBegin, uint256 hashEnd);
    bool IsSubscribed(unsigned int nChannel);
    void Subscribe(unsigned int nChannel, unsigned int nHops = 0);
    void CancelSubscribe(unsigned int nChannel);
    void CloseSocketDisconnect();

    // Denial-of-service detection/prevention
    // The idea is to detect peers that are behaving
    // badly and disconnect/ban them, but do it in a
    // one-coding-mistake-won't-shatter-the-entire-network
    // way.
    // IMPORTANT:  There should be nothing I can give a
    // node that it will forward on that will make that
    // node's peers drop it. If there is, an attacker
    // can isolate a node and/or try to split the network.
    // Dropping a node for sending stuff that is invalid
    // now but might be valid in a later version is also
    // dangerous, because it can cause a network split
    // between nodes running old code and nodes running
    // new code.
    static void ClearBanned(); // needed for unit testing
    static bool IsBanned(const CNetAddr& ip);
    static void Ban(const CNetAddr& ip, int64_t ban_time_offset, bool since_unix_epoch);
    static bool Unban(const CNetAddr& ip);
    static std::map<CNetAddr, int64_t> GetBanned();
    bool                               Misbehaving(int howmuch); // 1 == a little, 100 == a lot
    void                               copyStats(CNodeStats& stats);
};

inline void RelayInventory(const CInv& inv)
{
    // Put on lists to offer to the other nodes
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
            pnode->PushInventory(inv);
    }
}

class CTransaction;
void RelayTransaction(const CTransaction& tx);
void RelayTransaction(const CTransaction& tx, const CDataStream& ss);

bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant* grantOutbound = nullptr,
                           const char* strDest = nullptr, bool fOneShot = false);

/// functions idea from peercoin; perhaps they should be put in a class, together wish vAddedNodes
bool                       AddNode(const std::string& strNode);
bool                       RemoveAddedNode(const std::string& strNode);
std::vector<AddedNodeInfo> GetAddedNodeInfo();

#endif

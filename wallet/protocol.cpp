// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protocol.h"
#include "netbase.h"
#include "util.h"
#include <boost/filesystem.hpp>

#ifndef WIN32
#include <arpa/inet.h>
#endif

namespace fs = boost::filesystem;

static const char* ppszTypeName[] = {"ERROR", "tx", "block", "filtered block"};

/** Username used when cookie authentication is in use (arbitrary, only for
 * recognizability in debugging/logging purposes)
 */
static const std::string COOKIEAUTH_USER = "__cookie__";
/** Default name for auth cookie file */
static const std::string COOKIEAUTH_FILE = ".cookie";

CMessageHeader::CMessageHeader(const MessageStartChars& pchMessageStartIn)
{
    memcpy(pchMessageStart, pchMessageStartIn, sizeof(pchMessageStart));
    memset(pchCommand, 0, sizeof(pchCommand));
    pchCommand[1] = 1;
    nMessageSize  = -1;
    nChecksum     = 0;
}

CMessageHeader::CMessageHeader(const MessageStartChars& pchMessageStartIn, const char* pszCommand,
                               unsigned int nMessageSizeIn)
{
    memcpy(pchMessageStart, pchMessageStartIn, sizeof(pchMessageStart));
    strncpy(pchCommand, pszCommand, COMMAND_SIZE);
    nMessageSize = nMessageSizeIn;
    nChecksum    = 0;
}

std::string CMessageHeader::GetCommand() const
{
    if (pchCommand[COMMAND_SIZE - 1] == 0)
        return std::string(pchCommand, pchCommand + strlen(pchCommand));
    else
        return std::string(pchCommand, pchCommand + COMMAND_SIZE);
}

bool CMessageHeader::IsValid(const MessageStartChars& pchMessageStartIn) const
{
    // Check start string
    if (memcmp(pchMessageStart, pchMessageStartIn, MESSAGE_START_SIZE) != 0)
        return false;

    // Check the command string for errors
    for (const char* p1 = pchCommand; p1 < pchCommand + COMMAND_SIZE; p1++) {
        if (*p1 == 0) {
            // Must be all zeros after the first zero
            for (; p1 < pchCommand + COMMAND_SIZE; p1++)
                if (*p1 != 0)
                    return false;
        } else if (*p1 < ' ' || *p1 > 0x7E)
            return false;
    }

    // Message size
    if (nMessageSize > MAX_SIZE) {
        printf("CMessageHeader::IsValid() : (%s, %u bytes) nMessageSize > MAX_SIZE\n",
               GetCommand().c_str(), nMessageSize);
        return false;
    }

    return true;
}

CAddress::CAddress() : CService() { Init(); }

CAddress::CAddress(CService ipIn, uint64_t nServicesIn) : CService(ipIn)
{
    Init();
    nServices = nServicesIn;
}

void CAddress::Init()
{
    nServices = NODE_NETWORK;
    nTime     = 100000000;
    nLastTry  = 0;
}

CInv::CInv()
{
    type = 0;
    hash = 0;
}

CInv::CInv(int typeIn, const uint256& hashIn)
{
    type = typeIn;
    hash = hashIn;
}

CInv::CInv(const std::string& strType, const uint256& hashIn)
{
    unsigned int i;
    for (i = 1; i < ARRAYLEN(ppszTypeName); i++) {
        if (strType == ppszTypeName[i]) {
            type = i;
            break;
        }
    }
    if (i == ARRAYLEN(ppszTypeName))
        throw std::out_of_range(
            strprintf("CInv::CInv(string, uint256) : unknown type '%s'", strType.c_str()));
    hash = hashIn;
}

bool operator<(const CInv& a, const CInv& b)
{
    return (a.type < b.type || (a.type == b.type && a.hash < b.hash));
}

bool operator==(const CInv& a, const CInv& b) { return a.type == b.type && a.hash == b.hash; }

bool CInv::IsKnownType() const { return (type >= 1 && type < (int)ARRAYLEN(ppszTypeName)); }

const char* CInv::GetCommand() const
{
    if (!IsKnownType())
        throw std::out_of_range(strprintf("CInv::GetCommand() : type=%d unknown type", type));
    return ppszTypeName[type];
}

std::string CInv::ToString() const
{
    return strprintf("%s %s", GetCommand(), hash.ToString().substr(0, 20).c_str());
}

void CInv::print() const { printf("CInv(%s)\n", ToString().c_str()); }

/** Get name of RPC authentication cookie file */
static fs::path GetAuthCookieFile(bool temp = false)
{
    std::string arg = GetArg("-rpccookiefile", COOKIEAUTH_FILE);
    if (temp) {
        arg += ".tmp";
    }
    fs::path path(arg);
    if (!path.is_complete())
        path = GetDataDir() / path;
    return path;
}

bool GenerateAuthCookie(std::string* cookie_out)
{
    static constexpr const size_t COOKIE_SIZE = 32;

    std::array<unsigned char, COOKIE_SIZE> rand_pwd;
    if (!RandomBytesToBuffer(rand_pwd.data(), rand_pwd.size())) {
        printf("Generating a random password for the cookie failed");
        return false;
    }
    const std::string cookie = COOKIEAUTH_USER + ":" + HexStr(rand_pwd.begin(), rand_pwd.end());

    /** the umask determines what permissions are used to create this file -
     * these are set to 077 in init.cpp unless overridden with -sysperms.
     */
    std::ofstream file;
    fs::path      filepath_tmp = GetAuthCookieFile(true);
    file.open(filepath_tmp.string().c_str());
    if (!file.is_open()) {
        printf("Unable to open cookie authentication file %s for writing\n",
               filepath_tmp.string().c_str());
        return false;
    }
    file << cookie;
    file.close();

    fs::path filepath = GetAuthCookieFile(false);
    if (!RenameOver(filepath_tmp, filepath)) {
        printf("Unable to rename cookie authentication file %s to %s\n", filepath_tmp.string().c_str(),
               filepath.string().c_str());
        return false;
    }
    printf("Generated RPC authentication cookie %s\n", filepath.string().c_str());

    if (cookie_out) {
        *cookie_out = cookie;
    }
    return true;
}

boost::optional<std::string> GetAuthCookie()
{
    std::ifstream file;
    std::string   cookie;
    fs::path      filepath = GetAuthCookieFile();
    file.open(filepath.string().c_str());
    if (!file.is_open())
        return boost::none;
    std::getline(file, cookie);
    file.close();

    return cookie;
}

void DeleteAuthCookie()
{
    try {
        fs::remove(GetAuthCookieFile());
    } catch (const fs::filesystem_error& e) {
        printf("%s: Unable to remove random auth cookie file: %s\n", __func__, e.what());
    }
}

//
// Alert system
//

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <map>

#include "alert.h"
#include "key.h"
#include "net.h"
#include "sync.h"
#include "ui_interface.h"

using namespace std;

map<uint256, CAlert> mapAlerts;
CCriticalSection     cs_mapAlerts;

void CUnsignedAlert::SetNull()
{
    nVersion    = 1;
    nRelayUntil = 0;
    nExpiration = 0;
    nID         = 0;
    nCancel     = 0;
    setCancel.clear();
    nMinVer = 0;
    nMaxVer = 0;
    setSubVer.clear();
    nPriority = 0;

    strComment.clear();
    strStatusBar.clear();
    strReserved.clear();
}

std::string CUnsignedAlert::ToString() const
{
    std::string strSetCancel;
    for (int n : setCancel)
        strSetCancel += fmt::format("{} ", n);
    std::string strSetSubVer;
    for (std::string str : setSubVer)
        strSetSubVer += "\"" + str + "\" ";
    return fmt::format("CAlert(\n"
                     "    nVersion     = {}\n"
                     "    nRelayUntil  = {}\n"
                     "    nExpiration  = {}\n"
                     "    nID          = {}\n"
                     "    nCancel      = {}\n"
                     "    setCancel    = {}\n"
                     "    nMinVer      = {}\n"
                     "    nMaxVer      = {}\n"
                     "    setSubVer    = {}\n"
                     "    nPriority    = {}\n"
                     "    strComment   = \"{}\"\n"
                     "    strStatusBar = \"{}\"\n"
                     ")\n",
                     nVersion, nRelayUntil, nExpiration, nID, nCancel, strSetCancel.c_str(), nMinVer,
                     nMaxVer, strSetSubVer.c_str(), nPriority, strComment.c_str(), strStatusBar.c_str());
}

void CUnsignedAlert::print() const { NLog.write(b_sev::info, "{}", ToString()); }

void CAlert::SetNull()
{
    CUnsignedAlert::SetNull();
    vchMsg.clear();
    vchSig.clear();
}

bool CAlert::IsNull() const { return (nExpiration == 0); }

uint256 CAlert::GetHash() const { return Hash(this->vchMsg.begin(), this->vchMsg.end()); }

bool CAlert::IsInEffect() const { return (GetAdjustedTime() < nExpiration); }

bool CAlert::Cancels(const CAlert& alert) const
{
    if (!IsInEffect())
        return false; // this was a no-op before 31403
    return (alert.nID <= nCancel || setCancel.count(alert.nID));
}

bool CAlert::AppliesTo(int nVersionIn, std::string strSubVerIn) const
{
    // TODO: rework for client-version-embedded-in-strSubVer ?
    return (IsInEffect() && nMinVer <= nVersionIn && nVersionIn <= nMaxVer &&
            (setSubVer.empty() || setSubVer.count(strSubVerIn)));
}

bool CAlert::AppliesToMe() const
{
    return AppliesTo(PROTOCOL_VERSION,
                     FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<std::string>()));
}

bool CAlert::RelayTo(CNode* pnode) const
{
    if (!IsInEffect())
        return false;
    // returns true if wasn't already contained in the set
    if (pnode->setKnown.insert(GetHash()).second) {
        if (AppliesTo(pnode->nVersion, pnode->strSubVer) || AppliesToMe() ||
            GetAdjustedTime() < nRelayUntil) {
            pnode->PushMessage("alert", *this);
            return true;
        }
    }
    return false;
}

bool CAlert::CheckSignature() const
{
    CKey key;
    if (!key.SetPubKey(Params().AlertKey()))
        return NLog.error("CAlert::CheckSignature() : SetPubKey failed");
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return NLog.error("CAlert::CheckSignature() : verify signature failed");

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedAlert*)this;
    return true;
}

CAlert CAlert::getAlertByHash(const uint256& hash)
{
    CAlert retval;
    {
        LOCK(cs_mapAlerts);
        map<uint256, CAlert>::iterator mi = mapAlerts.find(hash);
        if (mi != mapAlerts.end())
            retval = mi->second;
    }
    return retval;
}

bool CAlert::ProcessAlert(bool fThread)
{
    if (!CheckSignature())
        return false;
    if (!IsInEffect())
        return false;

    // alert.nID=max is reserved for if the alert key is
    // compromised. It must have a pre-defined message,
    // must never expire, must apply to all versions,
    // and must cancel all previous
    // alerts or it will be ignored (so an attacker can't
    // send an "everything is OK, don't panic" version that
    // cannot be overridden):
    int maxInt = std::numeric_limits<int>::max();
    if (nID == maxInt) {
        if (!(nExpiration == maxInt && nCancel == (maxInt - 1) && nMinVer == 0 && nMaxVer == maxInt &&
              setSubVer.empty() && nPriority == maxInt &&
              strStatusBar == "URGENT: Alert key compromised, upgrade required"))
            return false;
    }

    {
        LOCK(cs_mapAlerts);
        // Cancel previous alerts
        for (map<uint256, CAlert>::iterator mi = mapAlerts.begin(); mi != mapAlerts.end();) {
            const CAlert& alert = (*mi).second;
            if (Cancels(alert)) {
                NLog.write(b_sev::warn, "cancelling alert {}", alert.nID);
                uiInterface.NotifyAlertChanged((*mi).first, CT_DELETED);
                mapAlerts.erase(mi++);
            } else if (!alert.IsInEffect()) {
                NLog.write(b_sev::warn, "expiring alert {}", alert.nID);
                uiInterface.NotifyAlertChanged((*mi).first, CT_DELETED);
                mapAlerts.erase(mi++);
            } else
                mi++;
        }

        // Check if this alert has been cancelled
        for (PAIRTYPE(const uint256, CAlert) & item : mapAlerts) {
            const CAlert& alert = item.second;
            if (alert.Cancels(*this)) {
                NLog.write(b_sev::info, "alert already cancelled by {}", alert.nID);
                return false;
            }
        }

        // Add to mapAlerts
        mapAlerts.insert(make_pair(GetHash(), *this));
        // Notify UI and -alertnotify if it applies to me
        if (AppliesToMe()) {
            uiInterface.NotifyAlertChanged(GetHash(), CT_NEW);
            std::string strCmd = GetArg("-alertnotify", "");
            if (!strCmd.empty()) {
                // Alert text should be plain ascii coming from a trusted source, but to
                // be safe we first strip anything not in safeChars, then add single quotes around
                // the whole string before passing it to the shell:
                std::string singleQuote("'");
                // safeChars chosen to allow simple messages/URLs/email addresses, but avoid anything
                // even possibly remotely dangerous like & or >
                std::string safeChars(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890 .,;_/:?@");
                std::string safeStatus;
                for (std::string::size_type i = 0; i < strStatusBar.size(); i++) {
                    if (safeChars.find(strStatusBar[i]) != std::string::npos)
                        safeStatus.push_back(strStatusBar[i]);
                }
                safeStatus = singleQuote + safeStatus + singleQuote;
                boost::replace_all(strCmd, "%s", safeStatus);

                if (fThread)
                    boost::thread t(runCommand, strCmd); // thread runs free
                else
                    runCommand(strCmd);
            }
        }
    }

    NLog.write(b_sev::info, "accepted alert {}, AppliesToMe()={}", nID, AppliesToMe());
    return true;
}

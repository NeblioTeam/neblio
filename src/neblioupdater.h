#ifndef NEBLIOUPDATER_H
#define NEBLIOUPDATER_H

#include <string>

#include "version.h"
#include "util.h"
#include "clientversion.h"
#include "neblioversion.h"
#include "neblioreleaseinfo.h"

#define CURL_STATICLIB
#include <curl/curl.h>

class NeblioUpdater
{

public:
    static const std::string ClientVersionSrcFileLink;
    static const std::string ReleasesInfoURL;

    NeblioUpdater();
    void checkIfUpdateIsAvailable(boost::promise<bool> &updateIsAvailablePromise, NeblioReleaseInfo &lastRelease);

    static std::string GetFileFromHTTPS(const std::string &url, bool IncludeProgressBar);
    static NeblioVersion ParseVersion(const std::string& versionFile);
    static std::string GetDefineFromCFile(const std::string& fileData, const std::string &fieldName);
    static std::string RemoveCFileComments(const std::string& fileData);
};

struct RemovePreReleaseFunctor
{
    bool operator() (const NeblioReleaseInfo& r)
    {
        return r.getIsPreRelease();
    }
};

struct NeblioReleaseVersionGreaterComparator
{
    bool operator() (const NeblioReleaseInfo& r1, const NeblioReleaseInfo& r2)
    {
        return r1.getVersion() > r2.getVersion();
    }
};


#endif // NEBLIOUPDATER_H

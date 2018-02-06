#ifndef NEBLIOUPDATER_H
#define NEBLIOUPDATER_H

#include <string>

#include "version.h"
#include "util.h"
#include "clientversion.h"
#include "neblioversion.h"

#define CURL_STATICLIB
#include <curl/curl.h>

class NeblioUpdater
{

public:
    static const std::string UpdateInfoLink;

    NeblioUpdater();
    void checkIfUpdateIsAvailable(boost::promise<bool> &updateIsAvailablePromise, NeblioVersion &lastVersion);

    static std::string GetFileFromHTTPS(const std::string &url, bool IncludeProgressBar);
    static NeblioVersion ParseVersion(const std::string& versionFile);
    static std::string GetDefineFromCFile(const std::string& fileData, const std::string &fieldName);
    static std::string RemoveCFileComments(const std::string& fileData);
};

#endif // NEBLIOUPDATER_H

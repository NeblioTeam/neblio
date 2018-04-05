#ifndef NEBLIORELEASEINFO_H
#define NEBLIORELEASEINFO_H

#include "json_spirit.h"
#include "neblioversion.h"

#include <string>

class NeblioReleaseInfo
{
    std::string versionStr;
    NeblioVersion version;
    std::string htmlURL;
    std::string bodyText;
    bool isPreRelease;

    static std::string GetStrField(const json_spirit::Object& data, const std::string& fieldName);
    static bool GetBoolField(const json_spirit::Object &data, const std::string &fieldName);
    static NeblioVersion VersionTagStrToObj(std::string VersionStr);
    static NeblioReleaseInfo ParseSingleReleaseData(const json_spirit::Object& data);

public:
    NeblioReleaseInfo();

    static std::vector<NeblioReleaseInfo> ParseAllReleaseDataFromJSON(const std::string& data);
    bool getIsPreRelease() const;
    NeblioVersion getVersion() const;
    std::string getUpdateDescription() const;
    std::string getDownloadLink() const;
    void clear();
};


#endif // NEBLIORELEASEINFO_H

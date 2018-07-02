#include "neblioreleaseinfo.h"

#include "boost/algorithm/string.hpp"
#include "util.h"

NeblioReleaseInfo::NeblioReleaseInfo() { this->clear(); }

NeblioReleaseInfo NeblioReleaseInfo::ParseSingleReleaseData(const json_spirit::Object& data)
{
    NeblioReleaseInfo release;
    release.versionStr   = GetStrField(data, "tag_name");
    release.htmlURL      = GetStrField(data, "html_url");
    release.isPreRelease = GetBoolField(data, "prerelease");
    release.bodyText     = GetStrField(data, "body");
    release.version      = VersionTagStrToObj(release.versionStr);
    return release;
}

std::string NeblioReleaseInfo::GetStrField(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_str();
}

bool NeblioReleaseInfo::GetBoolField(const json_spirit::Object& data, const std::string& fieldName)
{
    json_spirit::Value val;
    val = json_spirit::find_value(data, fieldName);
    return val.get_bool();
}

std::vector<NeblioReleaseInfo> NeblioReleaseInfo::ParseAllReleaseDataFromJSON(const std::string& data)
{
    json_spirit::Value value;
    json_spirit::read_or_throw(data, value);
    json_spirit::Array             array = value.get_array();
    std::vector<NeblioReleaseInfo> releases;
    for (long i = 0; i < static_cast<long>(array.size()); i++) {
        NeblioReleaseInfo rel;
        try {
            rel = ParseSingleReleaseData(array[i].get_obj());
        } catch (std::exception& ex) {
            std::string msg = std::string("Unable to parse a release: ") + ex.what();
            printf("%s", msg.c_str());
        }
        releases.push_back(rel);
    }
    return releases;
}

bool NeblioReleaseInfo::getIsPreRelease() const { return isPreRelease; }

NeblioVersion NeblioReleaseInfo::getVersion() const { return version; }

std::string NeblioReleaseInfo::getUpdateDescription() const { return bodyText; }

std::string NeblioReleaseInfo::getDownloadLink() const { return htmlURL; }

void NeblioReleaseInfo::clear()
{
    versionStr.clear();
    version.clear();
    htmlURL.clear();
    isPreRelease = false;
}

NeblioVersion NeblioReleaseInfo::VersionTagStrToObj(std::string VersionStr)
{
    // remove the "v" from the tag
    boost::replace_all(VersionStr, "v", "");
    std::vector<std::string> versionAndBetaParts;
    boost::split(versionAndBetaParts, VersionStr, boost::is_any_of("-"));
    std::string mainVersionPart; // like 1.2.3.4
    std::string betaPart;        // like beta.1
    // get the first part of the version
    if (versionAndBetaParts.size() > 0) {
        mainVersionPart = versionAndBetaParts[0];
    } else {
        throw std::length_error("Splitting the version and beta parts of the version string has led to "
                                "a zero sized vector. Unable to parse " +
                                VersionStr);
    }
    NeblioVersion version(0, 0, 0, 0);

    // get the beta part of the version (that comes after the "-")
    if (versionAndBetaParts.size() > 1) {
        betaPart = versionAndBetaParts[1];
        boost::replace_all(VersionStr, " ", "");
        boost::replace_all(VersionStr, "\t", "");
    }

    std::vector<std::string> mainVersionParts;
    boost::split(mainVersionParts, mainVersionPart, boost::is_any_of("."));
    if (mainVersionParts.size() > 1) {
        version.setMajor(FromString<int>(mainVersionParts[0]));
        version.setMinor(FromString<int>(mainVersionParts[1]));
    } else {
        throw std::length_error("Splitting the main version part, that should be like 1.2.3.4, did not "
                                "yield any results parts. Failed at splitting " +
                                mainVersionPart);
    }

    if (mainVersionParts.size() > 2) {
        version.setRevision(FromString<int>(mainVersionParts[2]));
    }

    if (mainVersionParts.size() > 3) {
        version.setBuild(FromString<int>(mainVersionParts[3]));
    }
    return version;
}

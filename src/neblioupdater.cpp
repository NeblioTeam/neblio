#include "neblioupdater.h"
#include "util.h"

#include <iostream>
#include <vector>
#include <boost/regex.hpp>
#include <sstream>
#include <boost/algorithm/string.hpp>

const std::string NeblioUpdater::UpdateInfoLink  = "https://raw.githubusercontent.com/NeblioTeam/neblio/master/src/clientversion.h";
const std::string NeblioUpdater::ReleasesInfoURL = "https://api.github.com/repos/NeblioTeam/neblio/releases";

size_t CurlWrite_CallbackFunc_StdString(void *contents, size_t size,
                                        size_t nmemb, std::deque<char> *s) {
  size_t newLength = size * nmemb;
  size_t oldLength = s->size();
  try {
    s->resize(oldLength + newLength);
  } catch (std::bad_alloc &e) {
    std::stringstream msg;
    msg << "Error allocating memory: " << e.what() << std::endl;
    printf("%s", msg.str().c_str());
    return 0;
  }

  std::copy((char *)contents, (char *)contents + newLength,
            s->begin() + oldLength);
  return size * nmemb;
}

int CurlProgress_CallbackFunc(void *, double TotalToDownload,
                              double NowDownloaded, double /*TotalToUpload*/,
                              double /*NowUploaded*/) {
  std::clog << "Download progress: " <<
               ToString(NowDownloaded) << " / " <<
               ToString(TotalToDownload) << std::endl;
  return CURLE_OK;
}

std::string NeblioUpdater::GetFileFromHTTPS(const std::string &url, bool IncludeProgressBar) {
  CURL *curl;
  CURLcode res;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();
  std::deque<char> s;
  if (curl) {

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // verify ssl peer
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // verify ssl hostname
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     CurlWrite_CallbackFunc_StdString);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Dark Secret Ninja/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

    if (IncludeProgressBar) {
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, false);
      curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION,
                       CurlProgress_CallbackFunc);
    } else {
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, true);
    }
    //        curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L); //verbose output

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if (res != CURLE_OK) {
      std::string errorMsg(curl_easy_strerror(res));
      throw std::runtime_error(std::string(errorMsg).c_str());
    } else {
      long http_response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
      if (http_response_code != 200) {
          throw std::runtime_error("Error retrieving data with https protocol, error . " + ToString(http_response_code) +
                                   "Probably the URL is invalid.");
      }
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  std::string fileStr(s.begin(), s.end());
  return fileStr;
}

NeblioUpdater::NeblioUpdater()
{
}

void NeblioUpdater::checkIfUpdateIsAvailable(boost::promise<bool> &updateIsAvailablePromise, NeblioVersion& lastVersion)
{
    NeblioVersion remoteVersion;
    NeblioVersion localVersion;
    std::string versionFile;
    try {
        versionFile = GetFileFromHTTPS(UpdateInfoLink, 0);
        versionFile = RemoveCFileComments(versionFile);
    } catch (std::exception& ex) {
        std::string m("Unable to download update file: " + std::string(ex.what()) + "\n");
        printf("%s", m.c_str());
        updateIsAvailablePromise.set_exception(boost::current_exception());
        return;
    }
    try {
        remoteVersion = ParseVersion(versionFile);
        localVersion  = NeblioVersion(CLIENT_VERSION_MAJOR,
                                      CLIENT_VERSION_MINOR,
                                      CLIENT_VERSION_REVISION,
                                      CLIENT_VERSION_BUILD);
    } catch (std::exception& ex) {
        std::stringstream msg;
        msg << "Unable to parse version data during update check: " << ex.what() << std::endl;
        printf("%s", msg.str().c_str());
        updateIsAvailablePromise.set_exception(boost::current_exception());
        return;
    }
    lastVersion = remoteVersion;
    updateIsAvailablePromise.set_value(remoteVersion > localVersion);
}

NeblioVersion NeblioUpdater::ParseVersion(const std::string &versionFile)
{
    int majorVersion    = FromString<int>(GetDefineFromCFile(versionFile, "CLIENT_VERSION_MAJOR"));
    int minorVersion    = FromString<int>(GetDefineFromCFile(versionFile, "CLIENT_VERSION_MINOR"));
    int revisionVersion = FromString<int>(GetDefineFromCFile(versionFile, "CLIENT_VERSION_REVISION"));
    int buildVersion    = FromString<int>(GetDefineFromCFile(versionFile, "CLIENT_VERSION_BUILD"));
    std::stringstream msg;
    msg << majorVersion    << "." <<
           minorVersion    << "." <<
           revisionVersion << "." <<
           buildVersion    << std::endl;
    printf("%s", msg.str().c_str());
    return NeblioVersion(majorVersion, minorVersion, revisionVersion, buildVersion);
}

std::string NeblioUpdater::GetDefineFromCFile(const std::string &fileData, const std::string& fieldName)
{
    //regex of define in one or multiple lines
    const std::string regex_str = ".*\\s*#define\\s+" + fieldName + "\\s+[\\s*|(\\n)]+([^\\s]+)\\s*.*";
    boost::regex pieces_regex(regex_str);
    boost::smatch pieces_match;
    std::string piece;
    bool match_found = boost::regex_match(fileData, pieces_match, pieces_regex);
    if (match_found) {
        piece = pieces_match[1];
    } else {
        std::string error = "Unable to find match for " + fieldName + " in the downloaded file.";
        throw std::runtime_error(error.c_str());
    }
    return piece;
}

std::string NeblioUpdater::RemoveCFileComments(const std::string &fileData)
{
    std::string result = fileData;

    //remove carriage return, as they could hinder detecting new lines
    std::string carriage_return_regex_str("\\r", boost::match_not_dot_newline);
    boost::regex carriage_return_regex(carriage_return_regex_str);
    result = boost::regex_replace(result, carriage_return_regex, "");

    //remove single line comments (//)
    std::string line_comments_regex_str("\\/\\/.*\\n");
    boost::regex line_comments_regex(line_comments_regex_str);
    result = boost::regex_replace(result, line_comments_regex, "", boost::match_not_dot_newline);

    //remove multi-line comments (/* */)
    std::string multiline_comments_regex_str("/\\*(.*?)\\*/"); // The "?" is to turn off greediness
    boost::regex multiline_comments_regex(multiline_comments_regex_str);
    result = boost::regex_replace(result, multiline_comments_regex, "");

    return result;
}

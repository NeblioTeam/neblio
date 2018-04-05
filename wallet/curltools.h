#ifndef CURLTOOLS_H
#define CURLTOOLS_H

#include "util.h"

#define CURL_STATICLIB
#include <curl/curl.h>

class cURLTools
{
public:
    static size_t CurlWrite_CallbackFunc_StdString(void *contents, size_t size,
                                                   size_t nmemb, std::deque<char> *s);
    static int CurlProgress_CallbackFunc(void *, double TotalToDownload,
                                         double NowDownloaded, double /*TotalToUpload*/,
                                         double /*NowUploaded*/);
    static std::string GetFileFromHTTPS(const std::string &url, bool IncludeProgressBar);
};

#endif // CURLTOOLS_H

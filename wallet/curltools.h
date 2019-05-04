#ifndef CURLTOOLS_H
#define CURLTOOLS_H

#include "util.h"

#include <atomic>
#include <boost/filesystem/fstream.hpp>
#include <curl/curl.h>

class cURLTools
{
public:
    static size_t CurlWrite_CallbackFunc_StdString(void* contents, size_t size, size_t nmemb,
                                                   std::deque<char>* s);
    static size_t CurlRead_CallbackFunc_StdString(void* dest, size_t size, size_t nmemb, void* userp);
    static int    CurlProgress_CallbackFunc(void*, double TotalToDownload, double NowDownloaded,
                                            double /*TotalToUpload*/, double /*NowUploaded*/);
    static void   CurlGlobalInit_ThreadSafe();
    static std::string GetFileFromHTTPS(const std::string& URL, long ConnectionTimeout,
                                        bool IncludeProgressBar);
    static std::string PostJsonToHTTPS(const std::string& URL, long ConnectionTimeout,
                                       const std::string& data, bool chunked);
    static void        GetLargeFileFromHTTPS(const std::string& URL, long ConnectionTimeout,
                                             const boost::filesystem::path& targetPath,
                                             std::atomic<float>&            progress);
    static int         CurlAtomicProgress_CallbackFunc(void* number, double TotalToDownload,
                                                       double NowDownloaded, double, double);
    static size_t      CurlWrite_CallbackFunc_File(void* contents, size_t size, size_t nmemb,
                                                   boost::filesystem::fstream* fs);
};

#endif // CURLTOOLS_H

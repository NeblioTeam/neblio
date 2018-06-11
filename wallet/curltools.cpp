#include "curltools.h"

#include <iostream>
#include <openssl/ssl.h>
#include <boost/thread.hpp>

boost::once_flag init_openssl_once_flag = BOOST_ONCE_INIT;
boost::once_flag init_curl_global_once_flag = BOOST_ONCE_INIT;
boost::mutex curl_global_init_lock;

class CurlCleaner
{
    CURL* curl_obj_to_clean;
public:
    CurlCleaner(CURL* Curl_obj_to_clean) : curl_obj_to_clean(Curl_obj_to_clean)
    {}

    ~CurlCleaner() {
        curl_easy_cleanup(curl_obj_to_clean);
    }
};

size_t cURLTools::CurlWrite_CallbackFunc_StdString(void *contents, size_t size,
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

size_t cURLTools::CurlRead_CallbackFunc_StdString(void *dest, size_t /*size*/, size_t /*nmemb*/, void *userp)
{
    std::string *from = (std::string*)userp;
    std::string *to = (std::string*)dest;

    std::copy(from->begin(), from->end(), std::back_inserter(*to));

    return 0; /* no more data left to deliver */
}

int cURLTools::CurlProgress_CallbackFunc(void *, double TotalToDownload,
                                         double NowDownloaded, double /*TotalToUpload*/,
                                         double /*NowUploaded*/) {
    std::clog << "Download progress: " <<
                 ToString(NowDownloaded) << " / " <<
                 ToString(TotalToDownload) << std::endl;
    return CURLE_OK;
}

void cURLTools::CurlGlobalInit_ThreadSafe()
{
    boost::lock_guard<boost::mutex> lg(curl_global_init_lock);
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::string cURLTools::GetFileFromHTTPS(const std::string &URL, long ConnectionTimeout, bool IncludeProgressBar) {

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    boost::call_once(init_openssl_once_flag, SSL_library_init);
#else
    boost::call_once(init_openssl_once_flag, OPENSSL_init_ssl, 0, static_cast<const ossl_init_settings_st*>(NULL));
#endif

    CURL *curl;
    CURLcode res;

    boost::call_once(init_curl_global_once_flag, CurlGlobalInit_ThreadSafe);

    curl = curl_easy_init();
    std::deque<char> s;
    if (curl) {

        CurlCleaner cleaner(curl);

        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // verify ssl peer
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // verify ssl hostname
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
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
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, ConnectionTimeout);

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
                throw std::runtime_error("Error retrieving data with https protocol, error code: " + ToString(http_response_code) +
                                         ". Probably the URL is invalid.");
            }
        }

        /* always cleanup */
        // This is replaced by a smart cleaning object with the destructor (CurlCleaner)
        // curl_easy_cleanup(curl);
    }
    std::string fileStr(s.begin(), s.end());
    return fileStr;
}

void cURLTools::PostDataToHTTPS(const std::string &URL, long ConnectionTimeout, const std::string &data, bool chunked)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    boost::call_once(init_openssl_once_flag, SSL_library_init);
#else
    boost::call_once(init_openssl_once_flag, OPENSSL_init_ssl, 0, static_cast<const ossl_init_settings_st*>(NULL));
#endif

    CURL *curl;
    CURLcode res;

    boost::call_once(init_curl_global_once_flag, CurlGlobalInit_ThreadSafe);

    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {

        CurlCleaner cleaner(curl);

        /* First set the URL that is about to receive our POST. This URL can
         just as well be a https:// URL if that is what should receive the
         data. */
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl, CURLOPT_READDATA, &data);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // verify ssl peer
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // verify ssl hostname
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, CurlRead_CallbackFunc_StdString);
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "name=daniel&project=curl");

        //        curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L); //verbose output
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, ConnectionTimeout);

        /*
          If you use POST to a HTTP 1.1 server, you can send data without knowing
          the size before starting the POST if you use chunked encoding. You
          enable this by adding a header like "Transfer-Encoding: chunked" with
          CURLOPT_HTTPHEADER. With HTTP 1.0 or without chunked transfer, you must
          specify the size in the request.
        */
        if(chunked) {
            struct curl_slist *chunk = NULL;

            chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            /* use curl_slist_free_all() after the *perform() call to free this
         list again */
        } else {
            /* Set the expected POST size. If you want to POST large amounts of data,
       consider CURLOPT_POSTFIELDSIZE_LARGE */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (curl_off_t)data.size());
        }

        /*
              Using POST with HTTP 1.1 implies the use of a "Expect: 100-continue"
              header.  You can disable this header with CURLOPT_HTTPHEADER as usual.
              NOTE: if you want chunked transfer too, you need to combine these two
              since you can only set one list of headers with CURLOPT_HTTPHEADER. */

        /* A less good option would be to enforce HTTP 1.0, but that might also
               have other implications. */
        const bool disable_expect = false;
        if(disable_expect)
        {
            struct curl_slist *chunk = NULL;

            chunk = curl_slist_append(chunk, "Expect:");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            /* use curl_slist_free_all() after the *perform() call to free this
                 list again */
        }

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
                throw std::runtime_error("Error posting data with https protocol, error code: " + ToString(http_response_code) +
                                         ". Probably the URL is invalid.");
            }
        }

        /* always cleanup */
        // This is replaced by a smart cleaning object with the destructor (CurlCleaner)
        // curl_easy_cleanup(curl);
    }
    return;
}

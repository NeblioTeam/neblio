#include "curltools.h"

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

int cURLTools::CurlProgress_CallbackFunc(void *, double TotalToDownload,
                              double NowDownloaded, double /*TotalToUpload*/,
                              double /*NowUploaded*/) {
  std::clog << "Download progress: " <<
               ToString(NowDownloaded) << " / " <<
               ToString(TotalToDownload) << std::endl;
  return CURLE_OK;
}

std::string cURLTools::GetFileFromHTTPS(const std::string &url, bool IncludeProgressBar) {
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

#include "stringmanip.h"

std::string PossiblyWideStringToString(const std::string& str) { return str; }

std::string PossiblyWideStringToString(const std::wstring& str)
{
    std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> cv;
    return cv.to_bytes(str);
}

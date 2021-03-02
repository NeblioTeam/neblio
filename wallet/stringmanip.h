#ifndef STRINGMANIP_H
#define STRINGMANIP_H

#include <codecvt> // std::codecvt_utf8
#include <string>
#include <locale>

/// These two functions convert a path to a string to be passed to lmdb, or any other legacy software
/// that takes only c_str as argument
std::string PossiblyWideStringToString(const std::string& str);
std::string PossiblyWideStringToString(const std::wstring& str);

#endif // STRINGMANIP_H

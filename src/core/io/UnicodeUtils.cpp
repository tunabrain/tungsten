#include "UnicodeUtils.hpp"

#if _WIN32
#include <windows.h>
#endif

namespace Tungsten {

namespace UnicodeUtils {

#if _WIN32
std::wstring utf8ToWchar(const char *s, size_t size)
{
    int wsize = MultiByteToWideChar(CP_UTF8, 0, s, int(size), nullptr, 0);
    std::wstring result(wsize, '\0');
    MultiByteToWideChar(CP_UTF8, 0, s, int(size), &result[0], wsize);
    return std::move(result);
}

std::string wcharToUtf8(const wchar_t *s, size_t wsize)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, s, int(wsize), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, int(wsize), &result[0], size, nullptr, nullptr);
    return std::move(result);
}

std::wstring utf8ToWchar(const std::string &s)
{
    return utf8ToWchar(s.c_str(), s.size());
}

std::string wcharToUtf8(const std::wstring &s)
{
    return wcharToUtf8(s.c_str(), s.size());
}

std::wstring utf8ToWchar(const char *s)
{
    // We want to count the number of bytes (not characters), so this is fine
    return utf8ToWchar(s, strlen(s));
}

std::string wcharToUtf8(const wchar_t *s)
{
    return wcharToUtf8(s, wcslen(s));
}
#endif

}

}

#ifndef UNICODEUTILS_HPP_
#define UNICODEUTILS_HPP_

#include <string>

namespace Tungsten {

namespace UnicodeUtils {

#if _WIN32
// Note 1: Ideally we could replace these with codecvt, but libstdc++ does not implement that.
// Boo!

// Note 2: By wchar, we don't really mean the C++ standard version of wchar (which can be anything
// capable of holding all codepoints of all supported locales), but the Microsoft interpretation
// of it (which is always UCS-2).

std::wstring utf8ToWchar(const char *s, size_t size);
std::string wcharToUtf8(const wchar_t *s, size_t size);

std::wstring utf8ToWchar(const std::string &s);
std::string wcharToUtf8(const std::wstring &s);

// Will compute string length automatically (possibly slow!)
std::wstring utf8ToWchar(const char *s);
std::string wcharToUtf8(const wchar_t *s);
#endif

}

}

#endif /* UNICODEUTILS_HPP_ */

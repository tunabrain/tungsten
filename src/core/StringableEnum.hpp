#ifndef STRINGABLEENUM_HPP_
#define STRINGABLEENUM_HPP_

#include "io/JsonValue.hpp"

#include "Debug.hpp"

#include <tinyformat/tinyformat.hpp>
#include <sstream>
#include <cstring>
#include <vector>

namespace Tungsten {

template<typename Enum>
class StringableEnum
{
    typedef std::pair<const char *, Enum> Entry;

    static const char *_name;
    static std::vector<Entry> _entries;

    Enum _t;

    bool fromString(const char *s)
    {
        for (const auto &i : _entries) {
            if (std::strcmp(i.first, s) == 0) {
                _t = i.second;
                return true;
            }
        }
        return false;
    }

    static std::string formatError(const char *source)
    {
        std::stringstream ss;
        ss << "Unknown " << _name << " name: \"" << source << "\". Available options are: ";
        for (size_t i = 0; i < _entries.size(); ++i) {
            if (i)
                ss << ", ";
            ss << _entries[i].first;
        }
        return ss.str();
    }

    static std::vector<Entry> initializer();

public:
    StringableEnum() = default;

    StringableEnum(JsonValue value)
    {
        const char *name = value.cast<const char *>();
        if (!fromString(name))
            value.parseError(formatError(name));
    }
    StringableEnum(const char *s)
    {
        if (!fromString(s))
            throw std::runtime_error(formatError(s));
    }
    StringableEnum(const std::string &s) : StringableEnum(s.c_str()) {}

    const char *toString() const
    {
        for (const auto &i : _entries)
            if (i.second == _t)
                return i.first;
        FAIL("StringifiedEnum has invalid value!");
    }

    operator Enum() const { return _t; }

    StringableEnum &operator=(JsonValue value)
    {
        if (value)
            *this = StringableEnum(value);
        return *this;
    }
};

template<typename T>
std::vector<typename StringableEnum<T>::Entry> StringableEnum<T>::_entries = StringableEnum<T>::initializer();

#define DECLARE_STRINGABLE_ENUM(TYPE, NAME, ENTRIES)          \
    template<> const char *TYPE::_name = NAME;                \
    template<> std::vector<TYPE::Entry> TYPE::initializer() { \
        return std::vector<TYPE::Entry>ENTRIES;               \
    }

}

#endif /* STRINGABLEENUM_HPP_ */

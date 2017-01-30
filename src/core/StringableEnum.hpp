#ifndef STRINGABLEENUM_HPP_
#define STRINGABLEENUM_HPP_

#include "io/JsonPtr.hpp"

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

    Enum _t;

    bool fromString(const char *s)
    {
        for (const auto &i : entries()) {
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
        for (size_t i = 0; i < entries().size(); ++i) {
            if (i)
                ss << ", ";
            ss << entries()[i].first;
        }
        return ss.str();
    }

public:
    StringableEnum() = default;

    StringableEnum(Enum t) : _t(t) {}

    StringableEnum(JsonPtr value)
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
        for (const auto &i : entries())
            if (i.second == _t)
                return i.first;
        FAIL("StringifiedEnum has invalid value!");
    }

    Enum &toEnum() { return _t; }

    operator Enum() const { return _t; }

    StringableEnum &operator=(JsonPtr value)
    {
        if (value)
            *this = StringableEnum(value);
        return *this;
    }

    static std::vector<Entry> &entries();
};

#define DEFINE_STRINGABLE_ENUM(TYPE, NAME, ENTRIES)           \
    template<> const char *TYPE::_name = NAME;                \
    template<> std::vector<TYPE::Entry> &TYPE::entries() {    \
        static std::vector<TYPE::Entry> entries ENTRIES;      \
        return entries;                                       \
    }

}

#endif /* STRINGABLEENUM_HPP_ */

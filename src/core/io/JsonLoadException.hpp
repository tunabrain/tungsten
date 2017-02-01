#ifndef JSONLOADEXCEPTION_HPP_
#define JSONLOADEXCEPTION_HPP_

#include "Path.hpp"

#include "Platform.hpp"

#include <exception>
#include <string>

namespace Tungsten {

class JsonLoadException : public std::exception
{
    std::string _description;
    std::string _excerpt;
    std::string _what;

public:
    JsonLoadException(const Path &path);
    JsonLoadException(const std::string &description, const std::string &excerpt);

    bool haveExcerpt() const { return !_excerpt.empty(); }
    const std::string &description() const { return _description; }
    const std::string &excerpt() const { return _excerpt; }

    virtual const char *what() const NOEXCEPT override { return _what.c_str(); }
};

}

#endif /* JSONLOADEXCEPTION_HPP_ */

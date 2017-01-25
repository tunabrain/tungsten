#include "JsonLoadException.hpp"

#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

JsonLoadException::JsonLoadException(const Path &path)
: _description(tfm::format("Unable to load file '%s'", path.fileName())),
  _what(_description)
{
}

JsonLoadException::JsonLoadException(const std::string &description, const std::string &excerpt)
: _description(description),
  _excerpt(excerpt),
  _what(tfm::format("%s\n\n%s", _description, _excerpt))
{
}

}

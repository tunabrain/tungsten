#ifndef JSONDOCUMENT_HPP_
#define JSONDOCUMENT_HPP_

#include "JsonPtr.hpp"
#include "Path.hpp"

#include "Platform.hpp"

#include <rapidjson/document.h>
#include <string>

namespace Tungsten {

class JsonDocument : public JsonPtr
{
    Path _file;
    rapidjson::Document _document;
    std::string _json;

    void load();

public:
    JsonDocument(const Path &file);
    JsonDocument(const Path &file, std::string json);

    NORETURN void parseError(JsonPtr source, std::string description) const;
};

}

#endif /* SRC_CORE_IO_JSONDOCUMENT_HPP_ */

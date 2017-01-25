#ifndef JSONDOCUMENT_HPP_
#define JSONDOCUMENT_HPP_

#include "Path.hpp"

#include <rapidjson/document.h>
#include <functional>
#include <string>

namespace Tungsten {

class JsonDocument
{
    void load(const Path &file, std::string json, std::function<void(const rapidjson::Document &)> loader);

public:
    JsonDocument(const Path &file, std::function<void(const rapidjson::Document &)> loader);
    JsonDocument(const Path &file, std::string json, std::function<void(const rapidjson::Document &)> loader);

    void setError(const rapidjson::Value &source, std::string error);
};

}

#endif /* SRC_CORE_IO_JSONDOCUMENT_HPP_ */

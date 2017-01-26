#ifndef JSONDOCUMENT_HPP_
#define JSONDOCUMENT_HPP_

#include "JsonValue.hpp"
#include "Path.hpp"

#include <functional>
#include <string>

namespace Tungsten {

class JsonDocument
{
    void load(const Path &file, std::string json, std::function<void(JsonValue)> loader);

public:
    JsonDocument(const Path &file, std::function<void(JsonValue)> loader);
    JsonDocument(const Path &file, std::string json, std::function<void(JsonValue)> loader);

    //void setError(const rapidjson::Value &source, std::string error);

    bool fromJson();
};

}

#endif /* SRC_CORE_IO_JSONDOCUMENT_HPP_ */

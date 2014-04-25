#include "JsonSerializable.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

void JsonSerializable::fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
{
    JsonUtils::fromJson(v, "name", _name);
}

}

#include "JsonSerializable.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

JsonSerializable::JsonSerializable(const std::string &name)
: _name(name)
{
}

void JsonSerializable::fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
{
    JsonUtils::fromJson(v, "name", _name);
}

rapidjson::Value JsonSerializable::toJson(Allocator &allocator) const
{
    rapidjson::Value v(rapidjson::kObjectType);
    if (!unnamed())
        v.AddMember("name", _name.c_str(), allocator);
    return std::move(v);
}

}

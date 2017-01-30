#include "JsonSerializable.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

JsonSerializable::JsonSerializable(const std::string &name)
: _name(name)
{
}

void JsonSerializable::fromJson(JsonPtr value, const Scene &/*scene*/)
{
    value.getField("name", _name);
}

rapidjson::Value JsonSerializable::toJson(Allocator &allocator) const
{
    rapidjson::Value v(rapidjson::kObjectType);
    if (!unnamed())
        v.AddMember("name", JsonUtils::toJson(_name, allocator), allocator);
    return std::move(v);
}

}

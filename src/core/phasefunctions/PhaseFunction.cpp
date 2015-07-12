#include "PhaseFunction.hpp"

namespace Tungsten {

void PhaseFunction::fromJson(const rapidjson::Value &/*v*/, const Scene &/*scene*/)
{
}

rapidjson::Value PhaseFunction::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));
    return std::move(v);
}

}

#include "PhaseFunction.hpp"

namespace Tungsten {

void PhaseFunction::fromJson(JsonValue /*value*/, const Scene &/*scene*/)
{
}

rapidjson::Value PhaseFunction::toJson(Allocator &allocator) const
{
    return JsonSerializable::toJson(allocator);
}

}

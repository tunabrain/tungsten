#include "PhaseFunction.hpp"

#include "Debug.hpp"

namespace Tungsten {

void PhaseFunction::fromJson(JsonPtr /*value*/, const Scene &/*scene*/)
{
}

rapidjson::Value PhaseFunction::toJson(Allocator &allocator) const
{
    return JsonSerializable::toJson(allocator);
}

bool PhaseFunction::invert(WritablePathSampleGenerator &/*sampler*/, const Vec3f &/*wi*/, const Vec3f &/*wo*/) const
{
    FAIL("PhaseFunction::invert not implemented!");
}

}

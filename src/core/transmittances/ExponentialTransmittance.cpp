#include "ExponentialTransmittance.hpp"

#include "sampling/UniformPathSampler.hpp"

#include "math/MathUtil.hpp"
#include "math/FastMath.hpp"

#include "io/JsonObject.hpp"

#include "Memory.hpp"

namespace Tungsten {

void ExponentialTransmittance::fromJson(JsonPtr value, const Scene &scene)
{
    Transmittance::fromJson(value, scene);
}

rapidjson::Value ExponentialTransmittance::toJson(Allocator &allocator) const
{
    return JsonObject{Transmittance::toJson(allocator), allocator,
        "type", "exponential",
    };
}

Vec3f ExponentialTransmittance::surfaceSurface(const Vec3f &tau) const
{
    return FastMath::exp(-tau);
}
Vec3f ExponentialTransmittance::surfaceMedium(const Vec3f &tau) const
{
    return FastMath::exp(-tau);
}
Vec3f ExponentialTransmittance::mediumSurface(const Vec3f &tau) const
{
    return FastMath::exp(-tau);
}
Vec3f ExponentialTransmittance::mediumMedium(const Vec3f &tau) const
{
    return FastMath::exp(-tau);
}

float ExponentialTransmittance::sigmaBar() const
{
    return 1.0f;
}

float ExponentialTransmittance::sampleSurface(PathSampleGenerator &sampler) const
{
    return -std::log(1.0f - sampler.next1D());
}
float ExponentialTransmittance::sampleMedium(PathSampleGenerator &sampler) const
{
    return -std::log(1.0f - sampler.next1D());
}

}

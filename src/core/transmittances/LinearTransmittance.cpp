#include "LinearTransmittance.hpp"

#include "sampling/UniformPathSampler.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonObject.hpp"

#include "Memory.hpp"

namespace Tungsten {

LinearTransmittance::LinearTransmittance()
: _maxT(1.0f)
{
}

void LinearTransmittance::fromJson(JsonPtr value, const Scene &scene)
{
    Transmittance::fromJson(value, scene);
    value.getField("max_t", _maxT);
}

rapidjson::Value LinearTransmittance::toJson(Allocator &allocator) const
{
    return JsonObject{Transmittance::toJson(allocator), allocator,
        "type", "linear",
        "max_t", _maxT
    };
}

Vec3f LinearTransmittance::surfaceSurface(const Vec3f &tau) const
{
    return 1.0f - min(tau/_maxT, Vec3f(1.0f));
}
Vec3f LinearTransmittance::surfaceMedium(const Vec3f &tau) const
{
    Vec3f result(1.0f/_maxT);
    for (int i = 0; i < 3; ++i)
        if (tau[i] > _maxT)
            result[i] = 0.0f;
    return result;
}
Vec3f LinearTransmittance::mediumSurface(const Vec3f &tau) const
{
    Vec3f result(1.0f);
    for (int i = 0; i < 3; ++i)
        if (tau[i] > _maxT)
            result[i] = 0.0f;
    return result;
}
Vec3f LinearTransmittance::mediumMedium(const Vec3f &tau) const
{
    Vec3f result;
    for (int i = 0; i < 3; ++i)
        result[i] = std::abs(tau[i] - _maxT) < 1e-3f ? 1.0f : 0.0f;
    return result;
}

float LinearTransmittance::sigmaBar() const
{
    return 1.0f/_maxT;
}

bool LinearTransmittance::isDirac() const
{
    return true;
}

float LinearTransmittance::sampleSurface(PathSampleGenerator &sampler) const
{
    return _maxT*sampler.next1D();
}
float LinearTransmittance::sampleMedium(PathSampleGenerator &/*sampler*/) const
{
    return _maxT;
}

}

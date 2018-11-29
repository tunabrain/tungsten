#include "DoubleExponentialTransmittance.hpp"

#include "sampling/UniformPathSampler.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonObject.hpp"

#include "Memory.hpp"

namespace Tungsten {

DoubleExponentialTransmittance::DoubleExponentialTransmittance()
: _sigmaA(0.5f),
  _sigmaB(10.0f)
{
}

void DoubleExponentialTransmittance::fromJson(JsonPtr value, const Scene &scene)
{
    Transmittance::fromJson(value, scene);
    value.getField("sigma_a", _sigmaA);
    value.getField("sigma_b", _sigmaB);
}

rapidjson::Value DoubleExponentialTransmittance::toJson(Allocator &allocator) const
{
    return JsonObject{Transmittance::toJson(allocator), allocator,
        "type", "double_exponential",
        "sigma_a", _sigmaA,
        "sigma_b", _sigmaB,
    };
}

Vec3f DoubleExponentialTransmittance::surfaceSurface(const Vec3f &tau) const
{
    return 0.5f*(std::exp(-_sigmaA*tau) + std::exp(-_sigmaB*tau));
}
Vec3f DoubleExponentialTransmittance::surfaceMedium(const Vec3f &tau) const
{
    return 0.5f*(_sigmaA*std::exp(-_sigmaA*tau) + _sigmaB*std::exp(-_sigmaB*tau));
}
Vec3f DoubleExponentialTransmittance::mediumSurface(const Vec3f &tau) const
{
    return (_sigmaA*std::exp(-_sigmaA*tau) + _sigmaB*std::exp(-_sigmaB*tau))/(_sigmaA + _sigmaB);
}
Vec3f DoubleExponentialTransmittance::mediumMedium(const Vec3f &tau) const
{
    return (sqr(_sigmaA)*std::exp(-_sigmaA*tau) + sqr(_sigmaB)*std::exp(-_sigmaB*tau))/(_sigmaA + _sigmaB);
}

float DoubleExponentialTransmittance::sigmaBar() const
{
    return 0.5f*(_sigmaA + _sigmaB);
}

float DoubleExponentialTransmittance::sampleSurface(PathSampleGenerator &sampler) const
{
    float t = -std::log(1.0f - sampler.next1D());
    return sampler.nextBoolean(0.5f) ? t/_sigmaA : t/_sigmaB;
}
float DoubleExponentialTransmittance::sampleMedium(PathSampleGenerator &sampler) const
{
    float t = -std::log(1.0f - sampler.next1D());
    return sampler.nextBoolean(_sigmaA/(_sigmaA + _sigmaB)) ? t/_sigmaA : t/_sigmaB;
}

}

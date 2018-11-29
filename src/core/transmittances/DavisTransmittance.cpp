#include "DavisTransmittance.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

DavisTransmittance::DavisTransmittance()
: _alpha(1.1f)
{
}

void DavisTransmittance::fromJson(JsonPtr value, const Scene &scene)
{
    Transmittance::fromJson(value, scene);
    value.getField("alpha", _alpha);

    if (_alpha < 1 + 1e-6f) {
        std::cout << "Warning: alpha parameter of Davis transmittance has to be > 1. Clamping the current value (" << _alpha << ")" << std::endl;
        _alpha = 1 + 1e-6f;
    }
}

rapidjson::Value DavisTransmittance::toJson(Allocator &allocator) const
{
    return JsonObject{Transmittance::toJson(allocator), allocator,
        "type", "davis",
        "alpha", _alpha
    };
}

Vec3f DavisTransmittance::surfaceSurface(const Vec3f &tau) const
{
    return std::pow(1.0f + tau/_alpha, -_alpha);
}
Vec3f DavisTransmittance::surfaceMedium(const Vec3f &tau) const
{
    return std::pow(1.0f + tau/_alpha, -(_alpha + 1.0f));
}
Vec3f DavisTransmittance::mediumSurface(const Vec3f &tau) const
{
    return surfaceMedium(tau);
}
Vec3f DavisTransmittance::mediumMedium(const Vec3f &tau) const
{
    return (1.0f + 1.0f/_alpha)*std::pow(1.0f + tau/_alpha, -(_alpha + 2.0f));
}

float DavisTransmittance::sigmaBar() const
{
    return 1.0f;
}

float DavisTransmittance::sampleSurface(PathSampleGenerator &sampler) const
{
    return _alpha*(std::pow(1.0f - sampler.next1D(), -1.0f/_alpha) - 1.0f);
}
float DavisTransmittance::sampleMedium(PathSampleGenerator &sampler) const
{
    return _alpha*(std::pow(1.0f - sampler.next1D(), -1.0f/(1.0f + _alpha)) - 1.0f);
}

}

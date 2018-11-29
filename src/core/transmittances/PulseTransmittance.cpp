#include "PulseTransmittance.hpp"

#include "sampling/UniformPathSampler.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonObject.hpp"
#include "io/FileUtils.hpp"

#include "Memory.hpp"

namespace Tungsten {

PulseTransmittance::PulseTransmittance()
: _a(0.0f),
  _b(1.0f),
  _numPulses(4)
{
}

void PulseTransmittance::fromJson(JsonPtr value, const Scene &scene)
{
    Transmittance::fromJson(value, scene);
    value.getField("min", _a);
    value.getField("max", _b);
    value.getField("num_pulses", _numPulses);
}

rapidjson::Value PulseTransmittance::toJson(Allocator &allocator) const
{
    return JsonObject{Transmittance::toJson(allocator), allocator,
        "type", "pulse",
        "min", _a,
        "max", _b,
        "num_pulses", _numPulses,
    };
}

bool PulseTransmittance::isDirac() const
{
    return true;
}

Vec3f PulseTransmittance::surfaceSurface(const Vec3f &tau) const
{
    Vec3f idxF = clamp(float(_numPulses)*(tau - _a)/(_b - _a) + 0.5f, Vec3f(0.0f), Vec3f(_numPulses));
    Vec3i idx = Vec3i(idxF);

    Vec3f height = Vec3f(_numPulses - idx)/float(_numPulses);

    Vec3f cellIntegral = height*(idxF - Vec3f(idx));
    for (int i = 0; i < 3; ++i) {
        if (idx[i] > 0)
            cellIntegral[i] += (idx[i] - 0.5f) - (idx[i]*(idx[i] - 1))/(2.0f*_numPulses);
        else
            cellIntegral[i] -= 0.5f;
    }

    return 1.0f - (2.0f/_numPulses)*cellIntegral;
}
Vec3f PulseTransmittance::surfaceMedium(const Vec3f &tau) const
{
    return 2.0f/(_b - _a)*mediumSurface(tau);
}
Vec3f PulseTransmittance::mediumSurface(const Vec3f &tau) const
{
    Vec3i idx = clamp(Vec3i(float(_numPulses)*(tau - _a)/(_b - _a) + 0.5f), Vec3i(0), Vec3i(_numPulses));
    return 1.0f - Vec3f(idx)/float(_numPulses);
}
Vec3f PulseTransmittance::mediumMedium(const Vec3f &tau) const
{
    Vec3f idxF = clamp(float(_numPulses)*(tau - _a)/(_b - _a), Vec3f(0.0f), Vec3f(_numPulses));
    Vec3i idx = Vec3i(idxF);
    return (1.0f/_numPulses)*Vec3f(
        std::abs(idxF[0] - idx[0] - 0.5f) < 1e-3f ? 1.0f : 0.0f,
        std::abs(idxF[1] - idx[1] - 0.5f) < 1e-3f ? 1.0f : 0.0f,
        std::abs(idxF[2] - idx[2] - 0.5f) < 1e-3f ? 1.0f : 0.0f
    );
}

float PulseTransmittance::sigmaBar() const
{
    return 2.0f/(_b - _a);
}

float PulseTransmittance::sampleSurface(PathSampleGenerator &sampler) const
{
    float xi = sampler.next1D()*_numPulses*0.5f;
    float delta = 1.0f/_numPulses;

    for (int i = 0; i < _numPulses; ++i) {
        float h0 = 1.0f - (i + 0.0f)*delta;
        float h1 = 1.0f - (i + 1.0f)*delta;
        xi -= h0*0.5f;
        if (xi < 0.0f)
            return _a + (i + 0.0f + 0.5f*sampler.next1D())*(_b - _a)*delta;
        xi -= h1*0.5f;
        if (xi < 0.0f)
            return _a + (i + 0.5f + 0.5f*sampler.next1D())*(_b - _a)*delta;
    }
    return 0.0f;
}
float PulseTransmittance::sampleMedium(PathSampleGenerator &sampler) const
{
    return _a + (0.5f + int(sampler.next1D()*_numPulses))/float(_numPulses)*(_b - _a);
}

}

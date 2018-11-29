#include "InterpolatedTransmittance.hpp"

#include "ExponentialTransmittance.hpp"
#include "LinearTransmittance.hpp"
#include "ErlangTransmittance.hpp"

#include "sampling/UniformPathSampler.hpp"

#include "math/MathUtil.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

#include "Memory.hpp"

namespace Tungsten {

InterpolatedTransmittance::InterpolatedTransmittance()
: _trA(std::make_shared<LinearTransmittance>()),
  _trB(std::make_shared<ErlangTransmittance>()),
  _u(0.5f)
{
}

void InterpolatedTransmittance::fromJson(JsonPtr value, const Scene &scene)
{
    Transmittance::fromJson(value, scene);
    if (auto m = value["tr_a"]) _trA = scene.fetchTransmittance(m);
    if (auto m = value["tr_b"]) _trB = scene.fetchTransmittance(m);
    value.getField("ratio", _u);
}

rapidjson::Value InterpolatedTransmittance::toJson(Allocator &allocator) const
{
    return JsonObject{Transmittance::toJson(allocator), allocator,
        "type", "interpolated",
        "tr_a", _trA->toJson(allocator),
        "tr_b", _trB->toJson(allocator),
        "ratio", _u
    };
}

Vec3f InterpolatedTransmittance::surfaceSurface(const Vec3f &tau) const
{
    return sigmaBar()*lerp(_trA->surfaceSurface(tau)/_trA->sigmaBar(), _trB->surfaceSurface(tau)/_trB->sigmaBar(), _u);
}
Vec3f InterpolatedTransmittance::surfaceMedium(const Vec3f &tau) const
{
    return mediumSurface(tau)*sigmaBar();
}
Vec3f InterpolatedTransmittance::mediumSurface(const Vec3f &tau) const
{
    return lerp(_trA->mediumSurface(tau), _trB->mediumSurface(tau), _u);
}
Vec3f InterpolatedTransmittance::mediumMedium(const Vec3f &tau) const
{
    Vec3f pdfA = _trA->mediumMedium(tau);
    Vec3f pdfB = _trB->mediumMedium(tau);
    Vec3f result;
    for (int i = 0; i < 3; ++i) {
        bool diracA = _trA->isDirac() && pdfA[i] > 0.0f;
        bool diracB = _trB->isDirac() && pdfB[i] > 0.0f;
        if (diracA ^ diracB)
            result[i] = diracA ? pdfA[i] : pdfB[i];
        else
            result[i] = lerp(pdfA[i], pdfB[i], _u);
    }
    return result;
}

float InterpolatedTransmittance::sigmaBar() const
{
    return 1.0f/lerp(1.0f/_trA->sigmaBar(), 1.0f/_trB->sigmaBar(), _u);
}

float InterpolatedTransmittance::sampleSurface(PathSampleGenerator &sampler) const
{
    return sampler.nextBoolean(_u) ? _trB->sampleSurface(sampler) : _trA->sampleSurface(sampler);
}
float InterpolatedTransmittance::sampleMedium(PathSampleGenerator &sampler) const
{
    return sampler.nextBoolean(_u) ? _trB->sampleMedium(sampler) : _trA->sampleMedium(sampler);
}

}

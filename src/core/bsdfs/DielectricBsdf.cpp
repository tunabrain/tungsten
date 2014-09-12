#include "DielectricBsdf.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

DielectricBsdf::DielectricBsdf()
: _ior(1.5f)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::SpecularTransmissionLobe);
}

DielectricBsdf::DielectricBsdf(float ior)
: _ior(ior)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::SpecularTransmissionLobe);
}

void DielectricBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
}

rapidjson::Value DielectricBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "dielectric", allocator);
    v.AddMember("ior", _ior, allocator);
    return std::move(v);
}

bool DielectricBsdf::sample(SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::SpecularTransmissionLobe);

    float eta = event.wi.z() < 0.0f ? _ior : 1.0f/_ior;

    float cosThetaT = 0.0f;
    float F = Fresnel::dielectricReflectance(eta, std::abs(event.wi.z()), cosThetaT);
    if (sampleR && !sampleT)
        F = 1.0f;
    else if (sampleT && !sampleR && F < 1.0f)
        F = 0.0f;
    else if (!sampleT && !sampleR)
        return false;
    if (event.sampler->next1D() < F) {
        event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
        event.pdf = F;
        event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
        event.throughput = Vec3f(1.0f);
    } else {
        event.wo = Vec3f(-event.wi.x()*eta, -event.wi.y()*eta, -std::copysign(cosThetaT, event.wi.z()));
        event.pdf = 1.0f - F;
        event.sampledLobe = BsdfLobes::SpecularTransmissionLobe;
        event.throughput = albedo(event.info);
    }
    return true;
}

Vec3f DielectricBsdf::eval(const SurfaceScatterEvent &/*event*/) const
{
    return Vec3f(0.0f);
}

float DielectricBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}

#include "ThinSheetBsdf.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "materials/ConstantTexture.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

ThinSheetBsdf::ThinSheetBsdf()
: _ior(1.5f),
  _enableInterference(false),
  _thickness(std::make_shared<ConstantTexture>(0.5f)),
  _sigmaA(0.0f)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::ForwardLobe);
}

void ThinSheetBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "enable_interference", _enableInterference);
    JsonUtils::fromJson(v, "sigmaA", _sigmaA);
    scene.textureFromJsonMember(v, "thickness", TexelConversion::REQUEST_AVERAGE, _thickness);
}

rapidjson::Value ThinSheetBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "thinsheet", allocator);
    v.AddMember("ior", _ior, allocator);
    v.AddMember("enable_interference", _enableInterference, allocator);
    JsonUtils::addObjectMember(v, "thickness", *_thickness, allocator);
    v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    return std::move(v);
}

bool ThinSheetBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
        return false;

    event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
    event.pdf = 1.0f;
    event.sampledLobe = BsdfLobes::SpecularReflectionLobe;

    if (_sigmaA == 0.0f && !_enableInterference) {
        // Fast path / early out
        event.throughput = Vec3f(1.0f);
        return true;
    }

    float thickness = (*_thickness)[event.info->uv].x();

    float cosThetaT;
    if (_enableInterference) {
        event.throughput = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                std::abs(event.wi.z()), thickness*500.0f, cosThetaT);
    } else {
        event.throughput = Vec3f(Fresnel::thinFilmReflectance(1.0f/_ior,
                std::abs(event.wi.z()), cosThetaT));
    }

    Vec3f transmittance = 1.0f - event.throughput;
    if (_sigmaA != 0.0f && cosThetaT > 0.0f)
        transmittance *= std::exp(-_sigmaA*(thickness*2.0f/cosThetaT));

    event.throughput /= 1.0f - transmittance.avg();

    return true;
}

Vec3f ThinSheetBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.isForward() || -event.wi != event.wo)
        return Vec3f(0.0f);

    float thickness = (*_thickness)[event.info->uv].x();

    float cosThetaT;
    Vec3f transmittance;
    if (_enableInterference) {
        transmittance = 1.0f - Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                std::abs(event.wi.z()), thickness*500.0f, cosThetaT);
    } else {
        transmittance = Vec3f(1.0f - Fresnel::thinFilmReflectance(1.0f/_ior, std::abs(event.wi.z()), cosThetaT));
    }

    if (_sigmaA != 0.0f && cosThetaT > 0.0f)
        transmittance *= std::exp(-_sigmaA*(thickness*2.0f/cosThetaT));

    return transmittance;
}

float ThinSheetBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}

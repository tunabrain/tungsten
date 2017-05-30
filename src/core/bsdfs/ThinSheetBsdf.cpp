#include "ThinSheetBsdf.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "textures/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

ThinSheetBsdf::ThinSheetBsdf()
: _ior(1.5f),
  _enableInterference(false),
  _thickness(std::make_shared<ConstantTexture>(0.5f)),
  _sigmaA(0.0f)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::ForwardLobe);
}

void ThinSheetBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("ior", _ior);
    value.getField("enable_interference", _enableInterference);
    value.getField("sigma_a", _sigmaA);
    if (auto thickness = value["thickness"])
        _thickness = scene.fetchTexture(thickness, TexelConversion::REQUEST_AVERAGE);
}

rapidjson::Value ThinSheetBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "thinsheet",
        "ior", _ior,
        "enable_interference", _enableInterference,
        "thickness", *_thickness,
        "sigma_a", _sigmaA
    };
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
        event.weight = Vec3f(1.0f);
        return true;
    }

    float thickness = (*_thickness)[*event.info].x();

    float cosThetaT;
    if (_enableInterference) {
        event.weight = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                std::abs(event.wi.z()), thickness*500.0f, cosThetaT);
    } else {
        event.weight = Vec3f(Fresnel::thinFilmReflectance(1.0f/_ior,
                std::abs(event.wi.z()), cosThetaT));
    }

    Vec3f transmittance = 1.0f - event.weight;
    if (_sigmaA != 0.0f && cosThetaT > 0.0f)
        transmittance *= std::exp(-_sigmaA*(thickness*2.0f/cosThetaT));

    event.weight /= 1.0f - transmittance.avg();

    return true;
}

Vec3f ThinSheetBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.isForward() || -event.wi != event.wo)
        return Vec3f(0.0f);

    float thickness = (*_thickness)[*event.info].x();

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

bool ThinSheetBsdf::invert(WritablePathSampleGenerator &/*sampler*/, const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    return sampleR && checkReflectionConstraint(event.wi, event.wo);
}

float ThinSheetBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    if (sampleR && checkReflectionConstraint(event.wi, event.wo))
        return 1.0f;
    else
        return 0.0f;
}

}

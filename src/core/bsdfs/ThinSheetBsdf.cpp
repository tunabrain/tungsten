#include "ThinSheetBsdf.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

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
  _thickness(std::make_shared<ConstantTextureA>(0.5f)),
  _sigmaA(0.0f)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
}

void ThinSheetBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "ior", _ior);
    JsonUtils::fromJson(v, "sigmaA", _sigmaA);

    const rapidjson::Value::Member *thickness = v.FindMember("thickness");
    if (thickness)
        _thickness = scene.fetchScalarTexture<2>(thickness->value);
}

rapidjson::Value ThinSheetBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "thinsheet", allocator);
    v.AddMember("ior", _ior, allocator);
    JsonUtils::addObjectMember(v, "thickness", *_thickness, allocator);
    v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
    return std::move(v);
}

float ThinSheetBsdf::alpha(const IntersectionInfo *info) const
{
    if (!UseAlphaTrick) {
        return 1.0f;
    } else {
        float thickness = (*_thickness)[info->uv]*500.0f;
        float R, cosThetaT;
        Vec3f interference = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                std::abs(info->w.dot(info->Ns)), thickness, R, cosThetaT);
        return interference.avg();
//          float cosThetaT;
//          return Fresnel::thinFilmReflectance(1.0f/_ior, std::abs(info->w.dot(info->Ns)), cosThetaT);
    }
}

Vec3f ThinSheetBsdf::transmittance(const IntersectionInfo *info) const
{
//      return Vec3f(1.0f);
//      float thickness = (*_thickness)[info->uv];
//      if (_sigmaA == 0.0f || thickness == 0.0f)
//          return Vec3f(1.0f);
//
//      float cosThetaT;
//      float R = Fresnel::dielectricReflectance(1.0f/_ior, std::abs(info->w.dot(info->Ns)), cosThetaT);
//
//      Vec3f absorption = std::exp(-_sigmaA*thickness/cosThetaT);
//      Vec3f interference = Fresnel::thinFilmReflectance(1.0f/_ior, std::abs(info->w.dot(info->Ns)), thickness);
//
//      return absorption*(1.0f - R)*(1.0f - R)/(1.0f - absorption*R*R);
//      return Vec3f(1.0f);
//      if (/*_sigmaA == 0.0f || */thickness == 0.0f)
//          return Vec3f(1.0f);
//
    float thickness = (*_thickness)[info->uv]*500.0f;
    float R, cosThetaT;
    Vec3f interference = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
            std::abs(info->w.dot(info->Ns)), thickness, R, cosThetaT);
    return (1.0f - interference)/(1.0f - interference.avg());
//
//      return ((1.0f - interference)*(1.0f + R))/((1.0f + interference)*(1.0f - R));

    //Vec3f absorption = std::exp(-_sigmaA*thickness/cosThetaT);

    //return absorption*(1.0f - R)*(1.0f - R)/(1.0f - absorption*R*R);
}

bool ThinSheetBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
        return false;
    if (!UseAlphaTrick) {
        float cosThetaT;
        float R = Fresnel::thinFilmReflectance(1.0f/_ior, std::abs(event.wi.z()), cosThetaT);

        if (event.sampler->next1D() < R) {
            event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
            event.pdf = R;
            event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
        } else {
            event.wo = Vec3f(-event.wi.x(), -event.wi.y(), -event.wi.z());
            event.pdf = 1.0f - R;
            event.sampledLobe = BsdfLobes::SpecularTransmissionLobe;
        }
        event.throughput = Vec3f(1.0f);
    } else {
        event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
        event.pdf = 1.0f;
        event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
//          event.throughput = Vec3f(1.0f);
        float R, cosThetaT;
        event.throughput = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                std::abs(event.wi.z()), (*_thickness)[event.info->uv]*500.0f, R, cosThetaT);
        event.throughput /= event.throughput.avg();
    }
    return true;
}

Vec3f ThinSheetBsdf::eval(const SurfaceScatterEvent &/*event*/) const
{
    return Vec3f(0.0f);
}

float ThinSheetBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}

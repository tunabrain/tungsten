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
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe | BsdfLobes::ForwardLobe);
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

bool ThinSheetBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
        return false;
        event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
        event.pdf = 1.0f;
        event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
//          event.throughput = Vec3f(1.0f);
        float R, cosThetaT;
        event.throughput = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
                std::abs(event.wi.z()), (*_thickness)[event.info->uv]*500.0f, R, cosThetaT);
        event.throughput /= event.throughput.avg();
    return true;
}

Vec3f ThinSheetBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.isForward() || -event.wi != event.wo)
        return Vec3f(0.0f);

    float thickness = (*_thickness)[event.info->uv]*500.0f;
    float R, cosThetaT;
    Vec3f interference = Fresnel::thinFilmReflectanceInterference(1.0f/_ior,
            std::abs(event.wi.z()), thickness, R, cosThetaT);

    return 1.0f - interference;
}

float ThinSheetBsdf::pdf(const SurfaceScatterEvent &/*event*/) const
{
    return 0.0f;
}

}

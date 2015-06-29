#include "ConductorBsdf.hpp"
#include "ComplexIor.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

ConductorBsdf::ConductorBsdf()
: _materialName("Cu"),
  _eta(0.200438f, 0.924033f, 1.10221f),
  _k(3.91295f, 2.45285f, 2.14219f)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
}

void ConductorBsdf::lookupMaterial()
{
    if (!ComplexIorList::lookup(_materialName, _eta, _k)) {
        DBG("Warning: Unable to find material with name '%s'. Using default", _materialName.c_str());
        ComplexIorList::lookup("Cu", _eta, _k);
    }
}

void ConductorBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    if (JsonUtils::fromJson(v, "eta", _eta) && JsonUtils::fromJson(v, "k", _k))
        _materialName.clear();
    if (JsonUtils::fromJson(v, "material", _materialName))
        lookupMaterial();
}

rapidjson::Value ConductorBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "conductor", allocator);
    if (_materialName.empty()) {
        v.AddMember("eta", JsonUtils::toJsonValue(_eta, allocator), allocator);
        v.AddMember("k", JsonUtils::toJsonValue(_k, allocator), allocator);
    } else {
        v.AddMember("material", _materialName.c_str(), allocator);
    }
    return std::move(v);
}

bool ConductorBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe))
        return false;

    event.wo = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z());
    event.pdf = 1.0f;
    event.weight = albedo(event.info)*Fresnel::conductorReflectance(_eta, _k, event.wi.z());
    event.sampledLobe = BsdfLobes::SpecularReflectionLobe;
    return true;
}

Vec3f ConductorBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool evalR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    if (evalR && checkReflectionConstraint(event.wi, event.wo))
        return albedo(event.info)*Fresnel::conductorReflectance(_eta, _k, event.wi.z());
    else
        return Vec3f(0.0f);
}

float ConductorBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::SpecularReflectionLobe);
    if (sampleR && checkReflectionConstraint(event.wi, event.wo))
        return 1.0f;
    else
        return 0.0f;
}

}

#include "ConductorBsdf.hpp"
#include "ComplexIor.hpp"
#include "Fresnel.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"

#include <tinyformat/tinyformat.hpp>
#include <rapidjson/document.h>

namespace Tungsten {

ConductorBsdf::ConductorBsdf()
: _materialName("Cu"),
  _eta(0.200438f, 0.924033f, 1.10221f),
  _k(3.91295f, 2.45285f, 2.14219f)
{
    _lobes = BsdfLobes(BsdfLobes::SpecularReflectionLobe);
}

bool ConductorBsdf::lookupMaterial()
{
    return ComplexIorList::lookup(_materialName, _eta, _k);
}

void ConductorBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    if (value.getField("eta", _eta) && value.getField("k", _k))
        _materialName.clear();
    if (value.getField("material", _materialName) && !lookupMaterial())
        value.parseError(tfm::format("Unable to find material with name '%s'", _materialName));
}

rapidjson::Value ConductorBsdf::toJson(Allocator &allocator) const
{
    JsonObject result{Bsdf::toJson(allocator), allocator,
        "type", "conductor"
    };
    if (_materialName.empty())
        result.add("eta", _eta, "k", _k);
    else
        result.add("material", _materialName);

    return result;
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

bool ConductorBsdf::invert(WritablePathSampleGenerator &/*sampler*/, const SurfaceScatterEvent &event) const
{
    return checkReflectionConstraint(event.wi, event.wo);
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

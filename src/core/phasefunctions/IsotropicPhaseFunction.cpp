#include "IsotropicPhaseFunction.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

rapidjson::Value IsotropicPhaseFunction::toJson(Allocator &allocator) const
{
    return JsonObject{PhaseFunction::toJson(allocator), allocator,
        "type", "isotropic"
    };
}

Vec3f IsotropicPhaseFunction::eval(const Vec3f &/*wi*/, const Vec3f &/*wo*/) const
{
    return Vec3f(INV_FOUR_PI);
}

bool IsotropicPhaseFunction::sample(PathSampleGenerator &sampler, const Vec3f &/*wi*/, PhaseSample &sample) const
{
    sample.w = SampleWarp::uniformSphere(sampler.next2D());
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::uniformSpherePdf();
    return true;
}

bool IsotropicPhaseFunction::invert(WritablePathSampleGenerator &sampler, const Vec3f &/*wi*/, const Vec3f &wo) const
{
    sampler.put2D(SampleWarp::invertUniformSphere(wo, sampler.untracked1D()));
    return true;
}

float IsotropicPhaseFunction::pdf(const Vec3f &/*wi*/, const Vec3f &/*wo*/) const
{
    return SampleWarp::uniformSpherePdf();
}

}

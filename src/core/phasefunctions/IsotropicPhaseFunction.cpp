#include "IsotropicPhaseFunction.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

namespace Tungsten {

rapidjson::Value IsotropicPhaseFunction::toJson(Allocator &allocator) const
{
    rapidjson::Value v = PhaseFunction::toJson(allocator);
    v.AddMember("type", "isotropic", allocator);
    return std::move(v);
}

Vec3f IsotropicPhaseFunction::eval(const Vec3f &/*wi*/, const Vec3f &/*wo*/) const
{
    return Vec3f(INV_FOUR_PI);
}

bool IsotropicPhaseFunction::sample(PathSampleGenerator &sampler, const Vec3f &/*wi*/, PhaseSample &sample) const
{
    sample.w = SampleWarp::uniformSphere(sampler.next2D(MediumPhaseSample));
    sample.weight = Vec3f(1.0f);
    sample.pdf = SampleWarp::uniformSpherePdf();
    return true;
}

float IsotropicPhaseFunction::pdf(const Vec3f &/*wi*/, const Vec3f &/*wo*/) const
{
    return SampleWarp::uniformSpherePdf();
}

}

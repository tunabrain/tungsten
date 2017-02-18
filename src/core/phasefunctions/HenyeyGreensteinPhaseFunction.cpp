#include "HenyeyGreensteinPhaseFunction.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

HenyeyGreensteinPhaseFunction::HenyeyGreensteinPhaseFunction()
: _g(0.0f)
{
}

inline float HenyeyGreensteinPhaseFunction::henyeyGreenstein(float cosTheta) const
{
    float term = 1.0f + _g*_g - 2.0f*_g*cosTheta;
    return INV_FOUR_PI*(1.0f - _g*_g)/(term*std::sqrt(term));
}

void HenyeyGreensteinPhaseFunction::fromJson(JsonPtr value, const Scene &scene)
{
    PhaseFunction::fromJson(value, scene);
    value.getField("g", _g);
}

rapidjson::Value HenyeyGreensteinPhaseFunction::toJson(Allocator &allocator) const
{
    return JsonObject{PhaseFunction::toJson(allocator), allocator,
        "type", "henyey_greenstein",
        "g", _g
    };
}

Vec3f HenyeyGreensteinPhaseFunction::eval(const Vec3f &wi, const Vec3f &wo) const
{
    return Vec3f(henyeyGreenstein(wi.dot(wo)));
}

bool HenyeyGreensteinPhaseFunction::sample(PathSampleGenerator &sampler, const Vec3f &wi, PhaseSample &sample) const
{
    Vec2f xi = sampler.next2D();
    if (_g == 0.0f) {
        sample.w = SampleWarp::uniformSphere(xi);
        sample.weight = Vec3f(1.0f);
        sample.pdf = SampleWarp::uniformSpherePdf();
    } else {
        float phi = xi.x()*TWO_PI;
        float cosTheta = (1.0f + _g*_g - sqr((1.0f - _g*_g)/(1.0f + _g*(xi.y()*2.0f - 1.0f))))/(2.0f*_g);
        float sinTheta = std::sqrt(max(1.0f - cosTheta*cosTheta, 0.0f));
        sample.w = TangentFrame(wi).toGlobal(Vec3f(
            std::cos(phi)*sinTheta,
            std::sin(phi)*sinTheta,
            cosTheta
        ));
        sample.weight = Vec3f(1.0f);
        sample.pdf = henyeyGreenstein(cosTheta);
    }
    return true;
}

bool HenyeyGreensteinPhaseFunction::invert(WritablePathSampleGenerator &sampler, const Vec3f &wi, const Vec3f &wo) const
{
    if (_g == 0.0f) {
        sampler.put2D(SampleWarp::invertUniformSphere(wo, sampler.untracked1D()));
    } else {
        Vec3f w = TangentFrame(wi).toLocal(wo);

        sampler.put2D(Vec2f(
            SampleWarp::invertPhi(w, sampler.untracked1D()),
            clamp(0.5f*(((1.0f - _g*_g)/std::sqrt(-((2.0f*_g)*w.z() - 1.0f - _g*_g)) - 1.0f)/_g + 1.0f), 0.0f, 1.0f)
        ));
    }

    return true;
}

float HenyeyGreensteinPhaseFunction::pdf(const Vec3f &wi, const Vec3f &wo) const
{
    return henyeyGreenstein(wi.dot(wo));
}

}

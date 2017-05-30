#include "RayleighPhaseFunction.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

inline float RayleighPhaseFunction::rayleigh(float cosTheta)
{
    return (3.0f/(16.0f*PI))*(1.0f + cosTheta*cosTheta);
}

rapidjson::Value RayleighPhaseFunction::toJson(Allocator &allocator) const
{
    return JsonObject{PhaseFunction::toJson(allocator), allocator,
        "type", "rayleigh"
    };
}

Vec3f RayleighPhaseFunction::eval(const Vec3f &wi, const Vec3f &wo) const
{
    return Vec3f(rayleigh(wi.dot(wo)));
}

bool RayleighPhaseFunction::sample(PathSampleGenerator &sampler, const Vec3f &wi, PhaseSample &sample) const
{
    Vec2f xi = sampler.next2D();
    float phi = xi.x()*TWO_PI;
    float z = xi.y()*4.0f - 2.0f;
    float invZ = std::sqrt(z*z + 1.0f);
    float u = std::cbrt(z + invZ);
    float cosTheta = u - 1.0f/u;

    float sinTheta = std::sqrt(max(1.0f - cosTheta*cosTheta, 0.0f));
    sample.w = TangentFrame(wi).toGlobal(Vec3f(
        std::cos(phi)*sinTheta,
        std::sin(phi)*sinTheta,
        cosTheta
    ));
    sample.weight = Vec3f(1.0f);
    sample.pdf = rayleigh(cosTheta);
    return true;
}

bool RayleighPhaseFunction::invert(WritablePathSampleGenerator &sampler, const Vec3f &wi, const Vec3f &wo) const
{
    Vec3f w = TangentFrame(wi).toLocal(wo);
    float cosTheta = w.z();
    float u = 0.5f*(cosTheta + std::sqrt(4.0f + cosTheta*cosTheta));
    float u3 = u*u*u;
    float z = (u3*u3 - 1.0f)/(2.0f*u3);
    float xi2 = (z + 2.0f)*0.25f;
    float xi1 = SampleWarp::invertPhi(w, sampler.untracked1D());

    sampler.put2D(Vec2f(xi1, xi2));

    return true;
}

float RayleighPhaseFunction::pdf(const Vec3f &wi, const Vec3f &wo) const
{
    return rayleigh(wi.dot(wo));
}

}

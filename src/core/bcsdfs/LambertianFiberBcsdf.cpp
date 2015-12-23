#include "LambertianFiberBcsdf.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/Angle.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

LambertianFiberBcsdf::LambertianFiberBcsdf()
{
    _lobes = BsdfLobes(BsdfLobes::DiffuseLobe | BsdfLobes::AnisotropicLobe);
}

// Closed form far-field solution for perfect Lambertian cylinder
// Problem first described in "Light Scattering from Filaments",
// exact solution presented in "Importance Sampling for Physically-Based Hair Fiber Models"
inline float LambertianFiberBcsdf::lambertianCylinder(const Vec3f &wo) const
{
    float cosThetaO = trigInverse(wo.y());
    float phi = std::atan2(wo.x(), wo.z());
    if (phi < 0.0f)
        phi += TWO_PI;

    return cosThetaO*std::abs(((PI - phi)*std::cos(phi) + std::sin(phi))*INV_FOUR_PI);
}

rapidjson::Value LambertianFiberBcsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "lambertian_fiber"
    };
}

Vec3f LambertianFiberBcsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseLobe))
        return Vec3f(0.0f);
    return albedo(event.info)*lambertianCylinder(event.wo);
}

bool LambertianFiberBcsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseLobe))
        return false;

    float h = event.sampler->next1D()*2.0f - 1.0f;
    float nx = h;
    float nz = trigInverse(nx);

    Vec3f d = SampleWarp::cosineHemisphere(event.sampler->next2D());

    event.wo = Vec3f(d.z()*nx + d.x()*nz, d.y(), d.z()*nz - d.x()*nx);
    event.pdf = lambertianCylinder(event.wo);
    event.weight = albedo(event.info);
    event.sampledLobe = BsdfLobes::DiffuseLobe;

    return true;
}

float LambertianFiberBcsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseLobe))
        return 0.0f;
    return lambertianCylinder(event.wo);
}

}

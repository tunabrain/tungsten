#include "PhongBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/TangentFrame.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

PhongBsdf::PhongBsdf(float exponent)
: _exponent(exponent)
{
    init();
}

void PhongBsdf::init()
{
    _lobes       = BsdfLobes(BsdfLobes::GlossyReflectionLobe);
    _invExponent = 1.0f/(1.0f + _exponent);
    _pdfFactor   = (_exponent + 1.0f)*INV_TWO_PI;
    _brdfFactor  = (_exponent + 2.0f)*INV_TWO_PI;
}

void PhongBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);
    JsonUtils::fromJson(v, "exponent", _exponent);
    init();
}

rapidjson::Value PhongBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "phong", allocator);
    v.AddMember("exponent", _exponent, allocator);
    return std::move(v);
}

bool PhongBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
        return false;
    if (event.wi.z() <= 0.0f)
        return false;

    Vec2f xi = event.sampler->next2D();
    float phi      = xi.x()*TWO_PI;
    float cosTheta = std::pow(xi.y(), _invExponent);
    float sinTheta = std::sqrt(max(0.0f, 1.0f - cosTheta*cosTheta));

    Vec3f woLocal(std::cos(phi)*sinTheta, std::sin(phi)*sinTheta, cosTheta);

    TangentFrame lobe(Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()));
    event.wo = lobe.toGlobal(woLocal);
    if (event.wo.z() < 0.0f)
        return false;

    cosTheta = lobe.normal.dot(event.wo);
    float cosThetaPowN = std::pow(cosTheta, _exponent);
    event.pdf = cosThetaPowN*_pdfFactor;
    event.throughput = albedo(event.info)*(event.wo.z()*(_brdfFactor/_pdfFactor));
    return true;
}

Vec3f PhongBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
        return Vec3f(0.0f);
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);
    float cosTheta = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()).dot(event.wo);
    if (cosTheta < 0.0f)
        return Vec3f(0.0f);
    return albedo(event.info)*(std::pow(cosTheta, _exponent)*_brdfFactor*event.wo.z());
}

float PhongBsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe))
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;
    return std::pow(Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()).dot(event.wo), _exponent)*_pdfFactor;
}

}

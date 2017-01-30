#include "PhongBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/TangentFrame.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

PhongBsdf::PhongBsdf(float exponent, float diffuseRatio)
: _exponent(exponent),
  _diffuseRatio(diffuseRatio)
{
    _lobes = BsdfLobes(BsdfLobes::GlossyReflectionLobe);
}

void PhongBsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("exponent", _exponent);
    value.getField("diffuse_ratio", _diffuseRatio);
}

rapidjson::Value PhongBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "phong",
        "exponent", _exponent,
        "diffuse_ratio", _diffuseRatio
    };
}

bool PhongBsdf::sample(SurfaceScatterEvent &event) const
{
    bool evalGlossy  = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool evalDiffuse = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

    if (!evalGlossy && !evalDiffuse)
        return false;
    if (event.wi.z() <= 0.0f)
        return false;

    bool sampleGlossy;
    if (evalGlossy && evalDiffuse)
        sampleGlossy = event.sampler->nextBoolean(1.0f - _diffuseRatio);
    else
        sampleGlossy = evalGlossy;

    if (sampleGlossy) {
        Vec2f xi = event.sampler->next2D();
        float phi      = xi.x()*TWO_PI;
        float cosTheta = std::pow(xi.y(), _invExponent);
        float sinTheta = std::sqrt(max(0.0f, 1.0f - cosTheta*cosTheta));

        Vec3f woLocal(std::cos(phi)*sinTheta, std::sin(phi)*sinTheta, cosTheta);

        TangentFrame lobe(Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()));
        event.wo = lobe.toGlobal(woLocal);
        if (event.wo.z() < 0.0f)
            return false;

        event.sampledLobe = BsdfLobes::GlossyReflectionLobe;
    } else {
        event.wo = SampleWarp::cosineHemisphere(event.sampler->next2D());
        event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
    }

    event.pdf = pdf(event);
    event.weight = eval(event)/event.pdf;

    return true;
}

Vec3f PhongBsdf::eval(const SurfaceScatterEvent &event) const
{
    bool evalGlossy  = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool evalDiffuse = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

    if (!evalGlossy && !evalDiffuse)
        return Vec3f(0.0f);
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);

    float result = 0.0f;
    if (evalDiffuse)
        result += _diffuseRatio*INV_PI;
    if (evalGlossy) {
        float cosTheta = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()).dot(event.wo);
        if (cosTheta > 0.0f)
            result += std::pow(cosTheta, _exponent)*_brdfFactor*(1.0f - _diffuseRatio);
    }

    return albedo(event.info)*event.wo.z()*result;
}

float PhongBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool evalGlossy  = event.requestedLobe.test(BsdfLobes::GlossyReflectionLobe);
    bool evalDiffuse = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);

    if (!evalGlossy && !evalDiffuse)
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    float result = 0.0f;
    if (evalGlossy) {
        float cosTheta = Vec3f(-event.wi.x(), -event.wi.y(), event.wi.z()).dot(event.wo);
        if (cosTheta > 0.0f)
            result += std::pow(cosTheta, _exponent)*_pdfFactor;
    }
    if (evalDiffuse && evalGlossy)
        result = result*(1.0f - _diffuseRatio) + _diffuseRatio*SampleWarp::cosineHemispherePdf(event.wo);
    else if (evalDiffuse)
        result = SampleWarp::cosineHemispherePdf(event.wo);

    return result;
}

void PhongBsdf::prepareForRender()
{
    _lobes       = BsdfLobes(BsdfLobes::GlossyReflectionLobe | BsdfLobes::DiffuseReflectionLobe);
    _invExponent = 1.0f/(1.0f + _exponent);
    _pdfFactor   = (_exponent + 1.0f)*INV_TWO_PI;
    _brdfFactor  = (_exponent + 2.0f)*INV_TWO_PI;
}

}

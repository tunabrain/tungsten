#include "OrenNayarBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "materials/ConstantTexture.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

OrenNayarBsdf::OrenNayarBsdf()
: _roughness(std::make_shared<ConstantTexture>(1.0f))
{
    _lobes = BsdfLobes(BsdfLobes::DiffuseReflectionLobe);
}

void OrenNayarBsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Bsdf::fromJson(v, scene);

    scene.textureFromJsonMember(v, "roughness", TexelConversion::REQUEST_AVERAGE, _roughness);
}

rapidjson::Value OrenNayarBsdf::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Bsdf::toJson(allocator);
    v.AddMember("type", "oren_nayar", allocator);
    JsonUtils::addObjectMember(v, "roughness", *_roughness, allocator);
    return std::move(v);
}


bool OrenNayarBsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return false;
    if (event.wi.z() <= 0.0f)
        return false;

    float roughness = (*_roughness)[*event.info].x();
    float ratio = clamp(roughness, 0.01f, 1.0f);
    if (event.sampler->nextBoolean(DiscreteBsdfSample, ratio))
        event.wo  = SampleWarp::uniformHemisphere(event.sampler->next2D(BsdfSample));
    else
        event.wo  = SampleWarp::cosineHemisphere(event.sampler->next2D(BsdfSample));

    event.pdf = SampleWarp::uniformHemispherePdf(event.wo)*ratio + SampleWarp::cosineHemispherePdf(event.wo)*(1.0f - ratio);
    event.weight = eval(event)/event.pdf;
    event.sampledLobe = BsdfLobes::DiffuseReflectionLobe;
    return event.wo.z() > 0.0f;
}

Vec3f OrenNayarBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return Vec3f(0.0f);
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return Vec3f(0.0f);

    const Vec3f &wi = event.wi;
    const Vec3f &wo = event.wo;

    float thetaR = std::acos(event.wo.z());
    float thetaI = std::acos(event.wi.z());
    float alpha = max(thetaR, thetaI);
    float beta  = min(thetaR, thetaI);
    float sinAlpha = std::sin(alpha);
    float denom = (wi.x()*wi.x() + wi.y()*wi.y())*(wo.x()*wo.x() + wo.y()*wo.y());
    float cosDeltaPhi;
    if (denom == 0.0f)
        cosDeltaPhi = 1.0f;
    else
        cosDeltaPhi = (wi.x()*wo.x() + wi.y()*wo.y())/std::sqrt(denom);

    const float RoughnessToSigma = 1.0f/std::sqrt(2.0f);
    float sigma = RoughnessToSigma*(*_roughness)[*event.info].x();
    float sigmaSq = sigma*sigma;

    float C1 = 1.0f - 0.5f*sigmaSq/(sigmaSq + 0.33f);
    float C2 = 0.45f*sigmaSq/(sigmaSq + 0.09f);
    if (cosDeltaPhi >= 0.0f)
        C2 *= sinAlpha;
    else
        C2 *= sinAlpha - cube((2.0f*INV_PI)*beta);
    float C3 = 0.125f*(sigmaSq/(sigmaSq + 0.09f))*sqr((4.0f*INV_PI*INV_PI)*alpha*beta);

    float fr1 = (C1 + cosDeltaPhi*C2*std::tan(beta) + (1.0f - std::abs(cosDeltaPhi))*C3*std::tan(0.5f*(alpha + beta)));
    float fr2 = 0.17f*sigmaSq/(sigmaSq + 0.13f)*(1.0f - cosDeltaPhi*sqr((2.0f*INV_PI)*beta));

    Vec3f diffuseAlbedo = albedo(event.info);
    return (diffuseAlbedo*fr1 + diffuseAlbedo*diffuseAlbedo*fr2)*wo.z()*INV_PI;
}

float OrenNayarBsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe))
        return 0.0f;
    if (event.wi.z() <= 0.0f || event.wo.z() <= 0.0f)
        return 0.0f;

    float roughness = (*_roughness)[*event.info].x();
    float ratio = clamp(roughness, 0.01f, 1.0f);
    return SampleWarp::uniformHemispherePdf(event.wo)*ratio + SampleWarp::cosineHemispherePdf(event.wo)*(1.0f - ratio);
}

}

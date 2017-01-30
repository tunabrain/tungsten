#include "RoughWireBcsdf.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "bsdfs/ComplexIor.hpp"
#include "bsdfs/Fresnel.hpp"

#include "math/Angle.hpp"

#include "io/JsonObject.hpp"

#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

RoughWireBcsdf::RoughWireBcsdf()
: _materialName("Cu"),
  _eta(0.200438f, 0.924033f, 1.10221f),
  _k(3.91295f, 2.45285f, 2.14219f),
  _roughness(0.1f)
{
    _lobes = BsdfLobes(BsdfLobes::GlossyLobe | BsdfLobes::AnisotropicLobe);
}

bool RoughWireBcsdf::lookupMaterial()
{
    return ComplexIorList::lookup(_materialName, _eta, _k);
}

// Modified Bessel function of the first kind
float RoughWireBcsdf::I0(float x)
{
    float result = 1.0f;
    float xSq = x*x;
    float xi = xSq;
    float denom = 4.0f;
    for (int i = 1; i <= 10; ++i) {
        result += xi/denom;
        xi *= xSq;
        denom *= 4.0f*float((i + 1)*(i + 1));
    }
    return result;
}

float RoughWireBcsdf::logI0(float x)
{
    if (x > 12.0f)
        // More stable evaluation of log(I0(x))
        // See also https://publons.com/discussion/12/
        return x + 0.5f*(std::log(1.0f/(TWO_PI*x)) + 1.0f/(8.0f*x));
    else
        return std::log(I0(x));
}

// Azimuthal scattering function. Assumes perfectly smooth reflection in
// the azimuth, which reduces the scattering function to the jacobian
// from h to phi
float RoughWireBcsdf::N(float cosPhi) const
{
    return 0.25f*trigHalfAngle(cosPhi);
}

// Rough longitudinal scattering function with variance v = beta^2
float RoughWireBcsdf::M(float v, float sinThetaI, float sinThetaO, float cosThetaI, float cosThetaO) const
{
    float a = cosThetaI*cosThetaO/v;
    float b = sinThetaI*sinThetaO/v;

    if (v < 0.1f)
        // More numerically stable evaluation for small roughnesses
        // See https://publons.com/discussion/12/
        return std::exp(-b + logI0(a) - 1.0f/v + 0.6931f + std::log(1.0f/(2.0f*v)));
    else
        return std::exp(-b)*I0(a)/(2.0f*v*std::sinh(1.0f/v));
}

// Returns sinPhi
float RoughWireBcsdf::sampleN(float xi) const
{
    return 2.0f*xi - 1.0f; // Well that was easy
}

// Returns sinThetaO
float RoughWireBcsdf::sampleM(float v, float sinThetaI, float cosThetaI, float xi1, float xi2) const
{
    // Version from the paper (unusably unstable)
    //float cosTheta = v*std::log(std::exp(1.0f/v) - 2.0f*xi1*std::sinh(1.0f/v));
    // More stable version from "Numerically stable sampling of the von Mises Fisher distribution on S2 (and other tricks)"
    float cosTheta = 1.0f + v*std::log(xi1 + (1.0f - xi1)*std::exp(-2.0f/v));
    float sinTheta = trigInverse(cosTheta);
    float cosPhi = std::cos(TWO_PI*xi2);

    return -cosTheta*sinThetaI + sinTheta*cosPhi*cosThetaI;
}

void RoughWireBcsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("roughness", _roughness);
    if (value.getField("eta", _eta) && value.getField("k", _k))
        _materialName.clear();
    if (value.getField("material", _materialName) && !lookupMaterial())
        value.parseError(tfm::format("Unable to find material with name '%s'", _materialName));
}

rapidjson::Value RoughWireBcsdf::toJson(Allocator &allocator) const
{
    JsonObject result{Bsdf::toJson(allocator), allocator,
        "type", "rough_wire",
        "roughness", _roughness
    };
    if (_materialName.empty())
        result.add("eta", _eta, "k", _k);
    else
        result.add("material", _materialName);

    return result;
}

Vec3f RoughWireBcsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyLobe) || event.wo.z() == 0.0f)
        return Vec3f(0.0f);

    float sinThetaI = event.wi.y();
    float sinThetaO = event.wo.y();
    float cosThetaI = trigInverse(sinThetaI);
    float cosThetaO = trigInverse(sinThetaO);
    float cosPhi = event.wo.z()/std::sqrt(event.wo.x()*event.wo.x() + event.wo.z()*event.wo.z());

    Vec3f attenuation = albedo(event.info)*Fresnel::conductorReflectance(_eta, _k, trigHalfAngle(event.wi.dot(event.wo)));

    return attenuation*N(cosPhi)*M(_v, sinThetaI, sinThetaO, cosThetaI, cosThetaO);
}

bool RoughWireBcsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyLobe))
        return false;

    float xi1 = event.sampler->next1D();
    Vec2f xi23 = event.sampler->next2D();

    float sinThetaI = event.wi.y();
    float cosThetaI = trigInverse(sinThetaI);

    float sinPhi = sampleN(xi1);
    float sinThetaO = sampleM(_v, sinThetaI, cosThetaI, xi23.x(), xi23.y());

    float cosPhi = trigInverse(sinPhi);
    float cosThetaO = trigInverse(sinThetaO);

    event.wo = Vec3f(sinPhi*cosThetaO, sinThetaO, cosPhi*cosThetaO);
    event.pdf = N(cosPhi)*M(_v, sinThetaI, sinThetaO, cosThetaI, cosThetaO);
    event.weight = albedo(event.info)*Fresnel::conductorReflectance(_eta, _k, trigHalfAngle(event.wi.dot(event.wo)));
    event.sampledLobe = BsdfLobes::GlossyLobe;

    return true;
}

float RoughWireBcsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyLobe))
        return 0.0f;

    float sinThetaI = event.wi.y();
    float sinThetaO = event.wo.y();
    float cosThetaI = trigInverse(sinThetaI);
    float cosThetaO = trigInverse(sinThetaO);
    float cosPhi = event.wo.z()/std::sqrt(event.wo.x()*event.wo.x() + event.wo.z()*event.wo.z());

    return N(cosPhi)*M(_v, sinThetaI, sinThetaO, cosThetaI, cosThetaO);
}

void RoughWireBcsdf::prepareForRender()
{
    _v = sqr(_roughness*PI_HALF);
}

}

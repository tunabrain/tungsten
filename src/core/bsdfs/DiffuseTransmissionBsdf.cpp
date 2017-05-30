#include "DiffuseTransmissionBsdf.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/Angle.hpp"
#include "math/Vec.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

DiffuseTransmissionBsdf::DiffuseTransmissionBsdf()
: _transmittance(0.5f)
{
    _lobes = BsdfLobes(BsdfLobes::DiffuseTransmissionLobe | BsdfLobes::DiffuseReflectionLobe);
}

rapidjson::Value DiffuseTransmissionBsdf::toJson(Allocator &allocator) const
{
    return JsonObject{Bsdf::toJson(allocator), allocator,
        "type", "diffuse_transmission"
    };
}

bool DiffuseTransmissionBsdf::sample(SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseTransmissionLobe);
    if (!sampleR && !sampleT)
        return false;

    float transmittanceProbability = sampleR && sampleT ? _transmittance : (sampleR ? 0.0f : 1.0f);
    bool transmit = event.sampler->nextBoolean(transmittanceProbability);
    float weight = sampleR && sampleT ? 1.0f : (transmit ? _transmittance : 1.0f - _transmittance);

    event.wo = SampleWarp::cosineHemisphere(event.sampler->next2D());
    event.wo.z() = std::copysign(event.wo.z(), event.wi.z());
    if (transmit)
        event.wo.z() = -event.wo.z();
    event.pdf = SampleWarp::cosineHemispherePdf(event.wo);
    event.weight = albedo(event.info)*weight;
    event.sampledLobe = BsdfLobes::DiffuseTransmissionLobe;
    return true;
}

Vec3f DiffuseTransmissionBsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::DiffuseTransmissionLobe))
        return Vec3f(0.0f);

    float factor = event.wi.z()*event.wo.z() < 0.0f ? _transmittance : 1.0f - _transmittance;
    return albedo(event.info)*factor*INV_PI*std::abs(event.wo.z());
}

bool DiffuseTransmissionBsdf::invert(WritablePathSampleGenerator &sampler, const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseTransmissionLobe);
    if (!sampleR && !sampleT)
        return false;

    bool transmit = event.wi.z()*event.wo.z() < 0.0f;
    if ((transmit && !sampleT) || (!transmit && !sampleR))
        return false;

    float transmittanceProbability = sampleR && sampleT ? _transmittance : (sampleR ? 0.0f : 1.0f);

    sampler.putBoolean(transmittanceProbability, transmit);
    sampler.put2D(SampleWarp::invertCosineHemisphere(event.wo, sampler.untracked1D()));

    return true;
}

float DiffuseTransmissionBsdf::pdf(const SurfaceScatterEvent &event) const
{
    bool sampleR = event.requestedLobe.test(BsdfLobes::DiffuseReflectionLobe);
    bool sampleT = event.requestedLobe.test(BsdfLobes::DiffuseTransmissionLobe);
    if (!sampleR && !sampleT)
        return 0.0f;

    float transmittanceProbability = sampleR && sampleT ? _transmittance : (sampleR ? 0.0f : 1.0f);

    float factor = event.wi.z()*event.wo.z() < 0.0f ? transmittanceProbability : 1.0f - transmittanceProbability;
    return factor*SampleWarp::cosineHemispherePdf(event.wo);
}

}

#ifndef VOLUMESCATTERINGEVENT_HPP_
#define VOLUMESCATTERINGEVENT_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

class SampleGenerator;
class UniformSampler;

struct VolumeScatterEvent
{
    SampleGenerator *sampler;
    UniformSampler *supplementalSampler;
    Vec3f currentThroughput;
    Vec3f p;
    Vec3f wi;
    float maxT;

    Vec3f wo;
    float t;
    Vec3f throughput;
    float pdf;

    VolumeScatterEvent() = default;

    VolumeScatterEvent(SampleGenerator *sampler_,
                 UniformSampler *supplementalSampler_,
                 Vec3f currentThroughput_,
                 const Vec3f &p_,
                 const Vec3f &wi_,
                 float maxT_)
    : sampler(sampler_),
      supplementalSampler(supplementalSampler_),
      currentThroughput(currentThroughput_),
      p(p_),
      wi(wi_),
      maxT(maxT_),
      wo(0.0f),
      t(maxT_),
      throughput(1.0f)
    {
    }

    VolumeScatterEvent(const Vec3f &p_, const Vec3f &wi_, float maxT_)
    : VolumeScatterEvent(nullptr, nullptr, Vec3f(1.0f), p_, wi_, maxT_)
    {
    }

    VolumeScatterEvent makeWarpedQuery(const Vec3f &newWi, const Vec3f &newWo) const
    {
        VolumeScatterEvent copy(*this);
        copy.wi = newWi;
        copy.wo = newWo;
        return copy;
    }

    VolumeScatterEvent makeFlippedQuery() const
    {
        return makeWarpedQuery(wo, wi);
    }
};

}

#endif /* VOLUMESCATTERINGEVENT_HPP_ */

#ifndef SCATTEREVENT_HPP_
#define SCATTEREVENT_HPP_

#include "bsdfs/BsdfLobes.hpp"

#include "math/TangentFrame.hpp"
#include "math/Vec.hpp"

namespace Tungsten {

class IntersectionInfo;
class SampleGenerator;
class UniformSampler;

struct SurfaceScatterEvent
{
    const IntersectionInfo *info;
    SampleGenerator *sampler;
    UniformSampler *supplementalSampler;
    TangentFrame frame;
    Vec3f wi, wo;
    Vec3f throughput;
    float pdf;
    BsdfLobes requestedLobe;
    BsdfLobes sampledLobe;
    bool flippedFrame;

    SurfaceScatterEvent(const IntersectionInfo *info_,
                 SampleGenerator *sampler_,
                 UniformSampler *supplementalSampler_,
                 const TangentFrame &frame_,
                 const Vec3f &wi_,
                 BsdfLobes requestedLobe_,
                 bool flippedFrame_)
    : info(info_),
      sampler(sampler_),
      supplementalSampler(supplementalSampler_),
      frame(frame_),
      wi(wi_),
      wo(0.0f),
      throughput(1.0f),
      pdf(1.0f),
      requestedLobe(requestedLobe_),
      flippedFrame(flippedFrame_)
    {
    }

    SurfaceScatterEvent makeWarpedQuery(const Vec3f &newWi, const Vec3f &newWo) const
    {
        SurfaceScatterEvent copy(*this);
        copy.wi = newWi;
        copy.wo = newWo;
        return copy;
    }

    SurfaceScatterEvent makeForwardEvent() const
    {
        SurfaceScatterEvent copy(*this);
        copy.wo = -copy.wi;
        copy.requestedLobe = BsdfLobes::ForwardLobe;
        return copy;
    }
};

}

#endif /* SCATTEREVENT_HPP_ */

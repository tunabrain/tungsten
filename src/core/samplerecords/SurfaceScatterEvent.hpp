#ifndef SCATTEREVENT_HPP_
#define SCATTEREVENT_HPP_

#include "bsdfs/BsdfLobes.hpp"

#include "math/TangentFrame.hpp"
#include "math/Vec.hpp"

namespace Tungsten {

struct IntersectionInfo;
class PathSampleGenerator;

struct SurfaceScatterEvent
{
    const IntersectionInfo *info;
    PathSampleGenerator *sampler;
    TangentFrame frame;
    Vec3f wi, wo;
    Vec3f weight;
    float pdf;
    BsdfLobes requestedLobe;
    BsdfLobes sampledLobe;
    bool flippedFrame;

    SurfaceScatterEvent() = default;

    SurfaceScatterEvent(const IntersectionInfo *info_,
                 PathSampleGenerator *sampler_,
                 const TangentFrame &frame_,
                 const Vec3f &wi_,
                 BsdfLobes requestedLobe_,
                 bool flippedFrame_)
    : info(info_),
      sampler(sampler_),
      frame(frame_),
      wi(wi_),
      wo(0.0f),
      weight(1.0f),
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

    SurfaceScatterEvent makeFlippedQuery() const
    {
        return makeWarpedQuery(wo, wi);
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

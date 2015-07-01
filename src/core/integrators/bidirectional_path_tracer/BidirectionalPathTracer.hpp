#ifndef BIDIRECTIONALPATHTRACER_HPP_
#define BIDIRECTIONALPATHTRACER_HPP_

#include "BidirectionalPathTracerSettings.hpp"
#include "LightPath.hpp"

#include "integrators/TraceBase.hpp"

namespace Tungsten {

class PathVertex;

class BidirectionalPathTracer : public TraceBase
{
    AtomicFramebuffer *_splatBuffer;

    std::unique_ptr<LightPath> _cameraPath;
    std::unique_ptr<LightPath> _emitterPath;

public:
    BidirectionalPathTracer(TraceableScene *scene, const BidirectionalPathTracerSettings &settings, uint32 threadId);

    Vec3f traceSample(Vec2u pixel, PathSampleGenerator &sampler);
};

}

#endif /* BIDIRECTIONALPATHTRACER_HPP_ */

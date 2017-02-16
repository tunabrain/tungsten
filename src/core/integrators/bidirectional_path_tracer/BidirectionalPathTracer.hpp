#ifndef BIDIRECTIONALPATHTRACER_HPP_
#define BIDIRECTIONALPATHTRACER_HPP_

#include "BidirectionalPathTracerSettings.hpp"
#include "LightPath.hpp"

#include "integrators/TraceBase.hpp"

namespace Tungsten {

class ImagePyramid;
class PathVertex;

class BidirectionalPathTracer : public TraceBase
{
    AtomicFramebuffer *_splatBuffer;
    ImagePyramid *_imagePyramid;

    std::unique_ptr<Vec3f[]> _directEmissionByBounce;
    std::unique_ptr<LightPath> _cameraPath;
    std::unique_ptr<LightPath> _emitterPath;

public:
    BidirectionalPathTracer(TraceableScene *scene, const BidirectionalPathTracerSettings &settings,
            uint32 threadId, ImagePyramid *imagePyramid);

    Vec3f traceSample(Vec2u pixel, uint32 lightPathId, PathSampleGenerator &sampler);
};

}

#endif /* BIDIRECTIONALPATHTRACER_HPP_ */

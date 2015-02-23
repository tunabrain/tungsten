#ifndef PATHTRACER_HPP_
#define PATHTRACER_HPP_

#include "integrators/TraceBase.hpp"

namespace Tungsten {

class PathTracer : public TraceBase
{
public:
    PathTracer(TraceableScene *scene, const TraceSettings &settings, uint32 threadId);

    Vec3f traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler);
};

}

#endif /* PATHTRACER_HPP_ */

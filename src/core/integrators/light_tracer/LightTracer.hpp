#ifndef LIGHTTRACER_HPP_
#define LIGHTTRACER_HPP_

#include "LightTracerSettings.hpp"

#include "integrators/TraceBase.hpp"

namespace Tungsten {

class LightTracer : public TraceBase
{
    LightTracerSettings _settings;
    AtomicFramebuffer *_splatBuffer;

public:
    LightTracer(TraceableScene *scene, const LightTracerSettings &settings, uint32 threadId);

    void traceSample(PathSampleGenerator &sampler);
};

}

#endif /* LIGHTTRACER_HPP_ */

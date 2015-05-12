#ifndef LIGHTTRACER_HPP_
#define LIGHTTRACER_HPP_

#include "LightTracerSettings.hpp"

#include "integrators/TraceBase.hpp"

#include "sampling/Distribution1D.hpp"

namespace Tungsten {

class LightTracer : public TraceBase
{
    AtomicFramebuffer *_splatBuffer;

    std::unique_ptr<Distribution1D> _lightSampler;

public:
    LightTracer(TraceableScene *scene, const LightTracerSettings &settings, uint32 threadId);

    void traceSample(SampleGenerator &sampler, SampleGenerator &supplementalSampler);
};

}

#endif /* LIGHTTRACER_HPP_ */

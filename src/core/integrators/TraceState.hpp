#ifndef TRACESTATE_HPP_
#define TRACESTATE_HPP_

#include "media/Medium.hpp"

#include "math/Ray.hpp"

namespace Tungsten {

class PathSampleGenerator;

// TODO: Modify TraceBase to primarily take/return TraceState as parameter
struct TraceState
{
    PathSampleGenerator &sampler;
    Medium::MediumState mediumState;
    const Medium *medium;
    bool wasSpecular;
    Ray ray;
    int bounce;

    TraceState(PathSampleGenerator &sampler_)
    : sampler(sampler_),
      medium(nullptr),
      wasSpecular(true),
      ray(Vec3f(0.0f), Vec3f(0.0f)),
      bounce(0)
    {
        mediumState.reset();
    }
};

}

#endif /* TRACESTATE_HPP_ */

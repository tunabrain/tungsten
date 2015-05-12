#ifndef KELEMENMLTTRACER_HPP_
#define KELEMENMLTTRACER_HPP_

#include "KelemenMltSettings.hpp"
#include "MetropolisSampler.hpp"

#include "integrators/path_tracer/PathTracer.hpp"

namespace Tungsten {

class AtomicFramebuffer;

class KelemenMltTracer : public PathTracer
{
    struct PathCandidate
    {
        uint64 state;
        float luminanceSum;
    };

    AtomicFramebuffer *_splatBuffer;
    KelemenMltSettings _settings;
    UniformSampler _sampler;

    std::unique_ptr<PathCandidate[]> _pathCandidates;

    void tracePath(SampleGenerator &sampler, Vec2u &pixel, Vec3f &f, float &i);

    void selectSeedPath(int &idx, float &weight);

public:
    KelemenMltTracer(TraceableScene *scene, const KelemenMltSettings &settings, uint32 threadId);

    void startSampleChain(int chainLength);
};

}

#endif /* KELEMENMLTTRACER_HPP_ */

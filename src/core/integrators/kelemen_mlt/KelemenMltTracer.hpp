#ifndef KELEMENMLTTRACER_HPP_
#define KELEMENMLTTRACER_HPP_

#include "KelemenMltSettings.hpp"
#include "MetropolisSampler.hpp"
#include "SplatQueue.hpp"

#include "integrators/bidirectional_path_tracer/LightPath.hpp"

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

    std::unique_ptr<SplatQueue> _currentSplats;
    std::unique_ptr<SplatQueue> _proposedSplats;
    std::unique_ptr<PathCandidate[]> _pathCandidates;

    std::unique_ptr<LightPath> _cameraPath;
    std::unique_ptr<LightPath> _emitterPath;

    void tracePath(PathSampleGenerator &cameraSampler, PathSampleGenerator &emitterSampler, SplatQueue &splatQueue);

    void selectSeedPath(int &idx, float &weight);

public:
    KelemenMltTracer(TraceableScene *scene, const KelemenMltSettings &settings, uint32 threadId);

    void startSampleChain(int chainLength);
};

}

#endif /* KELEMENMLTTRACER_HPP_ */

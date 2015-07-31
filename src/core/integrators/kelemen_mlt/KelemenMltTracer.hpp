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
    AtomicFramebuffer *_splatBuffer;
    KelemenMltSettings _settings;
    UniformSampler _sampler;

    std::unique_ptr<SplatQueue> _currentSplats;
    std::unique_ptr<SplatQueue> _proposedSplats;
    std::unique_ptr<MetropolisSampler> _cameraSampler;
    std::unique_ptr<MetropolisSampler> _emitterSampler;
    std::unique_ptr<LightPath> _cameraPath;
    std::unique_ptr<LightPath> _emitterPath;

public:
    KelemenMltTracer(TraceableScene *scene, const KelemenMltSettings &settings, uint64 seed, uint32 threadId);

    void tracePath(PathSampleGenerator &cameraSampler, PathSampleGenerator &emitterSampler, SplatQueue &splatQueue);

    void startSampleChain(UniformSampler &replaySampler, float luminance);
    void runSampleChain(int chainLength, float luminanceScale);

    UniformSampler &sampler()
    {
        return _sampler;
    }
};

}

#endif /* KELEMENMLTTRACER_HPP_ */

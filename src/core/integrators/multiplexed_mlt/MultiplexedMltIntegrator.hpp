#ifndef MULTIPLEXEDMLTINTEGRATOR_HPP_
#define MULTIPLEXEDMLTINTEGRATOR_HPP_

#include "MultiplexedMltSettings.hpp"
#include "MultiplexedMltTracer.hpp"
#include "MultiplexedStats.hpp"

#include "integrators/Integrator.hpp"
#include "integrators/ImageTile.hpp"

#include "integrators/bidirectional_path_tracer/ImagePyramid.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "thread/TaskGroup.hpp"

#include "math/MathUtil.hpp"

#include <thread>
#include <memory>
#include <vector>
#include <atomic>

namespace Tungsten {

class MultiplexedMltIntegrator : public Integrator
{
    struct PathCandidate
    {
        uint64 cameraState;
        uint64 emitterState;
        uint32 sequence;
        float luminance;
        double luminanceSum;
        int s, t;
    };
    struct SubtaskData
    {
        std::unique_ptr<LargeStepTracker[]> independentEstimator;
        uint32 rangeStart  = 0;
        uint32 rangeLength = 0;
        uint32 raysCast    = 0;

        SubtaskData() = default;
        SubtaskData(SubtaskData &&o)
        : independentEstimator(std::move(o.independentEstimator)),
          rangeStart(o.rangeStart),
          rangeLength(o.rangeLength),
          raysCast(o.raysCast)
        {
        }
    };

    MultiplexedMltSettings _settings;

    std::shared_ptr<TaskGroup> _group;

    uint32 _w;
    uint32 _h;

    UniformSampler _sampler;
    std::vector<SubtaskData> _subtaskData;
    std::vector<std::unique_ptr<MultiplexedMltTracer>> _tracers;

    bool _chainsLaunched;
    double _luminanceScale;
    std::atomic<uint64> _numSeedPathsTraced;
    std::vector<LargeStepTracker> _luminancePerLength;
    std::unique_ptr<PathCandidate[]> _pathCandidates;

    std::unique_ptr<AtomicMultiplexedStats> _stats;
    std::unique_ptr<ImagePyramid> _imagePyramid;

    virtual void saveState(OutputStreamHandle &out) override;
    virtual void loadState(InputStreamHandle &in) override;

    void traceSamplePool(uint32 taskId, uint32 numSubTasks, uint32 threadId);
    void runSampleChain(uint32 taskId, uint32 numSubTasks, uint32 threadId);

    void selectSeedPaths();

    void computeNormalizationFactor();
    void setBufferWeights();

public:
    MultiplexedMltIntegrator();

    virtual void fromJson(JsonPtr v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void saveOutputs() override;

    virtual void prepareForRender(TraceableScene &scene, uint32 seed) override;
    virtual void teardownAfterRender() override;

    virtual void startRender(std::function<void()> completionCallback) override;
    virtual void waitForCompletion() override;
    virtual void abortRender() override;
};

}

#endif /* MULTIPLEXEDMLTINTEGRATOR_HPP_ */

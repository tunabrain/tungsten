#ifndef LIGHTTRACEINTEGRATOR_HPP_
#define LIGHTTRACEINTEGRATOR_HPP_

#include "LightTracerSettings.hpp"
#include "LightTracer.hpp"

#include "integrators/Integrator.hpp"
#include "integrators/ImageTile.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "thread/TaskGroup.hpp"

#include "math/MathUtil.hpp"

#include <thread>
#include <memory>
#include <vector>
#include <atomic>

namespace Tungsten {

class LightTraceIntegrator : public Integrator
{
    LightTracerSettings _settings;

    std::shared_ptr<TaskGroup> _group;

    uint32 _w;
    uint32 _h;

    UniformSampler _sampler;
    std::vector<std::unique_ptr<LightTracer>> _tracers;
    std::vector<std::unique_ptr<PathSampleGenerator>> _taskData;

    virtual void saveState(OutputStreamHandle &out) override;
    virtual void loadState(InputStreamHandle &in) override;

    void traceRays(uint32 taskId, uint32 numSubTasks, uint32 threadId);

public:
    LightTraceIntegrator();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void prepareForRender(TraceableScene &scene, uint32 seed) override;
    virtual void teardownAfterRender() override;

    virtual void startRender(std::function<void()> completionCallback) override;
    virtual void waitForCompletion() override;
    virtual void abortRender() override;
};

}

#endif /* LIGHTTRACEINTEGRATOR_HPP_ */

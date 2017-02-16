#ifndef BIDIRECTIONALPATHTRACEINTEGRATOR_HPP_
#define BIDIRECTIONALPATHTRACEINTEGRATOR_HPP_

#include "BidirectionalPathTracer.hpp"

#include "integrators/TraceSettings.hpp"
#include "integrators/Integrator.hpp"
#include "integrators/ImageTile.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/UniformPathSampler.hpp"
#include "sampling/UniformSampler.hpp"

#include "thread/TaskGroup.hpp"

#include "math/MathUtil.hpp"

#include <thread>
#include <memory>
#include <vector>
#include <atomic>

namespace Tungsten {

class ImagePyramid;

class BidirectionalPathTraceIntegrator : public Integrator
{
    static CONSTEXPR uint32 TileSize = 16;

    BidirectionalPathTracerSettings _settings;

    std::shared_ptr<TaskGroup> _group;

    uint32 _w;
    uint32 _h;

    UniformSampler _sampler;
    std::vector<std::unique_ptr<BidirectionalPathTracer>> _tracers;

    std::vector<ImageTile> _tiles;
    std::unique_ptr<ImagePyramid> _imagePyramid;

    void diceTiles();

    void renderTile(uint32 id, uint32 tileId);

    virtual void saveState(OutputStreamHandle &out) override;
    virtual void loadState(InputStreamHandle &in) override;

public:
    BidirectionalPathTraceIntegrator();
    ~BidirectionalPathTraceIntegrator();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void prepareForRender(TraceableScene &scene, uint32 seed) override;
    virtual void teardownAfterRender() override;

    virtual bool supportsResumeRender() const override;

    virtual void startRender(std::function<void()> completionCallback) override;
    virtual void waitForCompletion() override;
    virtual void abortRender() override;

    virtual void saveOutputs() override;
};

}

#endif /* BIDIRECTIONALPATHTRACEINTEGRATOR_HPP_ */

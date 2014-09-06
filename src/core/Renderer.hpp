#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "math/MathUtil.hpp"

#include "ThreadPool.hpp"
#include "IntTypes.hpp"

#include <functional>
#include <thread>
#include <memory>
#include <atomic>

namespace Tungsten {

class Integrator;

struct SampleRecord
{
    uint32 sampleCount, nextSampleCount, sampleIndex;
    float adaptiveWeight;
    float mean, runningVariance;

    SampleRecord()
    : sampleCount(0), nextSampleCount(0), sampleIndex(0),
      adaptiveWeight(0.0f),
      mean(0.0f), runningVariance(0.0f)
    {
    }

    inline void addSample(float x)
    {
        sampleCount++;
        float delta = x - mean;
        mean += delta/sampleCount;
        runningVariance += delta*(x - mean);
    }

    inline void addSample(const Vec3f &x)
    {
        addSample(x.luminance());
    }

    inline float variance() const
    {
        return runningVariance/(sampleCount - 1);
    }

    inline float errorEstimate() const
    {
        return variance()/(sampleCount*max(mean*mean, 1e-3f));
    }
};

struct ImageTile
{
    uint32 x, y, w, h;
    std::unique_ptr<SampleGenerator> sampler;
    std::unique_ptr<UniformSampler> supplementalSampler;

    ImageTile(uint32 x_, uint32 y_, uint32 w_, uint32 h_,
            std::unique_ptr<SampleGenerator> sampler_,
            std::unique_ptr<UniformSampler> supplementalSampler_)
    :   x(x_), y(y_), w(w_), h(h_),
        sampler(std::move(sampler_)),
        supplementalSampler(std::move(supplementalSampler_))
    {
    }
};

class Renderer
{
    static constexpr uint32 TileSize = 16;
    static constexpr uint32 VarianceTileSize = 4;
    static constexpr uint32 AdaptiveThreshold = 16;

    ThreadPool _threadPool;

    std::atomic<bool> _abortRender;
    std::atomic<int> _inFlightCount;

    std::mutex _completionMutex;
    std::condition_variable _completionCond;
    std::function<void()> _completionCallback;

    uint32 _w;
    uint32 _h;
    uint32 _varianceW;
    uint32 _varianceH;

    UniformSampler _sampler;
    const TraceableScene &_scene;
    std::vector<std::unique_ptr<Integrator>> _integrators;

    std::vector<SampleRecord> _samples;
    std::vector<ImageTile> _tiles;

    void diceTiles();

    float errorPercentile95() const;
    void dilateAdaptiveWeights();
    void distributeAdaptiveSamples(int spp);
    bool generateWork(uint32 sppFrom, uint32 sppTo);

    void renderTile(uint32 id, uint32 tileId);

public:
    Renderer(const TraceableScene &scene, uint32 threadCount);
    ~Renderer();

    void startRender(std::function<void()> completionCallback, uint32 sppFrom, uint32 sppTo);
    void waitForCompletion();
    void abortRender();

    void saveVariance(const std::string &path) const;
};

}

#endif /* RENDERER_HPP_ */

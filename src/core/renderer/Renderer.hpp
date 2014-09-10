#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include "SampleRecord.hpp"
#include "ImageTile.hpp"

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

class TraceableScene;
class Integrator;

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

    float errorPercentile95();
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

    void getVarianceImage(std::vector<float> &data, int &w, int &h);
};

}

#endif /* RENDERER_HPP_ */

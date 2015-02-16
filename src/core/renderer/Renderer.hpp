#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include "SampleRecord.hpp"
#include "ImageTile.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "thread/TaskGroup.hpp"

#include "math/MathUtil.hpp"

#include "IntTypes.hpp"

#include <functional>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>

namespace Tungsten {

class TraceableScene;
class Integrator;
class Scene;

class Renderer
{
    static CONSTEXPR uint32 TileSize = 16;
    static CONSTEXPR uint32 VarianceTileSize = 4;
    static CONSTEXPR uint32 AdaptiveThreshold = 16;

    std::shared_ptr<TaskGroup> _group;

    uint32 _w;
    uint32 _h;
    uint32 _varianceW;
    uint32 _varianceH;

    UniformSampler _sampler;
    const TraceableScene &_scene;
    std::vector<std::unique_ptr<Integrator>> _integrators;

    std::vector<SampleRecord> _samples;
    std::vector<ImageTile> _tiles;

    uint32 _currentSpp;
    uint32 _nextSpp;

    void diceTiles();

    void advanceSpp();

    float errorPercentile95();
    void dilateAdaptiveWeights();
    void distributeAdaptiveSamples(int spp);
    bool generateWork();

    void renderTile(uint32 id, uint32 tileId);

    void writeBuffers(const std::string &suffix, bool overwrite);

public:
    Renderer(TraceableScene &scene);
    ~Renderer();

    void startRender(std::function<void()> completionCallback);
    void waitForCompletion();
    void abortRender();

    void saveOutputs();
    void saveCheckpoint();

    void saveRenderResumeData(Scene &scene);
    bool resumeRender(Scene &scene);

    bool done() const
    {
        return _currentSpp == _nextSpp;
    }

    uint32 currentSpp() const
    {
        return _currentSpp;
    }

    uint32 nextSpp() const
    {
        return _nextSpp;
    }
};

}

#endif /* RENDERER_HPP_ */

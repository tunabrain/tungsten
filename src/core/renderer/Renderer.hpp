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

    void diceTiles();

    float errorPercentile95();
    void dilateAdaptiveWeights();
    void distributeAdaptiveSamples(int spp);
    bool generateWork(uint32 sppFrom, uint32 sppTo);

    void renderTile(uint32 id, uint32 tileId);

public:
    Renderer(const TraceableScene &scene);
    ~Renderer();

    void startRender(std::function<void()> completionCallback, uint32 sppFrom, uint32 sppTo);
    void waitForCompletion();
    void abortRender();

    void getVarianceImage(std::vector<float> &data, int &w, int &h);
};

}

#endif /* RENDERER_HPP_ */

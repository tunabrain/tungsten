#ifndef PHOTONMAPINTEGRATOR_HPP_
#define PHOTONMAPINTEGRATOR_HPP_

#include "PhotonMapSettings.hpp"
#include "PhotonTracer.hpp"
#include "KdTree.hpp"
#include "Photon.hpp"

#include "integrators/Integrator.hpp"
#include "integrators/ImageTile.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"

#include "thread/TaskGroup.hpp"

#include "math/MathUtil.hpp"

#include <atomic>
#include <memory>
#include <vector>

namespace Tungsten {

class PhotonTracer;

class PhotonMapIntegrator : public Integrator
{
    static CONSTEXPR uint32 TileSize = 16;
    std::vector<ImageTile> _tiles;

    struct SubTaskData
    {
        std::unique_ptr<SampleGenerator> sampler;
        std::unique_ptr<UniformSampler> supplementalSampler;
    };

    PhotonMapSettings _settings;

    uint32 _w;
    uint32 _h;
    UniformSampler _sampler;

    std::shared_ptr<TaskGroup> _group;

    uint32 _currentPhotonCount;
    uint32 _nextPhotonCount;

    std::atomic<uint32> _totalTracedPhotons;

    int _photonOffset;
    std::vector<Photon> _photons;
    std::unique_ptr<KdTree> _tree;
    std::vector<std::unique_ptr<PhotonTracer>> _tracers;
    std::vector<SubTaskData> _taskData;

    void diceTiles();

    void advancePhotonCount();

    virtual void saveState(OutputStreamHandle &out) override;
    virtual void loadState(InputStreamHandle &in) override;

    void tracePhotons(uint32 taskId, uint32 numSubTasks, uint32 threadId);
    void tracePixels(uint32 tileId, uint32 threadId);

public:
    PhotonMapIntegrator();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void prepareForRender(TraceableScene &scene) override;
    virtual void teardownAfterRender() override;

    virtual void startRender(std::function<void()> completionCallback) override;
    virtual void waitForCompletion() override;
    virtual void abortRender() override;
};

}



#endif /* PHOTONMAPINTEGRATOR_HPP_ */

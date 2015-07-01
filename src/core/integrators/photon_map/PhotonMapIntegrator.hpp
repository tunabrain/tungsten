#ifndef PHOTONMAPINTEGRATOR_HPP_
#define PHOTONMAPINTEGRATOR_HPP_

#include "PhotonMapSettings.hpp"
#include "PhotonTracer.hpp"
#include "KdTree.hpp"
#include "Photon.hpp"

#include "integrators/Integrator.hpp"
#include "integrators/ImageTile.hpp"

#include "sampling/PathSampleGenerator.hpp"

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

    struct SubTaskData
    {
        SurfacePhotonRange surfaceRange;
        VolumePhotonRange volumeRange;
    };

    std::vector<ImageTile> _tiles;

    PhotonMapSettings _settings;

    uint32 _w;
    uint32 _h;
    UniformSampler _sampler;

    std::shared_ptr<TaskGroup> _group;

    std::atomic<uint32> _totalTracedSurfacePhotons;
    std::atomic<uint32> _totalTracedVolumePhotons;

    std::vector<Photon> _surfacePhotons;
    std::vector<VolumePhoton> _volumePhotons;

    std::unique_ptr<KdTree<Photon>> _surfaceTree;
    std::unique_ptr<KdTree<VolumePhoton>> _volumeTree;

    std::vector<std::unique_ptr<PhotonTracer>> _tracers;
    std::vector<SubTaskData> _taskData;
    std::vector<std::unique_ptr<PathSampleGenerator>> _samplers;

    void diceTiles();

    virtual void saveState(OutputStreamHandle &out) override;
    virtual void loadState(InputStreamHandle &in) override;

    void tracePhotons(uint32 taskId, uint32 numSubTasks, uint32 threadId);
    void tracePixels(uint32 tileId, uint32 threadId);

    void buildPhotonDataStructures();

public:
    PhotonMapIntegrator();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void prepareForRender(TraceableScene &scene, uint32 seed) override;
    virtual void teardownAfterRender() override;

    virtual void startRender(std::function<void()> completionCallback) override;
    virtual void waitForCompletion() override;
    virtual void abortRender() override;
};

}



#endif /* PHOTONMAPINTEGRATOR_HPP_ */

#ifndef PHOTONMAPINTEGRATOR_HPP_
#define PHOTONMAPINTEGRATOR_HPP_

#include "PhotonMapSettings.hpp"
#include "PhotonTracer.hpp"
#include "GridAccel.hpp"
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

namespace Bvh {
class BinaryBvh;
}

class PhotonTracer;

class PhotonMapIntegrator : public Integrator
{
protected:
    static CONSTEXPR uint32 TileSize = 16;

    struct SubTaskData
    {
        SurfacePhotonRange surfaceRange;
        VolumePhotonRange volumeRange;
        PathPhotonRange pathRange;
    };
    std::vector<ImageTile> _tiles;

    PhotonMapSettings _settings;

    uint32 _w;
    uint32 _h;
    UniformSampler _sampler;

    std::shared_ptr<TaskGroup> _group;
    std::unique_ptr<Ray[]> _depthBuffer;

    std::atomic<uint32> _totalTracedSurfacePaths;
    std::atomic<uint32> _totalTracedVolumePaths;
    std::atomic<uint32> _totalTracedPaths;

    std::vector<Photon> _surfacePhotons;
    std::vector<VolumePhoton> _volumePhotons;
    std::vector<PathPhoton> _pathPhotons;
    std::unique_ptr<PhotonBeam[]> _beams;
    std::unique_ptr<PhotonPlane0D[]> _planes0D;
    std::unique_ptr<PhotonPlane1D[]> _planes1D;
    uint32 _pathPhotonCount;

    std::unique_ptr<KdTree<Photon>> _surfaceTree;
    std::unique_ptr<KdTree<VolumePhoton>> _volumeTree;
    std::unique_ptr<Bvh::BinaryBvh> _volumeBvh;
    std::unique_ptr<GridAccel> _volumeGrid;

    std::vector<std::unique_ptr<PhotonTracer>> _tracers;
    std::vector<SubTaskData> _taskData;
    std::vector<std::unique_ptr<PathSampleGenerator>> _samplers;

    bool _useFrustumGrid;

    void diceTiles();

    virtual void saveState(OutputStreamHandle &out) override;
    virtual void loadState(InputStreamHandle &in) override;

    void tracePhotons(uint32 taskId, uint32 numSubTasks, uint32 threadId, uint32 sampleBase);
    void tracePixels(uint32 tileId, uint32 threadId, float surfaceRadius, float volumeRadius);

    void buildPointBvh(uint32 tail, float volumeRadiusScale);
    void buildBeamBvh(uint32 tail, float volumeRadiusScale);
    void buildBeamGrid(uint32 tail, float volumeRadiusScale);
    void buildPlaneBvh(uint32 tail, float volumeRadiusScale);
    void buildPlaneGrid(uint32 tail, float volumeRadiusScale);
    void buildPhotonDataStructures(float volumeRadiusScale);

    void renderSegment(std::function<void()> completionCallback);

public:
    PhotonMapIntegrator();
    ~PhotonMapIntegrator();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void prepareForRender(TraceableScene &scene, uint32 seed) override;
    virtual void teardownAfterRender() override;

    virtual void startRender(std::function<void()> completionCallback) override;
    virtual void waitForCompletion() override;
    virtual void abortRender() override;
};

}

#endif /* PHOTONMAPINTEGRATOR_HPP_ */

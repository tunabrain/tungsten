#include "ProgressivePhotonMapIntegrator.hpp"

#include "integrators/photon_map/PhotonTracer.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/SobolPathSampler.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

#include "bvh/BinaryBvh.hpp"

namespace Tungsten {

ProgressivePhotonMapIntegrator::ProgressivePhotonMapIntegrator()
: _iteration(0)
{
}

void ProgressivePhotonMapIntegrator::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    PhotonMapIntegrator::fromJson(v, scene);
    _progressiveSettings.fromJson(v);
}

rapidjson::Value ProgressivePhotonMapIntegrator::toJson(Allocator &allocator) const
{
    return _progressiveSettings.toJson(_settings, allocator);
}

void ProgressivePhotonMapIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _iteration = 0;
    PhotonMapIntegrator::prepareForRender(scene, seed);
}

void ProgressivePhotonMapIntegrator::renderSegment(std::function<void()> completionCallback)
{
    _totalTracedSurfacePhotons = 0;
    _totalTracedVolumePhotons  = 0;
    _totalTracedPathPhotons    = 0;

    using namespace std::placeholders;

    ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
        std::bind(&tracePhotons, this, _1, _2, _3, _iteration*_settings.photonCount),
        _tracers.size(),
        [](){}
    ));

    float gamma = 1.0f;
    for (uint32 i = 1; i <= _iteration; ++i)
        gamma *= (i + _progressiveSettings.alpha)/(i + 1.0f);

    float gamma1D = gamma;
    float gamma2D = std::sqrt(gamma);
    float gamma3D = std::cbrt(gamma);

    float surfaceRadius = _settings.gatherRadius*gamma2D;
    float volumeRadius = _settings.volumeGatherRadius;
    if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_POINTS)
        volumeRadius *= gamma3D;
    else
        volumeRadius *= gamma1D;

    buildPhotonDataStructures(gamma3D);

    ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
        std::bind(&ProgressivePhotonMapIntegrator::tracePixels, this, _1, _3, surfaceRadius, volumeRadius),
        _tiles.size(),
        [](){}
    ));

    _currentSpp = _nextSpp;
    advanceSpp();
    _iteration++;

    _surfaceTree.reset();
    _volumeTree.reset();
    _beamBvh.reset();
    for (SubTaskData &data : _taskData) {
        data.surfaceRange.reset();
        data.volumeRange.reset();
        data.pathRange.reset();
    }

    completionCallback();
}

void ProgressivePhotonMapIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    _group = ThreadUtils::pool->enqueue([&, completionCallback](uint32, uint32, uint32) {
        renderSegment(completionCallback);
    }, 1, [](){});
}

}

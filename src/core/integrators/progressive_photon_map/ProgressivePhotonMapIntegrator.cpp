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

void ProgressivePhotonMapIntegrator::fromJson(JsonPtr value, const Scene &scene)
{
    PhotonMapIntegrator::fromJson(value, scene);
    _progressiveSettings.fromJson(value);
}

rapidjson::Value ProgressivePhotonMapIntegrator::toJson(Allocator &allocator) const
{
    return _progressiveSettings.toJson(_settings, allocator);
}

void ProgressivePhotonMapIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _iteration = 0;
    PhotonMapIntegrator::prepareForRender(scene, seed);

    for (size_t i = 0; i < _tracers.size(); ++i)
        _shadowSamplers.emplace_back(_sampler.nextI());
}

void ProgressivePhotonMapIntegrator::renderSegment(std::function<void()> completionCallback)
{
    _totalTracedSurfacePaths = 0;
    _totalTracedVolumePaths  = 0;
    _totalTracedPaths        = 0;
    _pathPhotonCount         = 0;
    _scene->cam().setSplatWeight(1.0/_nextSpp);

    using namespace std::placeholders;

    ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
        std::bind(&ProgressivePhotonMapIntegrator::tracePhotons, this, _1, _2, _3, _iteration*_settings.photonCount),
        _tracers.size(),
        [](){}
    ));

    float gamma = 1.0f;
    for (uint32 i = 1; i <= _iteration; ++i)
        gamma *= (i + _progressiveSettings.alpha)/(i + 1.0f);

    float gamma1D = gamma;
    float gamma2D = std::sqrt(gamma);
    float gamma3D = std::cbrt(gamma);

    float volumeScale;
    if (_settings.volumePhotonType == PhotonMapSettings::VOLUME_POINTS)
        volumeScale = gamma3D;
    else
        volumeScale = gamma1D;

    float surfaceRadius = _settings.gatherRadius*gamma2D;
    float volumeRadius = _settings.volumeGatherRadius*volumeScale;

    buildPhotonDataStructures(volumeScale);

    ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
        std::bind(&ProgressivePhotonMapIntegrator::tracePixels, this, _1, _3, surfaceRadius, volumeRadius),
        _tiles.size(),
        [](){}
    ));
    if (_useFrustumGrid) {
        ThreadUtils::pool->yield(*ThreadUtils::pool->enqueue(
            [&](uint32 tracerId, uint32 numTracers, uint32) {
                uint32 start = intLerp(0, _pathPhotonCount, tracerId,     numTracers);
                uint32 end   = intLerp(0, _pathPhotonCount, tracerId + 1, numTracers);
                _tracers[tracerId]->evalPrimaryRays(_beams.get(), _planes0D.get(), _planes1D.get(),
                        start, end, volumeRadius, _depthBuffer.get(), *_samplers[tracerId], _nextSpp - _currentSpp);
            }, _tracers.size(), [](){}
        ));
    }

    _currentSpp = _nextSpp;
    advanceSpp();
    _iteration++;

    _beams.reset();
    _planes0D.reset();
    _planes1D.reset();
    _surfaceTree.reset();
    _volumeTree.reset();
    _volumeGrid.reset();
    _volumeBvh.reset();
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

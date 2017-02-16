#include "KelemenMltIntegrator.hpp"

#include "integrators/bidirectional_path_tracer/ImagePyramid.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/SobolPathSampler.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

namespace Tungsten {

KelemenMltIntegrator::KelemenMltIntegrator()
: Integrator(),
  _w(0),
  _h(0),
  _sampler(0xBA5EBA11)
{
}

void KelemenMltIntegrator::saveState(OutputStreamHandle &/*out*/)
{
}

void KelemenMltIntegrator::loadState(InputStreamHandle &/*in*/)
{
}

void KelemenMltIntegrator::fromJson(JsonPtr value, const Scene &/*scene*/)
{
    _settings.fromJson(value);
}

rapidjson::Value KelemenMltIntegrator::toJson(Allocator &allocator) const
{
    return _settings.toJson(allocator);
}

void KelemenMltIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _chainsLaunched = false;
    _currentSpp = 0;
    _sampler = UniformSampler(MathUtil::hash32(seed), ThreadUtils::pool->threadCount());
    _scene = &scene;
    advanceSpp();

    _w = scene.cam().resolution().x();
    _h = scene.cam().resolution().y();
    scene.cam().requestColorBuffer();
    scene.cam().requestSplatBuffer();

    if (_settings.imagePyramid)
        _imagePyramid.reset(new ImagePyramid(_settings.maxBounces, _scene->cam()));

    for (uint32 i = 0; i < ThreadUtils::pool->threadCount(); ++i)
        _tracers.emplace_back(new KelemenMltTracer(&scene, _settings, _sampler.state(), i, _imagePyramid.get()));
}

void KelemenMltIntegrator::teardownAfterRender()
{
    _group.reset();

    _tracers.clear();
    _tracers.shrink_to_fit();
}

void KelemenMltIntegrator::traceSamplePool(uint32 taskId, uint32 numSubTasks, uint32 /*threadId*/)
{
    uint32 rayBase = intLerp(0, _settings.initialSamplePool, taskId + 0, numSubTasks);
    uint32 rayTail = intLerp(0, _settings.initialSamplePool, taskId + 1, numSubTasks);

    std::unique_ptr<SplatQueue> queue(new SplatQueue(sqr(_settings.maxBounces + 2)));

    UniformPathSampler pathSampler(_tracers[taskId]->sampler());
    for (uint32 i = rayBase; i < rayTail; ++i) {
        _pathCandidates[i].state = pathSampler.sampler().state();
        _tracers[taskId]->tracePath(pathSampler, pathSampler, *queue, false);

        _pathCandidates[i].luminanceSum = queue->totalLuminance();
        _pathCandidates[i].luminance = _pathCandidates[i].luminanceSum;
        if (std::isnan(_pathCandidates[i].luminanceSum))
            _pathCandidates[i].luminanceSum = 0.0f;

        queue->apply(*_scene->cam().splatBuffer(), 1.0f);
    }

    _tracers[taskId]->sampler() = pathSampler.sampler();
}

void KelemenMltIntegrator::runSampleChain(uint32 taskId, uint32 numSubTasks, uint32 /*threadId*/)
{
    uint32 rayCount = _w*_h*(_nextSpp - _currentSpp);

    uint32 rayBase    = intLerp(0, rayCount, taskId + 0, numSubTasks);
    uint32 raysToCast = intLerp(0, rayCount, taskId + 1, numSubTasks) - rayBase;

    _tracers[taskId]->runSampleChain(raysToCast, _luminanceScale);
}

void KelemenMltIntegrator::selectSeedPaths()
{
    for (int i = 1; i < _settings.initialSamplePool; ++i)
        _pathCandidates[i].luminanceSum += _pathCandidates[i - 1].luminanceSum;

    double totalLuminance = _pathCandidates[_settings.initialSamplePool - 1].luminanceSum;
    for (size_t i = 0; i < _tracers.size(); ++i) {
        double target = _sampler.next1D()*totalLuminance;

        int idx;
        for (idx = 0; idx < _settings.initialSamplePool - 1; ++idx)
            if (target < _pathCandidates[idx].luminanceSum)
                break;

        uint64 sequence = _tracers[(idx*_tracers.size())/_settings.initialSamplePool]->sampler().sequence();
        UniformSampler replaySampler(_pathCandidates[idx].state, sequence);
        _tracers[i]->startSampleChain(replaySampler, _pathCandidates[idx].luminance);
    }

    _scene->cam().blitSplatBuffer();
    _luminanceScale = totalLuminance/_settings.initialSamplePool;
    _pathCandidates.reset();
}

void KelemenMltIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    double weight = double(_w*_h)/(_w*_h*_nextSpp + _settings.initialSamplePool);
    _scene->cam().setColorBufferWeight(weight);
    _scene->cam().setSplatWeight(weight);

    using namespace std::placeholders;
    if (!_chainsLaunched) {
        _pathCandidates.reset(new PathCandidate[_settings.initialSamplePool]);

        _group = ThreadUtils::pool->enqueue(
            std::bind(&KelemenMltIntegrator::traceSamplePool, this, _1, _2, _3),
            _tracers.size(),
            [&, completionCallback]() {
                selectSeedPaths();
                _chainsLaunched = true;
                completionCallback();
            }
        );
    } else {
        _group = ThreadUtils::pool->enqueue(
            std::bind(&KelemenMltIntegrator::runSampleChain, this, _1, _2, _3),
            _tracers.size(),
            [&, completionCallback]() {
                _currentSpp = _nextSpp;
                advanceSpp();
                completionCallback();
            }
        );
    }
}

void KelemenMltIntegrator::waitForCompletion()
{
    if (_group) {
        _group->wait();
        _group.reset();
    }
}

void KelemenMltIntegrator::abortRender()
{
    if (_group) {
        _group->abort();
        _group->wait();
        _group.reset();
    }
}

void KelemenMltIntegrator::saveOutputs()
{
    Integrator::saveOutputs();
    if (_imagePyramid)
        _imagePyramid->saveBuffers(_scene->rendererSettings().outputFile().stripExtension(), _scene->rendererSettings().spp(), true);
}

}

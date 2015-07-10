#include "KelemenMltIntegrator.hpp"

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
    FAIL("KelemenMltIntegrator::saveState not supported!");
}

void KelemenMltIntegrator::loadState(InputStreamHandle &/*in*/)
{
    FAIL("KelemenMltIntegrator::loadState not supported!");
}

void KelemenMltIntegrator::fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
{
    _settings.fromJson(v);
}

rapidjson::Value KelemenMltIntegrator::toJson(Allocator &allocator) const
{
    return _settings.toJson(allocator);
}

void KelemenMltIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _currentSpp = 0;
    _sampler = UniformSampler(MathUtil::hash32(seed));
    _scene = &scene;
    advanceSpp();

    _w = scene.cam().resolution().x();
    _h = scene.cam().resolution().y();
    scene.cam().requestSplatBuffer();

    for (uint32 i = 0; i < ThreadUtils::pool->threadCount(); ++i)
        _tracers.emplace_back(new KelemenMltTracer(&scene, _settings, i));
}

void KelemenMltIntegrator::teardownAfterRender()
{
    _group.reset();

    _tracers.clear();
    _tracers.shrink_to_fit();
}

void KelemenMltIntegrator::traceRays(uint32 taskId, uint32 numSubTasks, uint32 /*threadId*/)
{
    uint32 rayCount = _w*_h*(_nextSpp - _currentSpp);

    uint32 rayBase    = intLerp(0, rayCount, taskId + 0, numSubTasks);
    uint32 raysToCast = intLerp(0, rayCount, taskId + 1, numSubTasks) - rayBase;

    _tracers[taskId]->startSampleChain(raysToCast);
}

void KelemenMltIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    using namespace std::placeholders;
    _group = ThreadUtils::pool->enqueue(
        std::bind(&KelemenMltIntegrator::traceRays, this, _1, _2, _3),
        _tracers.size(),
        [&, completionCallback]() {
            _scene->cam().blitSplatBuffer(_nextSpp - _currentSpp);
            _currentSpp = _nextSpp;
            advanceSpp();
            completionCallback();
        }
    );
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

}

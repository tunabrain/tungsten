#include "LightTraceIntegrator.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/SobolPathSampler.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

namespace Tungsten {

LightTraceIntegrator::LightTraceIntegrator()
: Integrator(),
  _w(0),
  _h(0),
  _sampler(0xBA5EBA11)
{
}

void LightTraceIntegrator::traceRays(uint32 taskId, uint32 numSubTasks, uint32 threadId)
{
    PathSampleGenerator &sampler = *_taskData[taskId];
    uint32 rayCount = _w*_h*(_nextSpp - _currentSpp);

    uint32 rayBase    = intLerp(0, rayCount, taskId + 0, numSubTasks);
    uint32 raysToCast = intLerp(0, rayCount, taskId + 1, numSubTasks) - rayBase;

    for (uint32 i = 0; i < raysToCast; ++i) {
        sampler.startPath(0, _currentSpp*_w*_h + rayBase + i);
        _tracers[threadId]->traceSample(sampler);
    }
}

void LightTraceIntegrator::saveState(OutputStreamHandle &/*out*/)
{
}

void LightTraceIntegrator::loadState(InputStreamHandle &/*in*/)
{
}

void LightTraceIntegrator::fromJson(JsonPtr value, const Scene &/*scene*/)
{
    _settings.fromJson(value);
}

rapidjson::Value LightTraceIntegrator::toJson(Allocator &allocator) const
{
    return _settings.toJson(allocator);
}

void LightTraceIntegrator::prepareForRender(TraceableScene &scene, uint32 seed)
{
    _currentSpp = 0;
    _sampler = UniformSampler(MathUtil::hash32(seed));
    _scene = &scene;
    advanceSpp();

    _w = scene.cam().resolution().x();
    _h = scene.cam().resolution().y();
    scene.cam().requestSplatBuffer();

    for (uint32 i = 0; i < ThreadUtils::pool->threadCount(); ++i) {
        _taskData.emplace_back(_scene->rendererSettings().useSobol() ?
            std::unique_ptr<PathSampleGenerator>(new SobolPathSampler(MathUtil::hash32(_sampler.nextI()))) :
            std::unique_ptr<PathSampleGenerator>(new UniformPathSampler(MathUtil::hash32(_sampler.nextI())))
        );

        _tracers.emplace_back(new LightTracer(&scene, _settings, i));
    }
}

void LightTraceIntegrator::teardownAfterRender()
{
    _group.reset();

    _tracers.clear();
    _tracers.shrink_to_fit();
}

void LightTraceIntegrator::startRender(std::function<void()> completionCallback)
{
    if (done()) {
        completionCallback();
        return;
    }

    _scene->cam().setSplatWeight(1.0/(_w*_h*_nextSpp));

    using namespace std::placeholders;
    _group = ThreadUtils::pool->enqueue(
        std::bind(&LightTraceIntegrator::traceRays, this, _1, _2, _3),
        _tracers.size(),
        [&, completionCallback]() {
            _currentSpp = _nextSpp;
            advanceSpp();
            completionCallback();
        }
    );
}

void LightTraceIntegrator::waitForCompletion()
{
    if (_group) {
        _group->wait();
        _group.reset();
    }
}

void LightTraceIntegrator::abortRender()
{
    if (_group) {
        _group->abort();
        _group->wait();
        _group.reset();
    }
}

}

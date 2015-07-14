#include "KelemenMltTracer.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/UniformSampler.hpp"

namespace Tungsten {

KelemenMltTracer::KelemenMltTracer(TraceableScene *scene, const KelemenMltSettings &settings, uint32 threadId)
: PathTracer(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _settings(settings),
  _sampler(MathUtil::hash32(threadId)),
  _currentSplats(new SplatQueue(_settings.maxBounces + 4)),
  _proposedSplats(new SplatQueue(_settings.maxBounces + 4)),
  _pathCandidates(new PathCandidate[settings.initialSamplePool]),
  _cameraPath(new LightPath(settings.maxBounces + 1)),
  _emitterPath(new LightPath(settings.maxBounces + 1))
{
}

void KelemenMltTracer::tracePath(PathSampleGenerator &cameraSampler, PathSampleGenerator &emitterSampler,
        SplatQueue &splatQueue)
{
    splatQueue.clear();

    Vec2f resF(_scene->cam().resolution());
    Vec2u pixel = min(Vec2u(resF*cameraSampler.next2D(CameraSample)), _scene->cam().resolution());

    if (!_settings.bidirectional) {
        splatQueue.addSplat(pixel, traceSample(pixel, cameraSampler));
        return;
    }

    LightPath & cameraPath = * _cameraPath;
    LightPath &emitterPath = *_emitterPath;

    float lightPdf;
    const Primitive *light = chooseLightAdjoint(emitterSampler, lightPdf);

    float lightSplatScale = 1.0f/resF.product();

    cameraPath.startCameraPath(&_scene->cam(), pixel);
    emitterPath.startEmitterPath(light, lightPdf);

     cameraPath.tracePath(*_scene, *this,  cameraSampler);
    emitterPath.tracePath(*_scene, *this, emitterSampler);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    Vec3f primarySplat = cameraPath.bdptWeightedPathEmission(_settings.minBounces + 2, _settings.maxBounces + 1);
    for (int s = 1; s <= lightLength; ++s) {
        int upperBound = min(_settings.maxBounces - s + 1, cameraLength);
        for (int t = 1; t <= upperBound; ++t) {
            if (!cameraPath[t - 1].connectable() || !emitterPath[s - 1].connectable())
                continue;

            if (t == 1) {
                Vec2f pixel;
                Vec3f splatWeight;
                if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, emitterSampler, splatWeight, pixel))
                    splatQueue.addFilteredSplat(pixel, splatWeight*lightSplatScale);
            } else {
                primarySplat += LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces);
            }
        }
    }
    splatQueue.addSplat(pixel, primarySplat);
}

void KelemenMltTracer::selectSeedPath(int &idx, float &weight)
{
    UniformPathSampler pathSampler(_sampler);
    for (int i = 0; i < _settings.initialSamplePool; ++i) {
        _pathCandidates[i].state = pathSampler.sampler().state();

        tracePath(pathSampler, pathSampler, *_currentSplats);

        _pathCandidates[i].luminanceSum = _currentSplats->totalLuminance();

        if (i > 0)
            _pathCandidates[i].luminanceSum += _pathCandidates[i - 1].luminanceSum;
    }
    _sampler = pathSampler.sampler();

    float totalSum = _pathCandidates[_settings.initialSamplePool - 1].luminanceSum;
    float target = totalSum*_sampler.next1D();
    weight = totalSum/_settings.initialSamplePool;

    for (int i = 0; i < _settings.initialSamplePool; ++i) {
        if (target < _pathCandidates[i].luminanceSum || i == _settings.initialSamplePool - 1) {
            idx = i;
            break;
        }
    }
}

void KelemenMltTracer::startSampleChain(int chainLength)
{
    int idx;
    float weight;
    selectSeedPath(idx, weight);

    UniformSampler replaySampler(_pathCandidates[idx].state, _sampler.sequence());
    MetropolisSampler cameraSampler(&replaySampler, _settings.maxBounces*16);
    MetropolisSampler emitterSampler(&replaySampler, _settings.maxBounces*16);

    tracePath(cameraSampler, emitterSampler, *_currentSplats);

    cameraSampler.accept();
    emitterSampler.accept();
    cameraSampler.setHelperGenerator(&_sampler);
    emitterSampler.setHelperGenerator(&_sampler);

    float accumulatedWeight = 0.0f;
    for (int i = 1; i < chainLength; ++i) {
        bool largeStep = _sampler.next1D() < _settings.largeStepProbability;
        cameraSampler.setLargeStep(largeStep);
        emitterSampler.setLargeStep(largeStep);

        tracePath(cameraSampler, emitterSampler, *_proposedSplats);

        float currentI = _currentSplats->totalLuminance();
        float proposedI = _proposedSplats->totalLuminance();

        float a = currentI == 0.0f ? 1.0f : min(proposedI/currentI, 1.0f);

        float currentWeight = (1.0f - a)/((currentI/weight) + _settings.largeStepProbability);
        float proposedWeight = (a + (largeStep ? 1.0f : 0.0f))/((proposedI/weight) + _settings.largeStepProbability);

        accumulatedWeight += currentWeight;

        if (_sampler.next1D() < a) {
            if (currentI != 0.0f)
                _currentSplats->apply(*_splatBuffer, accumulatedWeight);

            std::swap(_currentSplats, _proposedSplats);
            accumulatedWeight = proposedWeight;

            cameraSampler.accept();
            emitterSampler.accept();
        } else {
            if (proposedI != 0.0f)
                _proposedSplats->apply(*_splatBuffer, proposedWeight);

            cameraSampler.reject();
            emitterSampler.reject();
        }
    }
}

}

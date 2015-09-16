#include "KelemenMltTracer.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/UniformSampler.hpp"

namespace Tungsten {

KelemenMltTracer::KelemenMltTracer(TraceableScene *scene, const KelemenMltSettings &settings, uint64 seed,
        uint32 threadId)
: PathTracer(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _settings(settings),
  _sampler(seed, threadId),
  _currentSplats(new SplatQueue(sqr(_settings.maxBounces + 2))),
  _proposedSplats(new SplatQueue(sqr(_settings.maxBounces + 2))),
  _cameraPath(new LightPath(settings.maxBounces + 1)),
  _emitterPath(new LightPath(settings.maxBounces + 1))
{
}

void KelemenMltTracer::tracePath(PathSampleGenerator &cameraSampler, PathSampleGenerator &emitterSampler,
        SplatQueue &splatQueue)
{
    splatQueue.clear();

    Vec2f resF(_scene->cam().resolution());
    Vec2u pixel = min(Vec2u(resF*cameraSampler.next2D()), _scene->cam().resolution());

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
                primarySplat += LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces, cameraSampler);
            }
        }
    }
    splatQueue.addSplat(pixel, primarySplat);
}

void KelemenMltTracer::startSampleChain(UniformSampler &replaySampler, float luminance)
{
    _cameraSampler.reset (new MetropolisSampler(&replaySampler, _settings.maxBounces*16));
    _emitterSampler.reset(new MetropolisSampler(&replaySampler, _settings.maxBounces*16));

    tracePath(*_cameraSampler, *_emitterSampler, *_currentSplats);

    if (_currentSplats->totalLuminance() != luminance)
        FAIL("Oh no! Terrible things have occurred! %f != %f", _currentSplats->totalLuminance(), luminance);

    _cameraSampler->accept();
    _emitterSampler->accept();
    _cameraSampler->setHelperGenerator(&_sampler);
    _emitterSampler->setHelperGenerator(&_sampler);
}

void KelemenMltTracer::runSampleChain(int chainLength, float luminanceScale)
{
    float accumulatedWeight = 0.0f;
    for (int i = 0; i < chainLength; ++i) {
        bool largeStep = _sampler.next1D() < _settings.largeStepProbability;
        _cameraSampler->setLargeStep(largeStep);
        _emitterSampler->setLargeStep(largeStep);

        tracePath(*_cameraSampler, *_emitterSampler, *_proposedSplats);

        float currentI = _currentSplats->totalLuminance();
        float proposedI = _proposedSplats->totalLuminance();

        float a = currentI == 0.0f ? 1.0f : min(proposedI/currentI, 1.0f);

        float currentWeight = (1.0f - a)/((currentI/luminanceScale) + _settings.largeStepProbability);
        float proposedWeight = (a + (largeStep ? 1.0f : 0.0f))/((proposedI/luminanceScale) + _settings.largeStepProbability);

        accumulatedWeight += currentWeight;

        if (_sampler.next1D() < a) {
            if (currentI != 0.0f)
                _currentSplats->apply(*_splatBuffer, accumulatedWeight);

            std::swap(_currentSplats, _proposedSplats);
            accumulatedWeight = proposedWeight;

            _cameraSampler->accept();
            _emitterSampler->accept();
        } else {
            if (proposedI != 0.0f)
                _proposedSplats->apply(*_splatBuffer, proposedWeight);

            _cameraSampler->reject();
            _emitterSampler->reject();
        }
    }
}

}

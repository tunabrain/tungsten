#include "KelemenMltTracer.hpp"

#include "integrators/bidirectional_path_tracer/ImagePyramid.hpp"

#include "sampling/UniformPathSampler.hpp"
#include "sampling/UniformSampler.hpp"

namespace Tungsten {

KelemenMltTracer::KelemenMltTracer(TraceableScene *scene, const KelemenMltSettings &settings, uint64 seed,
        uint32 threadId, ImagePyramid *imagePyramid)
: PathTracer(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _imagePyramid(imagePyramid),
  _settings(settings),
  _sampler(seed, threadId),
  _currentSplats(new SplatQueue(sqr(_settings.maxBounces + 2))),
  _proposedSplats(new SplatQueue(sqr(_settings.maxBounces + 2))),
  _cameraPath(new LightPath(settings.maxBounces + 1)),
  _emitterPath(new LightPath(settings.maxBounces + 1))
{
    if (_imagePyramid)
        _directEmissionByBounce.reset(new Vec3f[settings.maxBounces + 2]);
}

void KelemenMltTracer::tracePath(PathSampleGenerator &cameraSampler, PathSampleGenerator &emitterSampler,
        SplatQueue &splatQueue, bool record)
{
    splatQueue.clear();

    Vec2f resF(_scene->cam().resolution());
    Vec2u pixel = min(Vec2u(resF*cameraSampler.next2D()), _scene->cam().resolution());

    if (!_settings.bidirectional) {
        splatQueue.addSplat(0, 0, pixel, traceSample(pixel, cameraSampler));
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

    bool splitPrimaries = record && _imagePyramid;

    Vec3f primarySplat = cameraPath.bdptWeightedPathEmission(_settings.minBounces + 2, _settings.maxBounces + 1,
            nullptr, splitPrimaries ? _directEmissionByBounce.get() : nullptr);
    for (int s = 1; s <= lightLength; ++s) {
        int upperBound = min(_settings.maxBounces - s + 1, cameraLength);
        for (int t = 1; t <= upperBound; ++t) {
            if (!cameraPath[t - 1].connectable() || !emitterPath[s - 1].connectable())
                continue;

            if (t == 1) {
                Vec2f pixel;
                Vec3f splatWeight;
                if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, emitterSampler, splatWeight, pixel))
                    splatQueue.addFilteredSplat(s, t, pixel, splatWeight*lightSplatScale);
            } else {
                Vec3f v = LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces, cameraSampler);
                if (splitPrimaries)
                    splatQueue.addSplat(s, t, pixel, v);
                else
                    primarySplat += v;
            }
        }
    }
    if (splitPrimaries)
        for (int t = 2; t <= cameraPath.length(); ++t)
            splatQueue.addSplat(0, t, pixel, _directEmissionByBounce[t - 2]);
    else
        splatQueue.addSplat(0, 0, pixel, primarySplat);
}

void KelemenMltTracer::startSampleChain(UniformSampler &replaySampler, float luminance)
{
    _cameraSampler.reset (new MetropolisSampler(&replaySampler, _settings.maxBounces*16));
    _emitterSampler.reset(new MetropolisSampler(&replaySampler, _settings.maxBounces*16));

    tracePath(*_cameraSampler, *_emitterSampler, *_currentSplats, false);

    if (_currentSplats->totalLuminance() != luminance)
        FAIL("Underlying integrator is not consistent. Expected a value of %f, but received %f", luminance, _currentSplats->totalLuminance());

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

        tracePath(*_cameraSampler, *_emitterSampler, *_proposedSplats, true);

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

        if (_imagePyramid)
            _currentSplats->apply(*_imagePyramid, luminanceScale/_currentSplats->totalLuminance());
    }

    if (_currentSplats->totalLuminance() != 0.0f)
        _currentSplats->apply(*_scene->cam().splatBuffer(), accumulatedWeight/_currentSplats->totalLuminance());
}

}

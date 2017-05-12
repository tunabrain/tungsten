#include "MultiplexedMltTracer.hpp"

#include "sampling/UniformSampler.hpp"

namespace Tungsten {

MultiplexedMltTracer::MultiplexedMltTracer(TraceableScene *scene, const MultiplexedMltSettings &settings, uint32 threadId,
        UniformSampler &sampler, ImagePyramid *pyramid)
: TraceBase(scene, settings, threadId),
  _settings(settings),
  _sampler(sampler.state(), threadId*3 + 0),
  _cameraSampler(UniformSampler(sampler.state(), threadId*3 + 1)),
  _emitterSampler(UniformSampler(sampler.state(), threadId*3 + 2)),
  _chains(new MarkovChain[_settings.maxBounces + 1]),
  _lightSplatScale(1.0f/(_scene->cam().resolution().x()*_scene->cam().resolution().y())),
  _pyramid(pyramid)
{
}

void MultiplexedMltTracer::tracePaths(LightPath & cameraPath, PathSampleGenerator & cameraSampler,
                                      LightPath &emitterPath, PathSampleGenerator &emitterSampler,
                                      int s, int t)
{
    if (t == -1)
        t = _settings.maxBounces + 1;
    if (s == -1)
        s = _settings.maxBounces;

    cameraPath.clear();
    emitterPath.clear();

    if (t > 0) {
        Vec2f resF(_scene->cam().resolution());
        Vec2u pixel = min(Vec2u(resF*cameraSampler.next2D()), _scene->cam().resolution());
        cameraPath.startCameraPath(&_scene->cam(), pixel);
        cameraPath.tracePath(*_scene, *this,  cameraSampler, t);
    }
    if (s > 0) {
        float lightPdf;
        const Primitive *light = chooseLightAdjoint(emitterSampler, lightPdf);
        emitterPath.startEmitterPath(light, lightPdf);
        emitterPath.tracePath(*_scene, *this, emitterSampler, s);
    }
}

int MultiplexedMltTracer::evalSample(LightPath & cameraPath, PathSampleGenerator & cameraSampler,
                                     LightPath &emitterPath, PathSampleGenerator &emitterSampler,
                                     int length, SplatQueue &queue)
{
    queue.clear();

    int s = int(emitterSampler.next1D()*(length + 1));
    int t = length + 1 - s;

    tracePaths(cameraPath, cameraSampler, emitterPath, emitterSampler, s, t);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    if (cameraLength != t || lightLength != s)
        return s;

    if (s == 0) {
        Vec2u pixel = cameraPath[0].cameraRecord().pixel;
        Vec3f v = cameraPath.bdptWeightedPathEmission(t, t);
        queue.addSplat(s, t, pixel, v);
    } else if (t == 1) {
        Vec2f pixel;
        Vec3f splatWeight;
        if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, emitterSampler, splatWeight, pixel))
            queue.addFilteredSplat(s, t, pixel, splatWeight*_lightSplatScale);
    } else {
        Vec2u pixel = cameraPath[0].cameraRecord().pixel;
        Vec3f v = LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces, cameraSampler);
        queue.addSplat(s, t, pixel, v);
    }

    return s;
}

void MultiplexedMltTracer::traceCandidatePath(LightPath &cameraPath, LightPath &emitterPath,
        SplatQueue &queue, const std::function<void(Vec3f, int, int)> &addCandidate)
{
    tracePaths(cameraPath, _cameraSampler, emitterPath, _emitterSampler);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    for (int s = 0; s <= lightLength; ++s) {
        int upperBound = min(_settings.maxBounces - s + 1, cameraLength);
        for (int t = 1; t <= upperBound; ++t) {
            if (!cameraPath[t - 1].connectable() || (s > 0 && !emitterPath[s - 1].connectable()))
                continue;

            if (s == 0) {
                if (t - 2 < _settings.minBounces || t - 2 >= _settings.maxBounces)
                    continue;
                Vec3f v = cameraPath.bdptWeightedPathEmission(t, t);
                queue.addSplat(0, t, cameraPath[0].cameraRecord().pixel, v);
                addCandidate(v, 0, t);
            } else if (t == 1) {
                Vec2f pixel;
                Vec3f splatWeight;
                if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, _emitterSampler, splatWeight, pixel)) {
                    queue.addFilteredSplat(s, t, pixel, splatWeight*_lightSplatScale);
                    addCandidate(splatWeight*_lightSplatScale, s, t);
                }
            } else {
                Vec3f v = LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces, _cameraSampler);
                queue.addSplat(s, t, cameraPath[0].cameraRecord().pixel, v);
                addCandidate(v, s, t);
            }
        }
    }
}

void MultiplexedMltTracer::startSampleChain(int s, int t, float luminance, UniformSampler &cameraReplaySampler,
        UniformSampler &emitterReplaySampler)
{
    int length = s + t - 1;

    MarkovChain &chain = _chains[length];
    chain.currentSplats.reset(new SplatQueue(1));
    chain.proposedSplats.reset(new SplatQueue(1));
    chain.cameraPath.reset(new LightPath(length + 1));
    chain.emitterPath.reset(new LightPath(length));
    chain.cameraSampler .reset(new MetropolisSampler( &cameraReplaySampler, (length + 1)*16));
    chain.emitterSampler.reset(new MetropolisSampler(&emitterReplaySampler, (length + 1)*16));
    chain.currentS = s;

    chain.emitterSampler->setRandomElement(0, (s + 0.5f)/(length + 1.0f));
    evalSample(*chain.cameraPath, *chain.cameraSampler, *chain.emitterPath, *chain.emitterSampler,
            length, *chain.currentSplats);

    chain.cameraSampler ->accept();
    chain.emitterSampler->accept();
    chain.cameraSampler ->setHelperGenerator(&_sampler);
    chain.emitterSampler->setHelperGenerator(&_sampler);

    if (chain.currentSplats->totalLuminance() != luminance)
        FAIL("Underlying integrator is not consistent. Expected a value of %f, but received %f", luminance, chain.currentSplats->totalLuminance());
}

LargeStepTracker MultiplexedMltTracer::runSampleChain(int pathLength, int chainLength,
        MultiplexedStats &stats, float luminanceScale)
{
    MarkovChain &chain = _chains[pathLength];
    MetropolisSampler & cameraSampler = *chain. cameraSampler;
    MetropolisSampler &emitterSampler = *chain.emitterSampler;
    LightPath & cameraPath = *chain. cameraPath;
    LightPath &emitterPath = *chain.emitterPath;
    std::unique_ptr<SplatQueue> & currentSplats = chain. currentSplats;
    std::unique_ptr<SplatQueue> &proposedSplats = chain.proposedSplats;
    int &currentS = chain.currentS;

    LargeStepTracker largeSteps;

    float accumulatedWeight = 0.0f;
    for (int i = 0; i < chainLength; ++i) {
        bool largeStep = _sampler.next1D() < _settings.largeStepProbability;
        cameraSampler.setLargeStep(largeStep);
        emitterSampler.setLargeStep(largeStep);

        int proposedS = evalSample(cameraPath, cameraSampler, emitterPath, emitterSampler, pathLength, *proposedSplats);

        float currentI = currentSplats->totalLuminance();
        float proposedI = proposedSplats->totalLuminance();
        if (std::isnan(proposedI))
            proposedI = 0.0f;

        if (largeStep)
            largeSteps.add(proposedI*(pathLength + 1));

        float a = currentI == 0.0f ? 1.0f : min(proposedI/currentI, 1.0f);

        float currentWeight = (1.0f - a);
        float proposedWeight = a;

        accumulatedWeight += currentWeight;

        if (_sampler.next1D() < a) {
            if (currentI != 0.0f)
                currentSplats->apply(*_scene->cam().splatBuffer(), accumulatedWeight/currentI);

            std::swap(currentSplats, proposedSplats);
            accumulatedWeight = proposedWeight;

            cameraSampler.accept();
            emitterSampler.accept();

            if (largeStep)
                stats.largeStep().accept(pathLength);
            else if (currentS != proposedS)
                stats.techniqueChange().accept(pathLength);
            else
                stats.smallStep().accept(pathLength);

            currentS = proposedS;
        } else {
            if (proposedI != 0.0f)
                proposedSplats->apply(*_scene->cam().splatBuffer(), proposedWeight/proposedI);

            cameraSampler.reject();
            emitterSampler.reject();

            if (largeStep)
                stats.largeStep().reject(pathLength);
            else if (currentS != proposedS)
                stats.techniqueChange().reject(pathLength);
            else
                stats.smallStep().reject(pathLength);
        }

        if (_pyramid)
            currentSplats->apply(*_pyramid, luminanceScale/currentSplats->totalLuminance());
    }

    if (currentSplats->totalLuminance() != 0.0f)
        currentSplats->apply(*_scene->cam().splatBuffer(), accumulatedWeight/currentSplats->totalLuminance());

    return largeSteps;
}

}

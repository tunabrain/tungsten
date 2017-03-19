#include "ReversibleJumpMltTracer.hpp"

#include "sampling/UniformSampler.hpp"

namespace Tungsten {

ReversibleJumpMltTracer::ReversibleJumpMltTracer(TraceableScene *scene, const ReversibleJumpMltSettings &settings, uint32 threadId,
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

void ReversibleJumpMltTracer::tracePaths(
        LightPath & cameraPath, PathSampleGenerator & cameraSampler,
        LightPath &emitterPath, PathSampleGenerator &emitterSampler,
        int s, int t, bool traceCamera, bool traceEmitter)
{
    if (traceCamera) {
        if (t == -1)
            t = _settings.maxBounces + 1;
        cameraPath.clear();
        cameraPath.startCameraPath(&_scene->cam());
        cameraSampler.startPath(0, 0);
        if (t > 0)
            cameraPath.tracePath(*_scene, *this,  cameraSampler, t);
    }

    if (traceEmitter) {
        if (s == -1)
            s = _settings.maxBounces;
        emitterPath.clear();
        emitterPath.startEmitterPath(_scene->lights()[0].get(), 1.0f);
        emitterSampler.startPath(0, 0);
        if (s > 0)
            emitterPath.tracePath(*_scene, *this, emitterSampler, s);
    }
}

void ReversibleJumpMltTracer::evalSample(WritableMetropolisSampler &cameraSampler, WritableMetropolisSampler &emitterSampler,
        int length, int s, ChainState &state)
{
    state.splats.clear();

    int t = length + 1 - s;

    LightPath & cameraPath = state.cameraPath;
    LightPath &emitterPath = state.emitterPath;

    tracePaths(cameraPath, cameraSampler, emitterPath, emitterSampler, s, t, true, true);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    if (cameraLength != t || lightLength != s)
        return;
    if (!cameraPath[t - 1].connectable() || (s > 0 && !emitterPath[s - 1].connectable()))
        return;

    if (s == 0) {
        Vec2u pixel = cameraPath[0].cameraRecord().pixel;
        Vec3f v = cameraPath.bdptWeightedPathEmission(t, t, state.ratios.get());
        state.splats.addSplat(s, t, pixel, v);
    } else if (t == 1) {
        Vec2f pixel;
        Vec3f splatWeight;
        if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, emitterSampler, splatWeight, pixel, state.ratios.get()))
            state.splats.addFilteredSplat(s, t, pixel, splatWeight*_lightSplatScale);
    } else {
        Vec2u pixel = cameraPath[0].cameraRecord().pixel;
        Vec3f v = LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces, _cameraSampler, state.ratios.get());
        state.splats.addSplat(s, t, pixel, v);
    }
}

void ReversibleJumpMltTracer::traceCandidatePath(LightPath &cameraPath, LightPath &emitterPath,
        const std::function<void(Vec3f, int, int)> &addCandidate)
{
    tracePaths(cameraPath, _cameraSampler, emitterPath, _emitterSampler);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    for (int s = 0; s <= lightLength; ++s) {
        int lowerBound = max(_settings.minBounces - s + 2, 1);
        int upperBound = min(_settings.maxBounces - s + 1, cameraLength);
        for (int t = lowerBound; t <= upperBound; ++t) {
            if (!cameraPath[t - 1].connectable() || (s > 0 && !emitterPath[s - 1].connectable()))
                continue;

            if (s == 0) {
                addCandidate(cameraPath.bdptWeightedPathEmission(t, t), s, t);
            } else if (t == 1) {
                Vec2f pixel;
                Vec3f splatWeight;
                if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, _emitterSampler, splatWeight, pixel))
                    addCandidate(splatWeight*_lightSplatScale, s, t);
            } else {
                addCandidate(LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces, _cameraSampler), s, t);
            }
        }
    }
}

void ReversibleJumpMltTracer::startSampleChain(int s, int t, float luminance, UniformSampler &cameraReplaySampler,
        UniformSampler &emitterReplaySampler)
{
    int length = s + t - 1;

    MarkovChain &chain = _chains[length];
    chain. cameraSampler.reset(new WritableMetropolisSampler(_settings.gaussianMutation, & cameraReplaySampler, length + 4));
    chain.emitterSampler.reset(new WritableMetropolisSampler(_settings.gaussianMutation, &emitterReplaySampler, length + 4));
    chain. currentState.reset(new ChainState(length));
    chain.proposedState.reset(new ChainState(length));
    chain.currentS = s;

    evalSample(*chain.cameraSampler, *chain.emitterSampler, length, s, *chain.currentState);

    chain.cameraSampler->accept();
    chain.emitterSampler->accept();
    chain.cameraSampler->setHelperGenerator(&_sampler);
    chain.emitterSampler->setHelperGenerator(&_sampler);

    if (chain.currentState->splats.totalLuminance() != luminance)
        FAIL("Underlying integrator is not consistent. Expected a value of %f, but received %f", luminance, chain.currentState->splats.totalLuminance());
}

void ReversibleJumpMltTracer::runSampleChain(int pathLength, int chainLength, MultiplexedStats &stats, float luminanceScale)
{
    MarkovChain &chain = _chains[pathLength];
    WritableMetropolisSampler & cameraSampler = *chain. cameraSampler;
    WritableMetropolisSampler &emitterSampler = *chain.emitterSampler;
    std::unique_ptr<ChainState> &current  = chain. currentState;
    std::unique_ptr<ChainState> &proposed = chain.proposedState;
    int &currentS = chain.currentS;

    float accumulatedWeight = 0.0f;
    for (int iter = 0; iter < chainLength; ++iter) {
        int proposedS = currentS;
        float strategySelector = _sampler.next1D();
        bool largeStep = strategySelector < _settings.largeStepProbability;
        bool strategyChange = (strategySelector >= _settings.largeStepProbability && strategySelector < _settings.largeStepProbability + _settings.strategyPerturbationProbability);

        float proposalWeight = 1.0f;
        if (largeStep) {
            proposedS = min(int(_sampler.next1D()*(pathLength + 1)), pathLength);
            cameraSampler.largeStep();
            emitterSampler.largeStep();
        } else if (strategyChange) {
            cameraSampler.freeze();
            emitterSampler.freeze();

            float sum = 0.0f;
            for (int i = 0; i <= pathLength; ++i)
                sum += current->ratios[i];
            float target = sum*_sampler.next1D();
            for (proposedS = 0; proposedS < pathLength; ++proposedS) {
                target -= current->ratios[proposedS];
                if (target < 0.0f)
                    break;
            }

            if (!LightPath::invert(cameraSampler, emitterSampler, current->cameraPath, current->emitterPath, proposedS))
                proposalWeight = 0.0f;
            else
                proposalWeight = 1.0f;

            cameraSampler.seek(0);
            emitterSampler.seek(0);
        } else {
            cameraSampler.smallStep();
            emitterSampler.smallStep();
        }

        evalSample(cameraSampler, emitterSampler, pathLength, proposedS, *proposed);

        float currentI = current->splats.totalLuminance();
        float proposedI = proposed->splats.totalLuminance();

        float a = currentI == 0.0f ? 1.0f : min(proposalWeight*proposedI/currentI, 1.0f);

        float currentWeight = (1.0f - a);
        float proposedWeight = a;

        accumulatedWeight += currentWeight;

        bool accept = _sampler.next1D() < a;

        if (accept) {
            if (currentI != 0.0f)
                current->splats.apply(*_scene->cam().splatBuffer(), luminanceScale*accumulatedWeight/currentI);

            std::swap(current, proposed);
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
                proposed->splats.apply(*_scene->cam().splatBuffer(), luminanceScale*proposedWeight/proposedI);

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
            current->splats.apply(*_pyramid, luminanceScale/current->splats.totalLuminance());
    }

    if (current->splats.totalLuminance() != 0.0f)
        current->splats.apply(*_scene->cam().splatBuffer(), luminanceScale*accumulatedWeight/current->splats.totalLuminance());
}

}

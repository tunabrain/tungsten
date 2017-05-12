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
        int s, int t, bool prune)
{
    if (t == -1)
        t = _settings.maxBounces + 1;
    if (s == -1)
        s = _settings.maxBounces;

    cameraPath.clear();
    cameraPath.startCameraPath(&_scene->cam());
    cameraSampler.startPath(0, 0);
    if (t > 0)
        cameraPath.tracePath(*_scene, *this,  cameraSampler, t, prune);

    emitterPath.clear();
    emitterPath.startEmitterPath(_scene->lights()[0].get(), 1.0f);
    emitterSampler.startPath(0, 0);
    if (s > 0)
        emitterPath.tracePath(*_scene, *this, emitterSampler, s, prune);
}

void ReversibleJumpMltTracer::evalSample(WritableMetropolisSampler &cameraSampler, WritableMetropolisSampler &emitterSampler,
        int length, int s, ChainState &state)
{
    state.splats.clear();

    int t = length + 1 - s;

    LightPath & cameraPath = state.cameraPath;
    LightPath &emitterPath = state.emitterPath;

    tracePaths(cameraPath, cameraSampler, emitterPath, emitterSampler, s, t, false);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    if (cameraLength != t || lightLength != s)
        return;

    state. unprunedCameraPath.copy( cameraPath);
    state.unprunedEmitterPath.copy(emitterPath);

     cameraPath.prune();
    emitterPath.prune();

    int prunedT =  cameraPath.length();
    int prunedS = emitterPath.length();

    if (s == 0) {
        Vec2u pixel = cameraPath[0].cameraRecord().pixel;
        Vec3f v = cameraPath.bdptWeightedPathEmission(t, t, state.ratios.get());
        state.splats.addSplat(s, t, pixel, v);
    } else if (t == 1) {
        Vec2f pixel;
        Vec3f splatWeight;
        if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, prunedS, _settings.maxBounces, emitterSampler, splatWeight, pixel, state.ratios.get()))
            state.splats.addFilteredSplat(s, t, pixel, splatWeight*_lightSplatScale);
    } else {
        Vec2u pixel = cameraPath[0].cameraRecord().pixel;
        Vec3f v = LightPath::bdptConnect(*this, cameraPath, emitterPath, prunedS, prunedT, _settings.maxBounces, _cameraSampler, state.ratios.get());
        state.splats.addSplat(s, t, pixel, v);
    }
}

void ReversibleJumpMltTracer::traceCandidatePath(LightPath &cameraPath, LightPath &emitterPath,
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

LargeStepTracker ReversibleJumpMltTracer::runSampleChain(int pathLength, int chainLength,
        MultiplexedStats &stats, float luminanceScale)
{
    MarkovChain &chain = _chains[pathLength];
    WritableMetropolisSampler & cameraSampler = *chain. cameraSampler;
    WritableMetropolisSampler &emitterSampler = *chain.emitterSampler;
    std::unique_ptr<ChainState> &current  = chain. currentState;
    std::unique_ptr<ChainState> &proposed = chain.proposedState;
    int &currentS = chain.currentS;

    LargeStepTracker largeSteps;

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

            int prunedLength = current->cameraPath.length() + current->emitterPath.length() - 1;
            float sum = 0.0f;
            for (int i = 0; i <= prunedLength; ++i)
                sum += current->ratios[i];
            float target = sum*_sampler.next1D();
            for (proposedS = 0; proposedS < prunedLength; ++proposedS) {
                target -= current->ratios[proposedS];
                if (target < 0.0f)
                    break;
            }
            if (proposedS <= current->emitterPath.length()) {
                if (proposedS > 0)
                    proposedS = current->emitterPath.vertexIndex(proposedS - 1) + 1; /* TODO */
            } else {
                int proposedT = prunedLength + 1 - proposedS;
                if (proposedT > 0)
                    proposedT = current->cameraPath.vertexIndex(proposedT - 1) + 1; /* TODO */
                proposedS = prunedLength + 1 - proposedT;
            }

            if (!LightPath::invert(cameraSampler, emitterSampler, current->unprunedCameraPath, current->unprunedEmitterPath, proposedS)) {
                proposalWeight = 0.0f;
                stats.inversion().reject(pathLength);
            } else {
                proposalWeight = 1.0f;
                stats.inversion().accept(pathLength);
            }

            cameraSampler.seek(0);
            emitterSampler.seek(0);
        } else {
            cameraSampler.smallStep();
            emitterSampler.smallStep();
        }

        evalSample(cameraSampler, emitterSampler, pathLength, proposedS, *proposed);

        float currentI = current->splats.totalLuminance();
        float proposedI = proposed->splats.totalLuminance();

        if (largeStep)
            largeSteps.add(proposedI*(pathLength + 1));

        float a = currentI == 0.0f ? 1.0f : min(proposalWeight*proposedI/currentI, 1.0f);

        float currentWeight = (1.0f - a);
        float proposedWeight = a;

        accumulatedWeight += currentWeight;

        bool accept = _sampler.next1D() < a;

        if (accept) {
            if (currentI != 0.0f)
                current->splats.apply(*_scene->cam().splatBuffer(), accumulatedWeight/currentI);

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
                proposed->splats.apply(*_scene->cam().splatBuffer(), proposedWeight/proposedI);

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
        current->splats.apply(*_scene->cam().splatBuffer(), accumulatedWeight/current->splats.totalLuminance());

    return largeSteps;
}

}

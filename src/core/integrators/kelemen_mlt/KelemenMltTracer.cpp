#include "KelemenMltTracer.hpp"

#include "sampling/UniformSampler.hpp"

namespace Tungsten {

KelemenMltTracer::KelemenMltTracer(TraceableScene *scene, const KelemenMltSettings &settings, uint32 threadId)
: PathTracer(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _settings(settings),
  _sampler(MathUtil::hash32(threadId)),
  _pathCandidates(new PathCandidate[settings.initialSamplePool])
{
}

void KelemenMltTracer::tracePath(SampleGenerator &sampler, Vec2u &pixel, Vec3f &f, float &i)
{
    pixel = min(Vec2u(Vec2f(_scene->cam().resolution())*sampler.next2D()), _scene->cam().resolution());

    f = traceSample(pixel, sampler, sampler);
    i = f.luminance();
}

void KelemenMltTracer::selectSeedPath(int &idx, float &weight)
{
    for (int i = 0; i < _settings.initialSamplePool; ++i) {
        _pathCandidates[i].state = _sampler.state();

        Vec2u pixel;
        Vec3f f;
        tracePath(_sampler, pixel, f, _pathCandidates[i].luminanceSum);

        if (i > 0)
            _pathCandidates[i].luminanceSum += _pathCandidates[i - 1].luminanceSum;
    }

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
    MetropolisSampler sampler(&replaySampler, _settings.maxBounces*16);

    Vec2u currentPixel;
    Vec3f currentF;
    float currentI;
    tracePath(sampler, currentPixel, currentF, currentI);

    sampler.accept();
    sampler.setHelperGenerator(&_sampler);

    float currentWeight = 0.0f;
    for (int i = 1; i < chainLength; ++i) {
        sampler.setLargeStep(_sampler.next1D() < _settings.largeStepProbability);

        Vec2u proposedPixel;
        Vec3f proposedF;
        float proposedI;
        tracePath(sampler, proposedPixel, proposedF, proposedI);

        float a = currentI == 0.0f ? 1.0f : min(proposedI/currentI, 1.0f);

        currentWeight += 1.0f - a;

        if (_sampler.next1D() < a) {
            if (currentI != 0.0f)
                _splatBuffer->splat(currentPixel, currentF*(weight*currentWeight/currentI));

            currentPixel = proposedPixel;
            currentF = proposedF;
            currentI = proposedI;
            currentWeight = a;

            sampler.accept();
        } else {
            if (proposedI != 0.0f)
                _splatBuffer->splat(proposedPixel, proposedF*(weight*a/proposedI));

            sampler.reject();
        }
    }
}

}

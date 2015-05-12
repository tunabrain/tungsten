#include "BidirectionalPathTracer.hpp"

#include <thread>

namespace Tungsten {

BidirectionalPathTracer::BidirectionalPathTracer(TraceableScene *scene, const BidirectionalPathTracerSettings &settings,
        uint32 threadId)
: TraceBase(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _cameraPath(new LightPath(settings.maxBounces)),
  _emitterPath(new LightPath(settings.maxBounces))
{
    std::vector<float> lightWeights(scene->lights().size());
    for (size_t i = 0; i < scene->lights().size(); ++i) {
        scene->lights()[i]->makeSamplable(_threadId);
        lightWeights[i] = 1.0f; // TODO
    }
    _lightSampler.reset(new Distribution1D(std::move(lightWeights)));
}

Vec3f BidirectionalPathTracer::traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler)
{
    LightPath & cameraPath = * _cameraPath;
    LightPath &emitterPath = *_emitterPath;

    float u = supplementalSampler.next1D();
    int lightIdx;
    _lightSampler->warp(u, lightIdx);
    const Primitive *light = _scene->lights()[lightIdx].get();

    cameraPath.startCameraPath(&_scene->cam(), pixel);
    emitterPath.startEmitterPath(light, _lightSampler->pdf(lightIdx));

     cameraPath.tracePath(*_scene, *this, sampler, supplementalSampler);
    emitterPath.tracePath(*_scene, *this, sampler, supplementalSampler);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    Vec3f result = cameraPath.bdptWeightedPathEmission(_settings.minBounces + 2, _settings.maxBounces + 1);
    for (int s = 1; s <= lightLength; ++s) {
        int lowerBound = max(_settings.minBounces - s + 2, 1);
        int upperBound = min(_settings.maxBounces - s + 1, cameraLength);
        for (int t = lowerBound; t <= upperBound; ++t) {
            if (!cameraPath[t - 1].connectable() || !emitterPath[s - 1].connectable())
                continue;

            if (t == 1) {
                Vec2u pixel;
                Vec3f splatWeight;
                if (LightPath::bdptCameraConnect(*_scene, cameraPath, emitterPath, s, sampler, splatWeight, pixel))
                    _splatBuffer->splat(pixel, splatWeight);
            } else {
                result += LightPath::bdptConnect(*_scene, cameraPath, emitterPath, s, t);
            }
        }
    }
    return result;
}

}

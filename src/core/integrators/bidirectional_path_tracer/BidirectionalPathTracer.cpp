#include "BidirectionalPathTracer.hpp"

#include <thread>

namespace Tungsten {

BidirectionalPathTracer::BidirectionalPathTracer(TraceableScene *scene, const BidirectionalPathTracerSettings &settings,
        uint32 threadId)
: TraceBase(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _cameraPath(new LightPath(settings.maxBounces + 1)),
  _emitterPath(new LightPath(settings.maxBounces + 1))
{
}

Vec3f BidirectionalPathTracer::traceSample(Vec2u pixel, PathSampleGenerator &sampler)
{
    LightPath & cameraPath = * _cameraPath;
    LightPath &emitterPath = *_emitterPath;

    float lightPdf;
    const Primitive *light = chooseLightAdjoint(sampler, lightPdf);

    cameraPath.startCameraPath(&_scene->cam(), pixel);
    emitterPath.startEmitterPath(light, lightPdf);

     cameraPath.tracePath(*_scene, *this, sampler);
    emitterPath.tracePath(*_scene, *this, sampler);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    Vec3f result = cameraPath.bdptWeightedPathEmission(_settings.minBounces + 2, _settings.maxBounces + 1);
    for (int s = 1; s <= lightLength; ++s) {
        int upperBound = min(_settings.maxBounces - s + 1, cameraLength);
        for (int t = 1; t <= upperBound; ++t) {
            if (!cameraPath[t - 1].connectable() || !emitterPath[s - 1].connectable())
                continue;

            if (t == 1) {
                Vec2f pixel;
                Vec3f splatWeight;
                if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, sampler, splatWeight, pixel))
                    _splatBuffer->splatFiltered(pixel, splatWeight);
            } else {
                result += LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces);
            }
        }
    }
    return result;
}

}

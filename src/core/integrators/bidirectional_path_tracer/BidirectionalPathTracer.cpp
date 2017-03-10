#include "BidirectionalPathTracer.hpp"

#include "integrators/bidirectional_path_tracer/ImagePyramid.hpp"

#include <thread>

namespace Tungsten {

BidirectionalPathTracer::BidirectionalPathTracer(TraceableScene *scene, const BidirectionalPathTracerSettings &settings,
        uint32 threadId, ImagePyramid *imagePyramid)
: TraceBase(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer()),
  _imagePyramid(imagePyramid),
  _cameraPath(new LightPath(settings.maxBounces + 1)),
  _emitterPath(new LightPath(settings.maxBounces + 1))
{
    if (settings.imagePyramid)
        _directEmissionByBounce.reset(new Vec3f[settings.maxBounces + 2]);
}

Vec3f BidirectionalPathTracer::traceSample(Vec2u pixel, uint32 lightPathId, PathSampleGenerator &sampler)
{
    LightPath & cameraPath = * _cameraPath;
    LightPath &emitterPath = *_emitterPath;

    float lightPdf;
    const Primitive *light = chooseLightAdjoint(sampler, lightPdf);

    cameraPath.startCameraPath(&_scene->cam(), pixel);
    emitterPath.startEmitterPath(light, lightPdf);

     cameraPath.tracePath(*_scene, *this, sampler);
    sampler.startPath(0, lightPathId);
    emitterPath.tracePath(*_scene, *this, sampler);

    int cameraLength =  cameraPath.length();
    int  lightLength = emitterPath.length();

    Vec3f result = cameraPath.bdptWeightedPathEmission(_settings.minBounces + 2, _settings.maxBounces + 1, nullptr, _directEmissionByBounce.get());

    if (_imagePyramid)
        for (int t = 2; t <= cameraPath.length(); ++t)
            _imagePyramid->splat(0, t, pixel, _directEmissionByBounce[t - 2]);

    for (int s = 1; s <= lightLength; ++s) {
        int upperBound = min(_settings.maxBounces - s + 1, cameraLength);
        for (int t = 1; t <= upperBound; ++t) {
            if (!cameraPath[t - 1].connectable() || !emitterPath[s - 1].connectable())
                continue;

            if (t == 1) {
                Vec2f pixel;
                Vec3f splatWeight;
                if (LightPath::bdptCameraConnect(*this, cameraPath, emitterPath, s, _settings.maxBounces, sampler, splatWeight, pixel)) {
                    _splatBuffer->splatFiltered(pixel, splatWeight);
                    if (_imagePyramid)
                        _imagePyramid->splatFiltered(s, t, pixel, splatWeight);
                }
            } else {
                Vec3f v = LightPath::bdptConnect(*this, cameraPath, emitterPath, s, t, _settings.maxBounces, sampler);
                result += v;
                if (_imagePyramid)
                    _imagePyramid->splat(s, t, pixel, v);
            }
        }
    }
    return result;
}

}

#include "LightTracer.hpp"

namespace Tungsten {

LightTracer::LightTracer(TraceableScene *scene, const LightTracerSettings &settings, uint32 threadId)
: TraceBase(scene, settings, threadId),
  _settings(settings),
  _splatBuffer(scene->cam().splatBuffer())
{
}

void LightTracer::traceSample(PathSampleGenerator &sampler)
{
    float lightPdf;
    const Primitive *light = chooseLightAdjoint(sampler, lightPdf);
    const Medium *medium = light->extMedium().get();

    PositionSample point;
    if (!light->samplePosition(sampler, point))
        return;
    DirectionSample direction;
    if (!light->sampleDirection(sampler, point, direction))
        return;

    Vec3f throughput(point.weight/lightPdf);

    LensSample splat;
    if (_settings.minBounces == 0 && !light->isInfinite() && _scene->cam().sampleDirect(point.p, sampler, splat)) {
        Ray shadowRay(point.p, splat.d);
        shadowRay.setFarT(splat.dist);

        Vec3f transmission = generalizedShadowRay(sampler, shadowRay, medium, nullptr, true, true, 0);
        if (transmission != 0.0f) {
            Vec3f value = throughput*transmission*splat.weight
                    *light->evalDirectionalEmission(point, DirectionSample(splat.d));
            _splatBuffer->splatFiltered(splat.pixel, value);
        }
    }

    Ray ray(point.p, direction.d);
    throughput *= direction.weight;

    MediumSample mediumSample;
    SurfaceScatterEvent surfaceEvent;
    IntersectionTemporary data;
    IntersectionInfo info;
    Medium::MediumState state;
    state.reset();
    Vec3f emission(0.0f);

    int bounceSinceSurface = 0;
    int bounce = 0;
    bool wasSpecular = true;
    bool didHit = _scene->intersect(ray, data, info);
    while ((didHit || medium) && bounce < _settings.maxBounces - 1) {
        bool hitSurface = true;
        if (medium) {
            if (!medium->sampleDistance(sampler, ray, state, mediumSample))
                break;
            throughput *= mediumSample.weight;
            hitSurface = mediumSample.exited;
            if (hitSurface && !didHit)
                break;
        }

        if (hitSurface) {
            bounceSinceSurface = 0;
            surfaceEvent = makeLocalScatterEvent(data, info, ray, &sampler);

            if (_settings.includeSurfaces) {
                Vec3f weight;
                Vec2f pixel;
                if (surfaceLensSample(_scene->cam(), surfaceEvent, medium, bounce + 1, ray, weight, pixel))
                    _splatBuffer->splatFiltered(pixel, weight*throughput);
            }

            if (!handleSurface(surfaceEvent, data, info, medium, bounce,
                    true, false, ray, throughput, emission, wasSpecular, state))
                break;
        } else {
            bounceSinceSurface++;

            if (bounceSinceSurface >= 2 || _settings.lowOrderScattering) {
                Vec3f weight;
                Vec2f pixel;
                if (volumeLensSample(_scene->cam(), sampler, mediumSample, medium, bounce + 1, ray, weight, pixel))
                    _splatBuffer->splatFiltered(pixel, weight*throughput);
            }

            if (!handleVolume(sampler, mediumSample, medium, bounce,
                    true, false, ray, throughput, emission, wasSpecular))
                break;

        }

        if (throughput.max() == 0.0f)
            break;
        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            break;
        if (std::isnan(throughput.sum()))
            break;

        bounce++;
        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }
}

}

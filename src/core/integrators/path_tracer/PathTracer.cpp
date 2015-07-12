#include "PathTracer.hpp"

namespace Tungsten {

PathTracer::PathTracer(TraceableScene *scene, const PathTracerSettings &settings, uint32 threadId)
: TraceBase(scene, settings, threadId),
  _settings(settings)
{
}

Vec3f PathTracer::traceSample(Vec2u pixel, PathSampleGenerator &sampler)
{
    // TODO: Put diagnostic colors in JSON?
    const Vec3f nanDirColor = Vec3f(0.0f);
    const Vec3f nanEnvDirColor = Vec3f(0.0f);
    const Vec3f nanBsdfColor = Vec3f(0.0f);

    try {

    PositionSample point;
    if (!_scene->cam().samplePosition(sampler, point))
        return Vec3f(0.0f);
    DirectionSample direction;
    if (!_scene->cam().sampleDirection(sampler, point, pixel, direction))
        return Vec3f(0.0f);
    sampler.advancePath();

    Vec3f throughput = point.weight*direction.weight;
    Ray ray(point.p, direction.d);
    ray.setPrimaryRay(true);

    MediumSample mediumSample;
    SurfaceScatterEvent surfaceEvent;
    IntersectionTemporary data;
    Medium::MediumState state;
    state.reset();
    IntersectionInfo info;
    Vec3f emission(0.0f);
    const Medium *medium = _scene->cam().medium().get();

    int bounce = 0;
    bool didHit = _scene->intersect(ray, data, info);
    bool wasSpecular = true;
    while ((didHit || medium) && bounce < _settings.maxBounces) {
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
            surfaceEvent = makeLocalScatterEvent(data, info, ray, &sampler);
            if (!handleSurface(surfaceEvent, data, info, medium, bounce, false,
                    _settings.enableLightSampling, ray, throughput, emission, wasSpecular, state))
                break;
        } else {
            if (!handleVolume(sampler, mediumSample, medium, bounce, false,
                    _settings.enableVolumeLightSampling, ray, throughput, emission, wasSpecular))
                break;
        }

        if (throughput.max() == 0.0f)
            break;

        float roulettePdf = std::abs(throughput).max();
        if (bounce > 2 && roulettePdf < 0.1f) {
            if (sampler.nextBoolean(DiscreteRouletteSample, roulettePdf))
                throughput /= roulettePdf;
            else
                break;
        }

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            return nanDirColor;
        if (std::isnan(throughput.sum() + emission.sum()))
            return nanBsdfColor;

        sampler.advancePath();
        bounce++;
        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }
    if (!didHit && !medium && bounce >= _settings.minBounces) {
        if (_scene->intersectInfinites(ray, data, info)) {
            if (!_settings.enableLightSampling || bounce == 0 || wasSpecular || !info.primitive->isSamplable())
                emission += throughput*info.primitive->evalDirect(data, info);
        }
    }
    if (std::isnan(throughput.sum() + emission.sum()))
        return nanEnvDirColor;
    return emission;

    } catch (std::runtime_error &e) {
        std::cout << tfm::format("Caught an internal error at pixel %s: %s", pixel, e.what()) << std::endl;

        return Vec3f(0.0f);
    }
}

}

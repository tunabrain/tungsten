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

    Ray ray;
    Vec3f throughput(1.0f);
    if (!_scene->cam().generateSample(pixel, sampler, throughput, ray))
        return Vec3f(0.0f);
    sampler.advancePath();
    ray.setPrimaryRay(true);

    SurfaceScatterEvent event;
    IntersectionTemporary data;
    Medium::MediumState state;
    state.reset();
    IntersectionInfo info;
    Vec3f emission(0.0f);
    const Medium *medium = _scene->cam().medium().get();

    int bounce = 0;
    bool didHit = _scene->intersect(ray, data, info);
    bool wasSpecular = true;
    bool hitSurface = true;
    while ((didHit || medium) && bounce < _settings.maxBounces) {
        if (medium && !handleVolume(sampler, medium, bounce,
                false, _settings.enableVolumeLightSampling, ray, throughput, emission, wasSpecular, hitSurface, state))
            break;

        if (hitSurface && !didHit)
            break;

        if (hitSurface) {
            event = makeLocalScatterEvent(data, info, ray, &sampler);
            if (!handleSurface(event, data, info, sampler, medium, bounce,
                    false, _settings.enableLightSampling, ray, throughput, emission, wasSpecular, state))
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

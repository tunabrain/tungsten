#include "PathTracer.hpp"

namespace Tungsten {

PathTracer::PathTracer(TraceableScene *scene, const TraceSettings &settings, uint32 threadId)
: TraceBase(scene, settings, threadId)
{
}

Vec3f PathTracer::traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler)
{
    // TODO: Put diagnostic colors in JSON?
    const Vec3f nanDirColor(0.0f, 0.0f, 0.0f);
    const Vec3f nanEnvDirColor(0.0f, 0.0f, 0.0f);
    const Vec3f nanBsdfColor(0.0f, 0.0f, 0.0f);

    try {

    Ray ray;
    Vec3f throughput(1.0f);
    if (!_scene->cam().generateSample(pixel, sampler, throughput, ray))
        return Vec3f(0.0f);
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
        if (medium && !handleVolume(sampler, supplementalSampler, medium, bounce,
                true, ray, throughput, emission, wasSpecular, hitSurface, state))
            break;

        if (hitSurface && !didHit)
            break;

        if (hitSurface) {
            event = makeLocalScatterEvent(data, info, ray, &sampler, &supplementalSampler);
            if (!handleSurface(event, data, info, sampler, supplementalSampler, medium, bounce,
                    true, ray, throughput, emission, wasSpecular, state))
                break;
        }

        if (throughput.max() == 0.0f)
            break;

        float roulettePdf = std::abs(throughput).max();
        if (bounce > 2 && roulettePdf < 0.1f) {
            if (supplementalSampler.next1D() < roulettePdf)
                throughput /= roulettePdf;
            else
                break;
        }

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            return nanDirColor;
        if (std::isnan(throughput.sum() + emission.sum()))
            return nanBsdfColor;

        bounce++;
        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }
    if (!didHit && !medium && bounce >= _settings.minBounces) {
        if (_scene->intersectInfinites(ray, data, info)) {
            if (_settings.enableLightSampling) {
                if (bounce == 0 || wasSpecular || !info.primitive->isSamplable())
                    emission += throughput*info.primitive->emission(data, info);
            } else {
                emission += throughput*info.primitive->emission(data, info);
            }
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

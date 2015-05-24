#include "LightTracer.hpp"

namespace Tungsten {

LightTracer::LightTracer(TraceableScene *scene, const LightTracerSettings &settings, uint32 threadId)
: TraceBase(scene, settings, threadId),
  _splatBuffer(scene->cam().splatBuffer())
{
    std::vector<float> lightWeights(scene->lights().size());
    for (size_t i = 0; i < scene->lights().size(); ++i) {
        scene->lights()[i]->makeSamplable(_threadId);
        lightWeights[i] = 1.0f; // TODO
    }
    _lightSampler.reset(new Distribution1D(std::move(lightWeights)));
}

void LightTracer::traceSample(SampleGenerator &sampler, SampleGenerator &supplementalSampler)
{
    float u = supplementalSampler.next1D();
    int lightIdx;
    _lightSampler->warp(u, lightIdx);
    const Primitive &light = *_scene->lights()[lightIdx];

    PositionSample point;
    if (!light.samplePosition(sampler, point))
        return;

    Vec3f throughput(point.weight/_lightSampler->pdf(lightIdx));

    LensSample splat;
    if (_scene->cam().sampleDirect(point.p, sampler, splat)) {
        Ray shadowRay(point.p, splat.d);
        shadowRay.setFarT(splat.dist);

        // TODO: Volume handling
        Vec3f transmission = generalizedShadowRay(shadowRay, nullptr, nullptr, 0);
        if (transmission != 0.0f) {
            Vec3f value = throughput*transmission*splat.weight
                    *light.evalDirectionalEmission(point, DirectionSample(splat.d));
            _splatBuffer->splat(splat.pixel, value);
        }
    }

    DirectionSample direction;
    if (!light.sampleDirection(sampler, point, direction))
        return;

    Ray ray(point.p, direction.d);
    throughput *= direction.weight;

    SurfaceScatterEvent event;
    IntersectionTemporary data;
    IntersectionInfo info;
    Medium::MediumState state;
    state.reset();
    Vec3f emission(0.0f);
    const Medium *medium = nullptr; // TODO: Volume handling

    int bounce = 0;
    bool wasSpecular = true;
    bool hitSurface = true;
    bool didHit = _scene->intersect(ray, data, info);
    while ((didHit || medium) && bounce < _settings.maxBounces - 1) {
        if (medium) {
            VolumeScatterEvent event(&sampler, &supplementalSampler, throughput, ray.pos(), ray.dir(), ray.farT());
            if (!medium->sampleDistance(event, state))
                break;
            throughput *= event.throughput;
            event.throughput = Vec3f(1.0f);

            if (event.t < event.maxT) {
                event.p += event.t*event.wi;

                // TODO Camera splat

                if (medium->absorb(event, state))
                    break;
                if (!medium->scatter(event))
                    break;
                ray = ray.scatter(event.p, event.wo, 0.0f);
                ray.setPrimaryRay(false);
                throughput *= event.throughput;
                hitSurface = false;
            } else {
                hitSurface = true;
            }
        }

        if (hitSurface) {
            event = makeLocalScatterEvent(data, info, ray, &sampler, &supplementalSampler);

            Vec3f weight;
            Vec2u pixel;
            if (lensSample(_scene->cam(), event, medium, bounce, ray, weight, pixel))
                _splatBuffer->splat(pixel, weight*throughput);

            if (!handleSurface(event, data, info, sampler, supplementalSampler, medium, bounce,
                    true, ray, throughput, emission, wasSpecular, state))
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

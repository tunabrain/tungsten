#include "PhotonTracer.hpp"

namespace Tungsten {

PhotonTracer::PhotonTracer(TraceableScene *scene, const PhotonMapSettings &settings, uint32 threadId)
: TraceBase(scene, settings, threadId),
  _settings(settings),
  _photonQuery(new const Photon *[settings.gatherCount]),
  _distanceQuery(new float[settings.gatherCount])
{
    std::vector<float> lightWeights(scene->lights().size());
    for (size_t i = 0; i < scene->lights().size(); ++i) {
        scene->lights()[i]->makeSamplable(_threadId);
        lightWeights[i] = 1.0f; // TODO
    }
    _lightSampler.reset(new Distribution1D(std::move(lightWeights)));
}

int PhotonTracer::tracePhoton(Photon *dst, int maxCount, SampleGenerator &sampler, UniformSampler &supplementalSampler)
{
    float u = supplementalSampler.next1D();
    int lightIdx;
    _lightSampler->warp(u, lightIdx);

    LightSample sample(&sampler);
    if (!_scene->lights()[lightIdx]->sampleOutboundDirection(_threadId, sample))
        return 0;

    Ray ray(sample.p, sample.d);
    Vec3f throughput(sample.weight/_lightSampler->pdf(lightIdx));

    IntersectionTemporary data;
    IntersectionInfo info;
    Medium::MediumState state;
    state.reset();
    Vec3f emission(0.0f);
    const Medium *medium = _scene->cam().medium().get();

    int photonCount = 0;
    int bounce = 0;
    bool wasSpecular = true;
    bool didHit = _scene->intersect(ray, data, info);
    while (didHit && bounce < _settings.maxBounces) {
        ray.advanceFootprint();

        if (!info.bsdf->lobes().isPureSpecular()) {
            Photon &p = dst[photonCount++];
            p.pos = info.p;
            p.dir = ray.dir();
            p.power = throughput;

            if (photonCount == maxCount)
                break;
        }

        if (!handleSurface(data, info, sampler, supplementalSampler, medium, bounce,
                false, ray, throughput, emission, wasSpecular, state))
            break;

        /*float roulettePdf = std::abs(throughput).max();
        if (supplementalSampler.next1D() < roulettePdf)
            throughput /= roulettePdf;
        else
            break;*/

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            break;
        if (std::isnan(throughput.sum()))
            break;

        bounce++;
        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }

    return photonCount;
}

Vec3f PhotonTracer::traceSample(Vec2u pixel, const KdTree &tree, SampleGenerator &sampler,
        UniformSampler &supplementalSampler)
{
    Ray ray;
    Vec3f throughput(1.0f);
    if (!_scene->cam().generateSample(pixel, sampler, throughput, ray))
        return Vec3f(0.0f);
    ray.setPrimaryRay(true);

    IntersectionTemporary data;
    IntersectionInfo info;

    int bounce = 0;
    bool didHit = _scene->intersect(ray, data, info);
    while (didHit && bounce < _settings.maxBounces) {
        ray.advanceFootprint();

        const Bsdf &bsdf = *info.bsdf;

        SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, &sampler, &supplementalSampler);

        Vec3f transparency = bsdf.eval(event.makeForwardEvent());
        float transparencyScalar = transparency.avg();

        Vec3f wo;
        float pdf;
        if (sampler.next1D() < transparencyScalar) {
            pdf = 0.0f;
            wo = ray.dir();
            throughput *= transparency/transparencyScalar;
        } else {
            event.requestedLobe = BsdfLobes::SpecularLobe;
            if (!bsdf.sample(event))
                break;

            pdf = event.pdf;
            wo = event.frame.toGlobal(event.wo);

            throughput *= event.throughput;
        }

        ray = ray.scatter(ray.hitpoint(), wo, info.epsilon, pdf);

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            break;
        if (std::isnan(throughput.sum()))
            break;

        bounce++;
        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }

    if (!didHit) {
        if (!_scene->intersectInfinites(ray, data, info))
            return Vec3f(0.0f);
        return throughput*info.primitive->emission(data, info);
    }

    int count = tree.nearestNeighbours(ray.hitpoint(), _photonQuery.get(), _distanceQuery.get(),
            _settings.gatherCount, _settings.gatherRadius);
    if (count == 0)
        return Vec3f(0.0f);

    const Bsdf &bsdf = *info.bsdf;
    SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, &sampler, &supplementalSampler);

    Vec3f result = info.primitive->emission(data, info);
    for (int i = 0; i < count; ++i) {
        event.wo = event.frame.toLocal(-_photonQuery[i]->dir);
        result += _photonQuery[i]->power*PI*bsdf.eval(event)/std::abs(event.wo.z());
    }

    return throughput*result*(INV_PI/_distanceQuery[0]);
}

}

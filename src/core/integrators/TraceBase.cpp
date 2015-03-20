#include "TraceBase.hpp"

namespace Tungsten {

TraceBase::TraceBase(TraceableScene *scene, const TraceSettings &settings, uint32 threadId)
: _scene(scene),
  _settings(settings),
  _threadId(threadId)
{
    _scene = scene;
    _lightPdf.resize(scene->lights().size());

    for (const auto &prim : scene->lights())
        prim->makeSamplable(_threadId);

    if (_scene->lights().size() == 1)
        _lightPdf[0] = 1.0f;
}

SurfaceScatterEvent TraceBase::makeLocalScatterEvent(IntersectionTemporary &data, IntersectionInfo &info,
        Ray &ray, SampleGenerator *sampler, UniformSampler *supplementalSampler) const
{
    TangentFrame frame;
    info.primitive->setupTangentFrame(data, info, frame);

    bool hitBackside = frame.normal.dot(ray.dir()) > 0.0f;
    bool isTransmissive = info.bsdf->lobes().isTransmissive();

    bool flipFrame = _settings.enableTwoSidedShading && hitBackside && !isTransmissive;

    if (flipFrame) {
        // TODO: Should we flip info.Ns here too? It doesn't seem to be used at the moment,
        // but it may be in the future. Modifying the intersection info itself could be a bad
        // idea though
        frame.normal = -frame.normal;
        frame.tangent = -frame.tangent;
    }

    return SurfaceScatterEvent(
        &info,
        sampler,
        supplementalSampler,
        frame,
        frame.toLocal(-ray.dir()),
        BsdfLobes::AllLobes,
        flipFrame
    );
}

bool TraceBase::isConsistent(const SurfaceScatterEvent &event, const Vec3f &w) const
{
    if (!_settings.enableConsistencyChecks)
        return true;
    bool geometricBackside = (w.dot(event.info->Ng) < 0.0f);
    bool shadingBackside = (event.wo.z() < 0.0f) ^ event.flippedFrame;
    return geometricBackside == shadingBackside;
}

Vec3f TraceBase::generalizedShadowRay(Ray &ray,
                           const Medium *medium,
                           IntersectionTemporary &data,
                           IntersectionInfo &info,
                           int bounce)
{
    const float initialFarT = ray.farT();
    Vec3f pos = ray.pos();
    Vec3f throughput(1.0f);
    do {
        if (!_scene->intersect(ray, data, info))
            return Vec3f(0.0f);
        if (medium)
            throughput *= medium->transmittance(VolumeScatterEvent(pos, ray.dir(), ray.farT()));

        bounce++;
        if (data.primitive->isEmissive())
            return bounce >= _settings.minBounces ? throughput : Vec3f(0.0f);
        if (!info.bsdf->lobes().test(BsdfLobes::ForwardLobe) || bounce >= _settings.maxBounces)
            return Vec3f(0.0f);

        SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, nullptr, nullptr);
        Vec3f transmittance = info.bsdf->eval(event.makeForwardEvent());
        if (transmittance == 0.0f)
            return Vec3f(0.0f);

        throughput *= transmittance;
        medium = info.bsdf->selectMedium(medium, !info.primitive->hitBackside(data));

        pos += ray.dir()*ray.farT();
        ray.setNearT(ray.farT() + info.epsilon);
        ray.setFarT(initialFarT);
    } while(true);
    return Vec3f(0.0f);
}

Vec3f TraceBase::attenuatedEmission(const Primitive *light,
                         const Medium *medium,
                         float expectedDist,
                         IntersectionTemporary &data,
                         IntersectionInfo &info,
                         int bounce,
                         Ray &ray)
{

    Vec3f transmittance = generalizedShadowRay(ray, medium, data, info, bounce);
    if (transmittance == 0.0f)
        return Vec3f(0.0f);

    CONSTEXPR float fudgeFactor = 1.0f + 1e-3f;
    if (info.primitive != light || ray.farT()*fudgeFactor < expectedDist)
        return Vec3f(0.0f);

    return transmittance*light->emission(data, info);
}

Vec3f TraceBase::lightSample(const TangentFrame &frame,
                  const Bsdf &bsdf,
                  SurfaceScatterEvent &event,
                  const Medium *medium,
                  int bounce,
                  float epsilon,
                  const Ray &parentRay)
{
    float lightPdf;
    const Primitive *light = chooseLight(*event.sampler, event.info->p, lightPdf);
    if (light == nullptr)
        return Vec3f(0.0f);

    LightSample sample(event.sampler, event.info->p);

    if (!light->sampleInboundDirection(_threadId, sample))
        return Vec3f(0.0f);

    event.wo = frame.toLocal(sample.d);
    if (!isConsistent(event, sample.d))
        return Vec3f(0.0f);

    bool geometricBackside = (sample.d.dot(event.info->Ng) < 0.0f);
    medium = bsdf.selectMedium(medium, geometricBackside);

    event.requestedLobe = BsdfLobes::AllButSpecular;

    Vec3f f = bsdf.eval(event);
    if (f == 0.0f)
        return Vec3f(0.0f);

    Ray ray = parentRay.scatter(sample.p, sample.d, epsilon);
    ray.setPrimaryRay(false);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f e = attenuatedEmission(light, medium, sample.dist, data, info, bounce, ray);
    if (e == 0.0f)
        return Vec3f(0.0f);

    float pdf = sample.pdf*lightPdf;
    Vec3f lightF = f*e/pdf;

    if (!light->isDelta())
        lightF *= SampleWarp::powerHeuristic(pdf, bsdf.pdf(event));

    return lightF;
}

Vec3f TraceBase::bsdfSample(const TangentFrame &frame,
                     const Bsdf &bsdf,
                     SurfaceScatterEvent &event,
                     const Medium *medium,
                     int bounce,
                     float epsilon,
                     const Ray &parentRay)
{
    event.requestedLobe = BsdfLobes::AllButSpecular;
    if (!bsdf.sample(event))
        return Vec3f(0.0f);
    if (event.throughput == 0.0f)
        return Vec3f(0.0f);

    Vec3f wo = frame.toGlobal(event.wo);
    if (!isConsistent(event, wo))
        return Vec3f(0.0f);

    bool geometricBackside = (wo.dot(event.info->Ng) < 0.0f);
    medium = bsdf.selectMedium(medium, geometricBackside);

    Ray ray = parentRay.scatter(event.info->p, wo, epsilon);
    ray.setPrimaryRay(false);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f throughput = generalizedShadowRay(ray, medium, data, info, bounce);
    if (throughput == Vec3f(0.0f))
        return Vec3f(0.0f);

    float lightPdf = info.primitive->inboundPdf(_threadId, data, info, event.info->p, wo);
    lightPdf *= _lightPdf[info.primitive->lightIndex()];

    Vec3f bsdfF = info.primitive->emission(data, info)*throughput*event.throughput;
    bsdfF *= SampleWarp::powerHeuristic(event.pdf, lightPdf);

    return bsdfF;
}

Vec3f TraceBase::volumeLightSample(VolumeScatterEvent &event,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay)
{
    float lightPdf;
    const Primitive *light = chooseLight(*event.sampler, event.p, lightPdf);
    if (light == nullptr)
        return Vec3f(0.0f);

    LightSample sample(event.sampler, event.p);

    if (!light->sampleInboundDirection(_threadId, sample))
        return Vec3f(0.0f);
    event.wo = sample.d;

    Vec3f f = medium->phaseEval(event);
    if (f == 0.0f)
        return Vec3f(0.0f);

    Ray ray = parentRay.scatter(sample.p, sample.d, 0.0f);
    ray.setPrimaryRay(false);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f e = attenuatedEmission(light, medium, sample.dist, data, info, bounce, ray);
    if (e == 0.0f)
        return Vec3f(0.0f);

    float pdf = lightPdf*sample.pdf;
    Vec3f lightF = f*e/pdf;

    if (!light->isDelta())
        lightF *= SampleWarp::powerHeuristic(pdf, medium->phasePdf(event));

    return lightF;
}

Vec3f TraceBase::volumePhaseSample(VolumeScatterEvent &event,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay)
{
    if (!medium->scatter(event))
        return Vec3f(0.0f);
    if (event.throughput == 0.0f)
        return Vec3f(0.0f);

    Ray ray = parentRay.scatter(event.p, event.wo, 0.0f);
    ray.setPrimaryRay(false);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f throughput = generalizedShadowRay(ray, medium, data, info, bounce);
    if (throughput == Vec3f(0.0f))
        return Vec3f(0.0f);

    float lightPdf = info.primitive->inboundPdf(_threadId, data, info, event.p, event.wo);
    lightPdf *= _lightPdf[info.primitive->lightIndex()];

    Vec3f phaseF = info.primitive->emission(data, info)*throughput*event.throughput;
    phaseF *= SampleWarp::powerHeuristic(event.pdf, lightPdf);

    return phaseF;
}

Vec3f TraceBase::sampleDirect(const TangentFrame &frame,
                   const Bsdf &bsdf,
                   SurfaceScatterEvent &event,
                   const Medium *medium,
                   int bounce,
                   float epsilon,
                   const Ray &parentRay)
{
    Vec3f result(0.0f);

    if (bsdf.lobes().isPureSpecular() || bsdf.lobes().isForward())
        return Vec3f(0.0f);

    result += lightSample(frame, bsdf, event, medium, bounce, epsilon, parentRay);
    result += bsdfSample(frame, bsdf, event, medium, bounce, epsilon, parentRay);

    return result;
}

Vec3f TraceBase::volumeSampleDirect(VolumeScatterEvent &event,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay)
{
    Vec3f result = volumeLightSample(event, medium, bounce, parentRay);
    result += volumePhaseSample(event, medium, bounce, parentRay);

    return result;
}

const Primitive *TraceBase::chooseLight(SampleGenerator &sampler, const Vec3f &p, float &pdf)
{
    if (_scene->lights().empty())
        return nullptr;
    if (_scene->lights().size() == 1) {
        pdf = 1.0f;
        return _scene->lights()[0].get();
    }

    float total = 0.0f;
    unsigned numNonNegative = 0;
    for (size_t i = 0; i < _lightPdf.size(); ++i) {
        _lightPdf[i] = _scene->lights()[i]->approximateRadiance(_threadId, p);
        if (_lightPdf[i] >= 0.0f) {
            total += _lightPdf[i];
            numNonNegative++;
        }
    }
    if (numNonNegative == 0) {
        for (size_t i = 0; i < _lightPdf.size(); ++i)
            _lightPdf[i] = 1.0f;
        total = _lightPdf.size();
    } else if (numNonNegative < _lightPdf.size()) {
        for (size_t i = 0; i < _lightPdf.size(); ++i) {
            float uniformWeight = (total == 0.0f ? 1.0f : total)/numNonNegative;
            if (_lightPdf[i] < 0.0f) {
                _lightPdf[i] = uniformWeight;
                total += uniformWeight;
            }
        }
    }
    if (total == 0.0f)
        return nullptr;
    for (size_t i = 0; i < _lightPdf.size(); ++i)
        _lightPdf[i] /= total;
    float t = sampler.next1D();
    for (size_t i = 0; i < _lightPdf.size(); ++i) {
        if (t < _lightPdf[i] || i == _lightPdf.size() - 1) {
            pdf = _lightPdf[i];
            return _scene->lights()[i].get();
        } else {
            t -= _lightPdf[i];
        }
    }
    return nullptr;
}

bool TraceBase::handleVolume(SampleGenerator &sampler, UniformSampler &supplementalSampler,
           const Medium *&medium, int bounce, bool handleLights, Ray &ray,
           Vec3f &throughput, Vec3f &emission, bool &wasSpecular, bool &hitSurface,
           Medium::MediumState &state)
{
    VolumeScatterEvent event(&sampler, &supplementalSampler, throughput, ray.pos(), ray.dir(), ray.farT());
    if (!medium->sampleDistance(event, state))
        return false;
    throughput *= event.throughput;
    event.throughput = Vec3f(1.0f);

    if (handleLights && bounce >= _settings.minBounces)
        emission += throughput*medium->emission(event);

    if (!_settings.enableVolumeLightSampling)
        wasSpecular = !hitSurface;

    if (event.t < event.maxT) {
        event.p += event.t*event.wi;

        if (handleLights && _settings.enableVolumeLightSampling && bounce < _settings.maxBounces - 1) {
            wasSpecular = false;
            emission += throughput*volumeSampleDirect(event, medium, bounce + 1, ray);
        }

        if (medium->absorb(event, state))
            return false;
        if (!medium->scatter(event))
            return false;
        ray = ray.scatter(event.p, event.wo, 0.0f);
        ray.setPrimaryRay(false);
        throughput *= event.throughput;
        hitSurface = false;
    } else {
        hitSurface = true;
    }

    return true;
}

bool TraceBase::handleSurface(IntersectionTemporary &data, IntersectionInfo &info,
                   SampleGenerator &sampler, UniformSampler &supplementalSampler,
                   const Medium *&medium, int bounce, bool handleLights, Ray &ray,
                   Vec3f &throughput, Vec3f &emission, bool &wasSpecular,
                   Medium::MediumState &state)
{
    const Bsdf &bsdf = *info.bsdf;

    SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, &sampler, &supplementalSampler);

    Vec3f transparency = bsdf.eval(event.makeForwardEvent());
    float transparencyScalar = transparency.avg();

    Vec3f wo;
    if (sampler.next1D() < transparencyScalar) {
        wo = ray.dir();
        throughput *= transparency/transparencyScalar;
    } else {
        if (handleLights) {
            if (_settings.enableLightSampling) {
                if ((wasSpecular || !info.primitive->isSamplable()) && bounce >= _settings.minBounces)
                    emission += info.primitive->emission(data, info)*throughput;

                if (bounce < _settings.maxBounces - 1)
                    emission += sampleDirect(event.frame, bsdf, event, medium, bounce + 1, info.epsilon, ray)*throughput;
            } else if (bounce >= _settings.minBounces) {
                emission += info.primitive->emission(data, info)*throughput;
            }
        }

        event.requestedLobe = BsdfLobes::AllLobes;
        if (!bsdf.sample(event))
            return false;

        wo = event.frame.toGlobal(event.wo);

        if (!isConsistent(event, wo))
            return false;

        throughput *= event.throughput;
        wasSpecular = event.sampledLobe.hasSpecular();
        if (!wasSpecular)
            ray.setPrimaryRay(false);
    }

    bool geometricBackside = (wo.dot(info.Ng) < 0.0f);
    medium = bsdf.selectMedium(medium, geometricBackside);
    state.reset();

    ray = ray.scatter(ray.hitpoint(), wo, info.epsilon);

    return true;
}

}

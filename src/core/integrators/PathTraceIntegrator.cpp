#include "PathTraceIntegrator.hpp"

namespace Tungsten {

CONSTEXPR bool PathTraceIntegrator::GeneralizedShadowRays;

PathTraceIntegrator::PathTraceIntegrator()
: _scene(nullptr),
  _enableLightSampling(true),
  _enableVolumeLightSampling(true),
  _enableConsistencyChecks(false),
  _enableTwoSidedShading(true),
  _minBounces(0),
  _maxBounces(64)
{
}

SurfaceScatterEvent PathTraceIntegrator::makeLocalScatterEvent(IntersectionTemporary &data, IntersectionInfo &info,
        Ray &ray, SampleGenerator *sampler, UniformSampler *supplementalSampler) const
{
    TangentFrame frame;
    info.primitive->setupTangentFrame(data, info, frame);

    bool hitBackside = frame.normal.dot(ray.dir()) > 0.0f;
    bool isTransmissive = info.bsdf->lobes().isTransmissive();

    bool flipFrame = _enableTwoSidedShading && hitBackside && !isTransmissive;

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

bool PathTraceIntegrator::isConsistent(const SurfaceScatterEvent &event, const Vec3f &w) const
{
    bool geometricBackside = (w.dot(event.info->Ng) < 0.0f);
    bool shadingBackside = (event.wo.z() < 0.0f) ^ event.flippedFrame;
    return geometricBackside == shadingBackside;
}

Vec3f PathTraceIntegrator::generalizedShadowRay(Ray &ray,
                           const Medium *medium,
                           const Primitive *endCap,
                           int bounce)
{
    // TODO: Could fire off occlusion ray regardless and only do expensive handling if we
    // hit something. May or may not be faster.

    if (!GeneralizedShadowRays)
        return _scene->occluded(ray) ? Vec3f(0.0f) : Vec3f(1.0f);
    IntersectionTemporary data;
    IntersectionInfo info;

    float initialFarT = ray.farT();
    Vec3f throughput(1.0f);
    do {
        if (_scene->intersect(ray, data, info) && info.primitive != endCap) {
            SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, nullptr, nullptr);

            Vec3f transmittance = info.bsdf->eval(event.makeForwardEvent());
            if (transmittance == 0.0f)
                return Vec3f(0.0f);

            throughput *= transmittance;
            bounce++;

            if (bounce >= _maxBounces)
                return Vec3f(0.0f);
        }

        if (medium)
            throughput *= medium->transmittance(VolumeScatterEvent(ray.pos(), ray.dir(), ray.farT()));
        if (info.primitive == nullptr || info.primitive == endCap)
            return bounce >= _minBounces ? throughput : Vec3f(0.0f);
        if (info.bsdf->overridesMedia()) {
            if (info.primitive->hitBackside(data))
                medium = info.bsdf->extMedium().get();
            else
                medium = info.bsdf->intMedium().get();
        }

        ray.advanceFootprint();
        ray.setPos(ray.hitpoint());
        initialFarT -= ray.farT();
        ray.setNearT(info.epsilon);
        ray.setFarT(initialFarT);
    } while(true);
    return Vec3f(0.0f);
}

Vec3f PathTraceIntegrator::attenuatedEmission(const Primitive &light,
                         const Medium *medium,
                         float expectedDist,
                         IntersectionTemporary &data,
                         IntersectionInfo &info,
                         int bounce,
                         Ray &ray)
{
    CONSTEXPR float fudgeFactor = 1.0f + 1e-3f;

    if (!light.intersect(ray, data) || ray.farT()*fudgeFactor < expectedDist)
        return Vec3f(0.0f);
    light.intersectionInfo(data, info);

    Vec3f transmittance = generalizedShadowRay(ray, medium, &light, bounce);
    if (transmittance == 0.0f)
        return Vec3f(0.0f);

    return transmittance*light.emission(data, info);
}

Vec3f PathTraceIntegrator::lightSample(const TangentFrame &frame,
                  const Primitive &light,
                  const Bsdf &bsdf,
                  SurfaceScatterEvent &event,
                  const Medium *medium,
                  int bounce,
                  float epsilon,
                  const Ray &parentRay)
{
    LightSample sample(event.sampler, event.info->p);

    if (!light.sampleInboundDirection(sample))
        return Vec3f(0.0f);

    event.wo = frame.toLocal(sample.d);
    if (!isConsistent(event, sample.d))
        return Vec3f(0.0f);

    bool geometricBackside = (sample.d.dot(event.info->Ng) < 0.0f);
    if (bsdf.overridesMedia()) {
        if (geometricBackside)
            medium = bsdf.intMedium().get();
        else
            medium = bsdf.extMedium().get();
    }

    event.requestedLobe = BsdfLobes::AllButSpecular;

    Vec3f f = bsdf.eval(event);
    if (f == 0.0f)
        return Vec3f(0.0f);

    Ray ray = parentRay.scatter(sample.p, sample.d, epsilon, sample.pdf);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f e = attenuatedEmission(light, medium, sample.dist, data, info, bounce, ray);
    if (e == 0.0f)
        return Vec3f(0.0f);

    Vec3f lightF = f*e/sample.pdf;

    if (!light.isDelta())
        lightF *= SampleWarp::powerHeuristic(sample.pdf, bsdf.pdf(event));

    return lightF;
}

Vec3f PathTraceIntegrator::bsdfSample(const TangentFrame &frame,
                     const Primitive &light,
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
    if (bsdf.overridesMedia()) {
        if (geometricBackside)
            medium = bsdf.intMedium().get();
        else
            medium = bsdf.extMedium().get();
    }

    Ray ray = parentRay.scatter(event.info->p, wo, epsilon, event.pdf);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f e = attenuatedEmission(light, medium, -1.0f, data, info, bounce, ray);

    if (e == Vec3f(0.0f))
        return Vec3f(0.0f);

    Vec3f bsdfF = e*event.throughput;

    bsdfF *= SampleWarp::powerHeuristic(event.pdf, light.inboundPdf(data, info, event.info->p, wo));

    return bsdfF;
}

Vec3f PathTraceIntegrator::volumeLightSample(VolumeScatterEvent &event,
                    const Primitive &light,
                    const Medium *medium,
                    bool performMis,
                    int bounce,
                    const Ray &parentRay)
{
    LightSample sample(event.sampler, event.p);

    if (!light.sampleInboundDirection(sample))
        return Vec3f(0.0f);
    event.wo = sample.d;

    Vec3f f = medium->phaseEval(event);
    if (f == 0.0f)
        return Vec3f(0.0f);

    Ray ray = parentRay.scatter(sample.p, sample.d, 0.0f, sample.pdf);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f e = attenuatedEmission(light, medium, sample.dist, data, info, bounce, ray);
    if (e == 0.0f)
        return Vec3f(0.0f);

    Vec3f lightF = f*e/sample.pdf;

    if (!light.isDelta() && performMis)
        lightF *= SampleWarp::powerHeuristic(sample.pdf, medium->phasePdf(event));

    return lightF;
}

Vec3f PathTraceIntegrator::volumePhaseSample(const Primitive &light,
                    VolumeScatterEvent &event,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay)
{
    if (!medium->scatter(event))
        return Vec3f(0.0f);
    if (event.throughput == 0.0f)
        return Vec3f(0.0f);

    Ray ray = parentRay.scatter(event.p, event.wo, 0.0f, event.pdf);

    IntersectionTemporary data;
    IntersectionInfo info;
    Vec3f e = attenuatedEmission(light, medium, -1.0f, data, info, bounce, ray);

    if (e == Vec3f(0.0f))
        return Vec3f(0.0f);

    Vec3f phaseF = e*event.throughput;

    phaseF *= SampleWarp::powerHeuristic(event.pdf, light.inboundPdf(data, info, event.p, event.wo));

    return phaseF;
}

Vec3f PathTraceIntegrator::sampleDirect(const TangentFrame &frame,
                   const Primitive &light,
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

    result += lightSample(frame, light, bsdf, event, medium, bounce, epsilon, parentRay);
    if (!light.isDelta())
        result += bsdfSample(frame, light, bsdf, event, medium, bounce, epsilon, parentRay);

    return result;
}

Vec3f PathTraceIntegrator::volumeSampleDirect(const Primitive &light,
                    VolumeScatterEvent &event,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay)
{
    // TODO: Re-enable Mis suggestions? Might be faster, but can cause fireflies
    bool mis = true;//medium->suggestMis();

    Vec3f result = volumeLightSample(event, light, medium, mis, bounce, parentRay);
    if (!light.isDelta() && mis)
        result += volumePhaseSample(light, event, medium, bounce, parentRay);

    return result;
}

const Primitive *PathTraceIntegrator::chooseLight(SampleGenerator &sampler, const Vec3f &p, float &weight)
{
    if (_scene->lights().empty())
        return nullptr;
    if (_scene->lights().size() == 1) {
        weight = 1.0f;
        return _scene->lights()[0].get();
    }

    float total = 0.0f;
    unsigned numNonNegative = 0;
    for (size_t i = 0; i < _lightPdf.size(); ++i) {
        _lightPdf[i] = _scene->lights()[i]->approximateRadiance(p);
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
            float uniformWeight = total/numNonNegative;
            if (_lightPdf[i] < 0.0f) {
                _lightPdf[i] = uniformWeight;
                total += uniformWeight;
            }
        }
    }
    if (total == 0.0f)
        return nullptr;
    float t = sampler.next1D()*total;
    for (size_t i = 0; i < _lightPdf.size(); ++i) {
        if (t < _lightPdf[i] || i == _lightPdf.size() - 1) {
            weight = total/_lightPdf[i];
            return _scene->lights()[i].get();
        } else {
            t -= _lightPdf[i];
        }
    }
    return nullptr;
}

Vec3f PathTraceIntegrator::volumeEstimateDirect(VolumeScatterEvent &event,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay)
{
    float weight;
    const Primitive *light = chooseLight(*event.sampler, event.p, weight);
    if (light == nullptr)
        return Vec3f(0.0f);
    return volumeSampleDirect(*light, event, medium, bounce, parentRay)*weight;
}

Vec3f PathTraceIntegrator::estimateDirect(const TangentFrame &frame,
                     const Bsdf &bsdf,
                     SurfaceScatterEvent &event,
                     const Medium *medium,
                     int bounce,
                     float epsilon,
                     const Ray &parentRay)
{
    float weight;
    const Primitive *light = chooseLight(*event.sampler, event.info->p, weight);
    if (light == nullptr)
        return Vec3f(0.0f);
    return sampleDirect(frame, *light, bsdf, event, medium, bounce, epsilon, parentRay)*weight;
}

bool PathTraceIntegrator::handleVolume(SampleGenerator &sampler, UniformSampler &supplementalSampler,
           const Medium *&medium, int bounce, Ray &ray,
           Vec3f &throughput, Vec3f &emission, bool &wasSpecular, bool &hitSurface,
           Medium::MediumState &state)
{
    VolumeScatterEvent event(&sampler, &supplementalSampler, throughput, ray.pos(), ray.dir(), ray.farT());
    if (!medium->sampleDistance(event, state))
        return false;
    throughput *= event.throughput;
    event.throughput = Vec3f(1.0f);

    emission += throughput*medium->emission(event);

    if (!_enableVolumeLightSampling)
        wasSpecular = !hitSurface;

    if (event.t < event.maxT) {
        event.p += event.t*event.wi;

        if (_enableVolumeLightSampling) {
            wasSpecular = false;
            emission += throughput*volumeEstimateDirect(event, medium, bounce + 1, ray);
        }

        if (medium->absorb(event, state))
            return false;
        if (!medium->scatter(event))
            return false;
        ray = ray.scatter(event.p, event.wo, 0.0f, event.pdf);
        throughput *= event.throughput;
        hitSurface = false;
    } else {
        hitSurface = true;
    }

    return true;
}

bool PathTraceIntegrator::handleSurface(IntersectionTemporary &data, IntersectionInfo &info,
                   SampleGenerator &sampler, UniformSampler &supplementalSampler,
                   const Medium *&medium, int bounce, Ray &ray,
                   Vec3f &throughput, Vec3f &emission, bool &wasSpecular,
                   Medium::MediumState &state)
{
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

        if (!GeneralizedShadowRays)
            wasSpecular = true;
    } else {
        if (_enableLightSampling) {
            if ((wasSpecular || !info.primitive->isSamplable()) && bounce >= _minBounces)
                emission += info.primitive->emission(data, info)*throughput;

            if (bounce < _maxBounces - 1)
                emission += estimateDirect(event.frame, bsdf, event, medium, bounce + 1, info.epsilon, ray)*throughput;
        } else if (bounce >= _minBounces) {
            emission += info.primitive->emission(data, info)*throughput;
        }

        event.requestedLobe = BsdfLobes::AllLobes;
        if (!bsdf.sample(event))
            return false;

        pdf = event.pdf;
        wo = event.frame.toGlobal(event.wo);

        if (!isConsistent(event, wo))
            return false;

        throughput *= event.throughput;
        wasSpecular = event.sampledLobe.hasSpecular();
    }

    bool geometricBackside = (wo.dot(info.Ng) < 0.0f);
    const Medium *newMedium = medium;
    if (bsdf.overridesMedia()) {
        if (geometricBackside)
            newMedium = bsdf.intMedium().get();
        else
            newMedium = bsdf.extMedium().get();
    }
    state.reset();
    medium = newMedium;

    ray = ray.scatter(ray.hitpoint(), wo, info.epsilon, pdf);

    return true;
}

void PathTraceIntegrator::setScene(const TraceableScene *scene)
{
    _scene = scene;
    _lightPdf.resize(scene->lights().size());
}

void PathTraceIntegrator::fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
{
    JsonUtils::fromJson(v, "min_bounces", _minBounces);
    JsonUtils::fromJson(v, "max_bounces", _maxBounces);
    JsonUtils::fromJson(v, "enable_light_sampling", _enableLightSampling);
    JsonUtils::fromJson(v, "enable_volume_light_sampling", _enableVolumeLightSampling);
    JsonUtils::fromJson(v, "enable_consistency_checks", _enableConsistencyChecks);
    JsonUtils::fromJson(v, "enable_two_sided_shading", _enableTwoSidedShading);
}

rapidjson::Value PathTraceIntegrator::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));
    v.AddMember("type", "path_trace", allocator);
    v.AddMember("min_bounces", _minBounces, allocator);
    v.AddMember("max_bounces", _maxBounces, allocator);
    v.AddMember("enable_light_sampling", _enableLightSampling, allocator);
    v.AddMember("enable_volume_light_sampling", _enableVolumeLightSampling, allocator);
    v.AddMember("enable_consistency_checks", _enableConsistencyChecks, allocator);
    v.AddMember("enable_two_sided_shading", _enableTwoSidedShading, allocator);
    return std::move(v);
}

Vec3f PathTraceIntegrator::traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler)
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
    while (didHit && bounce < _maxBounces) {
        ray.advanceFootprint();

        if (medium && !handleVolume(sampler, supplementalSampler, medium, bounce, ray, throughput, emission, wasSpecular, hitSurface, state))
            break;

        if (hitSurface && !handleSurface(data, info, sampler, supplementalSampler, medium, bounce, ray, throughput, emission, wasSpecular, state))
            break;

        if (throughput.max() == 0.0f)
            break;

        float roulettePdf = throughput.max();
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
        if (bounce < _maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }
    if (!didHit && !medium && bounce >= _minBounces) {
        if (_scene->intersectInfinites(ray, data, info)) {
            if (_enableLightSampling) {
                if (bounce == 0 || wasSpecular || !info.primitive->isSamplable())
                    emission += throughput*info.primitive->emission(data, info);
            } else {
                emission += throughput*info.primitive->emission(data, info);
            }
        }
    }
    if (std::isnan(throughput.sum() + emission.sum()))
        return nanEnvDirColor;
    return min(emission, Vec3f(100.0f));

    } catch (std::runtime_error &e) {
        std::cout << tfm::format("Caught an internal error at pixel %s: %s", pixel, e.what()) << std::endl;

        return Vec3f(0.0f);
    }
}

Integrator *PathTraceIntegrator::cloneThreadSafe(uint32 /*threadId*/, const TraceableScene *scene) const
{
    PathTraceIntegrator *integrator = new PathTraceIntegrator(*this);
    integrator->setScene(scene);
    return integrator;
}

}

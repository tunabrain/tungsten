#ifndef PATHTRACEINTEGRATOR_HPP_
#define PATHTRACEINTEGRATOR_HPP_

#include <embree/common/ray.h>
#include <vector>
#include <memory>
#include <cmath>

#include "integrators/Integrator.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/SurfaceScatterEvent.hpp"
#include "sampling/VolumeScatterEvent.hpp"
#include "sampling/LightSample.hpp"
#include "sampling/Sample.hpp"

#include "volume/Medium.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "bsdfs/Bsdf.hpp"

#include "TraceableScene.hpp"
#include "cameras/Camera.hpp"

#define ENABLE_LIGHT_SAMPLING 1
#define ENABLE_VOLUME_LIGHT_SAMPLING 1

namespace Tungsten
{

class PathTraceIntegrator : public Integrator
{
    static constexpr float Epsilon = 5e-4f;
    static constexpr int MaxBounces = 64;

    const TraceableScene &_scene;

    std::vector<float> _lightPdf;

    Vec3f generalizedShadowRay(Ray &ray,
                               const Medium *medium,
                               const Primitive *endCap,
                               int bounce)
    {
        Primitive::IntersectionTemporary data;
        IntersectionInfo info;

        float initialFarT = ray.farT();
        Vec3f throughput(1.0f);
        do {
            if (_scene.intersect(ray, data, info) && info.primitive != endCap) {
                if (!info.primitive->bsdf()->flags().isForward()) {
                    float alpha = info.primitive->bsdf()->alpha(&info);
                    if (alpha < 1.0f)
                        throughput *= 1.0f - alpha;
                    else
                        return Vec3f(0.0f);
                }
                bounce++;
                if (bounce >= MaxBounces)
                    return Vec3f(0.0f);
            }
            if (medium)
                throughput *= medium->transmittance(VolumeScatterEvent(ray.pos(), ray.dir(), ray.farT()));
            if (info.primitive == nullptr || info.primitive == endCap)
                return throughput;
            if (info.primitive->hitBackside(data))
                medium = info.primitive->bsdf()->extMedium().get();
            else
                medium = info.primitive->bsdf()->intMedium().get();
            ray.setPos(ray.hitpoint());
            initialFarT -= ray.farT();
            ray.setNearT(Epsilon);
            ray.setFarT(initialFarT);
        } while(true);
        return Vec3f(0.0f);
    }

    Vec3f attenuatedEmission(const Primitive &light,
                             const Medium *medium,
                             const Vec3f &p, const Vec3f &d,
                             float expectedDist,
                             Primitive::IntersectionTemporary &data,
                             int bounce,
                             float tMin)
    {
        constexpr float fudgeFactor = 1.0f + 1e-3f;

        IntersectionInfo info;
        Ray ray(p, d, tMin);
        if (!light.intersect(ray, data) || ray.farT()*fudgeFactor < expectedDist)
            return Vec3f(0.0f);
        light.intersectionInfo(data, info);

        Vec3f transmittance = generalizedShadowRay(ray, medium, &light, bounce);
        if (transmittance == 0.0f)
            return Vec3f(0.0f);

        return transmittance*light.emission(data, info);
    }

    Vec3f lightSample(const TangentFrame &frame,
                      const Primitive &light,
                      const Bsdf &bsdf,
                      SurfaceScatterEvent &event,
                      int bounce)
    {
        LightSample sample(event.sampler, event.info->p);

        if (!light.sampleInboundDirection(sample))
            return Vec3f(0.0f);

        event.wo = frame.toLocal(sample.d);
        float geometricBackside = (sample.d.dot(event.info->Ng) < 0.0f);
        if (geometricBackside != (event.wo.z() < 0.0f))
            return Vec3f(0.0f);

        Medium *medium;
        if (geometricBackside)
            medium = bsdf.intMedium().get();
        else
            medium = bsdf.extMedium().get();

        event.requestedLobe = BsdfLobes::AllButSpecular;
        Vec3f f = bsdf.eval(event);
        if (f == 0.0f)
            return Vec3f(0.0f);

        Primitive::IntersectionTemporary data;
        Vec3f e = attenuatedEmission(light, medium, sample.p, sample.d, sample.dist, data, bounce, Epsilon);
        if (e == 0.0f)
            return Vec3f(0.0f);

        Vec3f lightF = f*e/sample.pdf;

        if (!light.isDelta())
            lightF *= Sample::powerHeuristic(sample.pdf, bsdf.pdf(event));

        return lightF;
    }

    Vec3f bsdfSample(const TangentFrame &frame,
                     const Primitive &light,
                     const Bsdf &bsdf,
                     SurfaceScatterEvent &event,
                     int bounce)
    {
        event.requestedLobe = BsdfLobes::AllButSpecular;
        if (!bsdf.sample(event))
            return Vec3f(0.0f);
        if (event.throughput == 0.0f)
            return Vec3f(0.0f);

        Vec3f wo = frame.toGlobal(event.wo);
        float geometricBackside = (wo.dot(event.info->Ng) < 0.0f);
        if (geometricBackside != (event.wo.z() < 0.0f))
            return Vec3f(0.0f);

        Medium *medium;
        if (geometricBackside)
            medium = bsdf.intMedium().get();
        else
            medium = bsdf.extMedium().get();

        Primitive::IntersectionTemporary data;
        Vec3f e = attenuatedEmission(light, medium, event.info->p, wo, -1.0f, data, bounce, Epsilon);

        if (e == Vec3f(0.0f))
            return Vec3f(0.0f);

        Vec3f bsdfF = e*event.throughput;

        bsdfF *= Sample::powerHeuristic(event.pdf, light.inboundPdf(data, event.info->p, wo));

        return bsdfF;
    }

    Vec3f volumeLightSample(VolumeScatterEvent &event, const Primitive &light, const Medium *medium, bool performMis, int bounce)
    {
        LightSample sample(event.sampler, event.p);

        if (!light.sampleInboundDirection(sample))
            return Vec3f(0.0f);
        event.wo = sample.d;

        Vec3f f = medium->eval(event);
        if (f == 0.0f)
            return Vec3f(0.0f);

        Primitive::IntersectionTemporary data;
        Vec3f e = attenuatedEmission(light, medium, sample.p, sample.d, sample.dist, data, bounce, 0.0f);
        if (e == 0.0f)
            return Vec3f(0.0f);

        Vec3f lightF = f*e/sample.pdf;

        if (!light.isDelta() && performMis)
            lightF *= Sample::powerHeuristic(sample.pdf, medium->pdf(event));

        return lightF;
    }

    Vec3f volumePhaseSample(const Primitive &light, VolumeScatterEvent &event, const Medium *medium, int bounce)
    {
        if (!medium->scatter(event))
            return Vec3f(0.0f);
        if (event.throughput == 0.0f)
            return Vec3f(0.0f);

        Primitive::IntersectionTemporary data;
        Vec3f e = attenuatedEmission(light, medium, event.p, event.wo, -1.0f, data, bounce, 0.0f);

        if (e == Vec3f(0.0f))
            return Vec3f(0.0f);

        Vec3f phaseF = e*event.throughput;

        phaseF *= Sample::powerHeuristic(event.pdf, light.inboundPdf(data, event.p, event.wo));

        return phaseF;
    }

    Vec3f sampleDirect(const TangentFrame &frame,
                       const Primitive &light,
                       const Bsdf &bsdf,
                       SurfaceScatterEvent &event,
                       int bounce)
    {
        Vec3f result(0.0f);

        if (bsdf.flags().isPureSpecular() || bsdf.flags().isForward())
            return Vec3f(0.0f);

        result += lightSample(frame, light, bsdf, event, bounce);
        if (!light.isDelta())
            result += bsdfSample(frame, light, bsdf, event, bounce);

        return result;
    }

    Vec3f volumeSampleDirect(const Primitive &light, VolumeScatterEvent &event, const Medium *medium, int bounce)
    {
        bool mis = true;//medium->suggestMis();

        Vec3f result = volumeLightSample(event, light, medium, mis, bounce);
        if (!light.isDelta() && mis)
            result += volumePhaseSample(light, event, medium, bounce);

        return result;
    }

    const Primitive *chooseLight(SampleGenerator &sampler, const Vec3f &p, float &weight)
    {
        if (_scene.lights().empty())
            return nullptr;
        if (_scene.lights().size() == 1) {
            weight = 1.0f;
            return _scene.lights()[0].get();
        }

        float total = 0.0f;
        unsigned numNonNegative = 0;
        for (size_t i = 0; i < _lightPdf.size(); ++i) {
            _lightPdf[i] = _scene.lights()[i]->approximateRadiance(p);
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
                return _scene.lights()[i].get();
            } else {
                t -= _lightPdf[i];
            }
        }
        return nullptr;
    }

    Vec3f volumeEstimateDirect(VolumeScatterEvent &event, const Medium *medium, int bounce)
    {
        float weight;
        const Primitive *light = chooseLight(*event.sampler, event.p, weight);
        if (light == nullptr)
            return Vec3f(0.0f);
        return volumeSampleDirect(*light, event, medium, bounce)*weight;
    }

    Vec3f estimateDirect(const TangentFrame &frame,
                         const Bsdf &bsdf,
                         SurfaceScatterEvent &event,
                         int bounce)
    {
        float weight;
        const Primitive *light = chooseLight(*event.sampler, event.info->p, weight);
        if (light == nullptr)
            return Vec3f(0.0f);
        return sampleDirect(frame, *light, bsdf, event, bounce)*weight;
    }

public:
    PathTraceIntegrator(const TraceableScene &scene)
    : _scene(scene),
      _lightPdf(scene.lights().size())
    {
    }

    bool handleVolume(SampleGenerator &sampler, UniformSampler &supplementalSampler,
               const Medium *&medium, int bounce, Ray &ray,
               Vec3f &throughput, Vec3f &emission, bool &wasSpecular, bool &hitSurface,
               Medium::MediumState &state)
    {
        VolumeScatterEvent event(&sampler, &supplementalSampler, ray.pos(), ray.dir(), ray.farT());
        if (!medium->sampleDistance(event, state))
            return false;
        throughput *= event.throughput;
        event.throughput = Vec3f(1.0f);

        emission += throughput*medium->emission(event);

#if !ENABLE_VOLUME_LIGHT_SAMPLING
        wasSpecular = !hitSurface;
#endif

        if (event.t < event.maxT) {
            event.p += event.t*event.wi;

#if ENABLE_VOLUME_LIGHT_SAMPLING
            wasSpecular = false;
            emission += throughput*volumeEstimateDirect(event, medium, bounce + 1);
#endif

            if (medium->absorb(event, state))
                return false;
            if (!medium->scatter(event))
                return false;
            throughput *= event.throughput;
            ray = Ray(event.p, event.wo, 0.0f);
            hitSurface = false;
        } else {
            hitSurface = true;
        }

        return true;
    }

    bool handleSurface(Primitive::IntersectionTemporary &data, IntersectionInfo &info,
                       SampleGenerator &sampler, UniformSampler &supplementalSampler,
                       const Medium *&medium, int bounce, Ray &ray,
                       Vec3f &throughput, Vec3f &emission, bool &wasSpecular,
                       Medium::MediumState &state)
    {
        const Bsdf &bsdf = *info.primitive->bsdf();

        Vec3f wo;
        if (supplementalSampler.next1D() >= bsdf.alpha(&info)) {
            wo = ray.dir();
        } else {
            TangentFrame frame;
            bsdf.setupTangentFrame(*info.primitive, data, info, frame);

            SurfaceScatterEvent event(&info, &sampler, &supplementalSampler, frame.toLocal(-ray.dir()), BsdfLobes::AllLobes);

#if ENABLE_LIGHT_SAMPLING
            if (wasSpecular || !info.primitive->isSamplable())
                emission += info.primitive->emission(data, info)*throughput;

            if (bounce < MaxBounces - 1)
                emission += estimateDirect(frame, bsdf, event, bounce + 1)*throughput;
#else
            emission += info.primitive->emission(data, info)*throughput;
#endif

            event.requestedLobe = BsdfLobes::AllLobes;
            if (!bsdf.sample(event))
                return false;

            wo = frame.toGlobal(event.wo);

            if ((wo.dot(info.Ng) < 0.0f) != (event.wo.z() < 0.0f))
                return false;

            throughput *= event.throughput;
            if (!event.sampledLobe.isForward())
                wasSpecular = event.sampledLobe.hasSpecular();
        }

        bool geometricBackside = (wo.dot(info.Ng) < 0.0f);
        Medium *newMedium;
        if (geometricBackside)
            newMedium = bsdf.intMedium().get();
        else
            newMedium = bsdf.extMedium().get();
        if (newMedium != medium)
            state.reset();
        medium = newMedium;
        ray = Ray(ray.hitpoint(), wo, Epsilon);
        return true;
    }

    Vec3f traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler) final override
    {
        Ray ray(_scene.cam().generateSample(pixel, sampler));

        Primitive::IntersectionTemporary data;
        Medium::MediumState state;
        IntersectionInfo info;
        Vec3f throughput(1.0f);
        Vec3f emission(0.0f);
        const Medium *medium = _scene.cam().medium().get();

        int bounce = 0;
        bool didHit = _scene.intersect(ray, data, info);
        bool wasSpecular = true;
        bool hitSurface = true;
        while (didHit && bounce < MaxBounces) {
            if (medium && !handleVolume(sampler, supplementalSampler, medium, bounce, ray, throughput, emission, wasSpecular, hitSurface, state))
                break;

            if (hitSurface && !handleSurface(data, info, sampler, supplementalSampler, medium, bounce, ray, throughput, emission, wasSpecular, state))
                break;

            if (throughput.max() == 0.0f)
                break;

            float roulettePdf = throughput.max();
            if (bounce > 5 && roulettePdf < 0.1f) {
                if (supplementalSampler.next1D() < roulettePdf)
                    throughput /= roulettePdf;
                else
                    break;
            }

            bounce++;
            if (bounce < MaxBounces)
                didHit = _scene.intersect(ray, data, info);
        }
        if (!didHit && !medium) {
            if (_scene.intersectInfinites(ray, data, info)) {
#if ENABLE_LIGHT_SAMPLING
                if (bounce == 0 || wasSpecular || !info.primitive->isSamplable())
                    emission += throughput*info.primitive->emission(data, info);
#else
                emission += throughput*info.primitive->emission(data, info);
#endif
            }
        }
        return emission;
    }
};

}

#endif /* PATHTRACEINTEGRATOR_HPP_ */

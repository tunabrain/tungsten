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

namespace Tungsten
{

class PathTraceIntegrator : public Integrator
{
    static constexpr float Epsilon = 5e-4f;
    static constexpr bool GeneralizedShadowRays = true;

    const TraceableScene *_scene;

    bool _enableLightSampling;
    bool _enableVolumeLightSampling;
    int _maxBounces;
    std::vector<float> _lightPdf;

    Vec3f generalizedShadowRay(Ray &ray,
                               const Medium *medium,
                               const Primitive *endCap,
                               int bounce)
    {
        if (!GeneralizedShadowRays)
            return _scene->occluded(ray) ? Vec3f(0.0f) : Vec3f(1.0f);
        Primitive::IntersectionTemporary data;
        IntersectionInfo info;

        float initialFarT = ray.farT();
        Vec3f throughput(1.0f);
        do {
            if (_scene->intersect(ray, data, info) && info.primitive != endCap) {
                if (!info.primitive->bsdf()->flags().isForward()) {
                    float alpha = info.primitive->bsdf()->alpha(&info);
                    if (alpha == 1.0f)
                        return Vec3f(0.0f);
                    Vec3f transmittance = info.primitive->bsdf()->transmittance(&info)*(1.0f - alpha);
                    if (transmittance == 0.0f)
                        return Vec3f(0.0f);
                    else
                        throughput *= transmittance;
                }
                bounce++;
                if (bounce >= _maxBounces)
                    return Vec3f(0.0f);
            }
            if (medium)
                throughput *= medium->transmittance(VolumeScatterEvent(ray.pos(), ray.dir(), ray.farT()));
            if (info.primitive == nullptr || info.primitive == endCap)
                return throughput;
            if (info.primitive->bsdf()->overridesMedia()) {
                if (info.primitive->hitBackside(data))
                    medium = info.primitive->bsdf()->extMedium().get();
                else
                    medium = info.primitive->bsdf()->intMedium().get();
            }
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
                      const Medium *medium,
                      int bounce)
    {
        LightSample sample(event.sampler, event.info->p);

        if (!light.sampleInboundDirection(sample))
            return Vec3f(0.0f);

        event.wo = frame.toLocal(sample.d);
        float geometricBackside = (sample.d.dot(event.info->Ng) < 0.0f);
        if (geometricBackside != (event.wo.z() < 0.0f))
            return Vec3f(0.0f);

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
                     const Medium *medium,
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

        if (bsdf.overridesMedia()) {
            if (geometricBackside)
                medium = bsdf.intMedium().get();
            else
                medium = bsdf.extMedium().get();
        }

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
                       const Medium *medium,
                       int bounce)
    {
        Vec3f result(0.0f);

        if (bsdf.flags().isPureSpecular() || bsdf.flags().isForward())
            return Vec3f(0.0f);

        result += lightSample(frame, light, bsdf, event, medium, bounce);
        if (!light.isDelta())
            result += bsdfSample(frame, light, bsdf, event, medium, bounce);

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
#if 1
                total += _lightPdf[i];
#else
                if (_lightPdf[i] > 0.0f) {
                    total += 1.0f;
                    _lightPdf[i] = 1.0f;
                }
#endif
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
                         const Medium *medium,
                         int bounce)
    {
        float weight;
        const Primitive *light = chooseLight(*event.sampler, event.info->p, weight);
        if (light == nullptr)
            return Vec3f(0.0f);
        return sampleDirect(frame, *light, bsdf, event, medium, bounce)*weight;
    }

public:
    PathTraceIntegrator()
    : _scene(nullptr),
      _enableLightSampling(true),
      _enableVolumeLightSampling(true),
      _maxBounces(64)
    {
    }

    void setScene(const TraceableScene *scene)
    {
        _scene = scene;
        _lightPdf.resize(scene->lights().size());
    }

    bool handleVolume(SampleGenerator &sampler, UniformSampler &supplementalSampler,
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
                emission += throughput*volumeEstimateDirect(event, medium, bounce + 1);
            }

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
        if (bsdf.flags().isForward()) {
            wo = ray.dir();
            if (!GeneralizedShadowRays)
                wasSpecular = true;
        } else if (sampler.next1D() >= bsdf.alpha(&info)) {
            wo = ray.dir();
            throughput *= bsdf.transmittance(&info);
            if (!GeneralizedShadowRays)
                wasSpecular = true;
        } else {
            TangentFrame frame;
            bsdf.setupTangentFrame(*info.primitive, data, info, frame);

            if (std::isnan(frame.normal.sum())) {
                FAIL("NAN frame! %s %s %s", frame.normal, frame.tangent, frame.bitangent);
            }

            if (frame.normal.dot(ray.dir()) > 0.0f && !bsdf.flags().isTransmissive()) {
                info.Ng = -info.Ng;
                info.Ns = -info.Ns;
                frame.normal = -frame.normal;
                frame.tangent = -frame.tangent;
            }

            SurfaceScatterEvent event(&info, &sampler, &supplementalSampler, frame.toLocal(-ray.dir()), BsdfLobes::AllLobes);

            if (_enableLightSampling) {
                if ((wasSpecular || !info.primitive->isSamplable()) && !info.primitive->disableReflectedEmission())
                    emission += info.primitive->emission(data, info)*throughput;

                if (bounce < _maxBounces - 1)
                    emission += estimateDirect(frame, bsdf, event, medium, bounce + 1)*throughput;
            } else {
                emission += info.primitive->emission(data, info)*throughput;
            }

            event.requestedLobe = BsdfLobes::AllLobes;
            if (!bsdf.sample(event))
                return false;

            wo = frame.toGlobal(event.wo);

            if ((wo.dot(info.Ng) < 0.0f) != (event.wo.z() < 0.0f))
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
        //if (newMedium != medium)
            state.reset();
        medium = newMedium;
        ray = Ray(ray.hitpoint(), wo, Epsilon);
        return true;
    }

    Vec3f traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler) final override
    {
//      const Vec3f nanDirColor(1000.0f, 0.0f, 0.0f);
//      const Vec3f nanEnvDirColor(0.0f, 0.0f, 1000.0f);
//      const Vec3f nanBsdfColor(0.0f, 1000.0f, 0.0f);
        const Vec3f nanDirColor(0.0f, 0.0f, 0.0f);
        const Vec3f nanEnvDirColor(0.0f, 0.0f, 0.0f);
        const Vec3f nanBsdfColor(0.0f, 0.0f, 0.0f);

        try {

        Ray ray;
        Vec3f throughput(1.0f);
        if (!_scene->cam().generateSample(pixel, sampler, throughput, ray))
            return Vec3f(0.0f);

        Primitive::IntersectionTemporary data;
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
        if (!didHit && !medium) {
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

    virtual Integrator *cloneThreadSafe(uint32 /*threadId*/, const TraceableScene *scene) const
    {
        PathTraceIntegrator *integrator = new PathTraceIntegrator(*this);
        integrator->setScene(scene);
        return integrator;
    }

    virtual Integrator *clone() const
    {
        return new PathTraceIntegrator(*this);
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
    {
        JsonUtils::fromJson(v, "max_bounces", _maxBounces);
        JsonUtils::fromJson(v, "enable_light_sampling", _enableLightSampling);
        JsonUtils::fromJson(v, "enable_volume_light_sampling", _enableVolumeLightSampling);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(JsonSerializable::toJson(allocator));
        v.AddMember("type", "path_trace", allocator);
        v.AddMember("max_bounces", _maxBounces, allocator);
        v.AddMember("enable_light_sampling", _enableLightSampling, allocator);
        v.AddMember("enable_volume_light_sampling", _enableVolumeLightSampling, allocator);
        return std::move(v);
    }
};

}

#endif /* PATHTRACEINTEGRATOR_HPP_ */

#ifndef PATHTRACEINTEGRATOR_HPP_
#define PATHTRACEINTEGRATOR_HPP_

#include <embree/common/ray.h>
#include <vector>
#include <memory>
#include <cmath>

#include "integrators/Integrator.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/ScatterEvent.hpp"
#include "sampling/LightSample.hpp"
#include "sampling/Sample.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "bsdfs/Bsdf.hpp"

#include "TraceableScene.hpp"
#include "cameras/Camera.hpp"

#define ENABLE_LIGHT_SAMPLING 1

namespace Tungsten
{

class PathTraceIntegrator : public Integrator
{
    static constexpr float Epsilon = 5e-4f;
    static constexpr int MaxBounces = 64;

    const TraceableScene &_scene;

    std::vector<float> _lightPdfs;

    Vec3f lightSample(const TangentFrame &frame,
                      const Primitive &light,
                      const Bsdf &bsdf,
                      SurfaceScatterEvent &event)
    {
        LightSample sample(event.sampler, event.info.p);

        if (!light.sampleInboundDirection(sample))
            return Vec3f(0.0f);

        event.wo = frame.toLocal(sample.d);
        if ((sample.d.dot(event.info.Ng) < 0.0f) != (event.wo.z() < 0.0f))
            return Vec3f(0.0f);

        event.requestedLobe = BsdfLobes::AllButSpecular;
        Vec3f f = bsdf.eval(event);
        if (f == 0.0f)
            return Vec3f(0.0f);

        if (_scene.occluded(Ray(sample.p, sample.d, Epsilon, sample.dist*(1.0f - 1e-4f))))
            return Vec3f(0.0f);

        Vec3f lightF = f*light.bsdf()->emission()/sample.pdf;

        if (!light.isDelta())
            lightF *= Sample::powerHeuristic(sample.pdf, bsdf.pdf(event));

        return lightF;
    }

    Vec3f bsdfSample(const TangentFrame &frame,
                     const Primitive &light,
                     const Bsdf &bsdf,
                     SurfaceScatterEvent &event)
    {
        event.requestedLobe = BsdfLobes::AllButSpecular;
        if (!bsdf.sample(event))
            return Vec3f(0.0f);
        if (event.throughput == 0.0f)
            return Vec3f(0.0f);

        Vec3f wo = frame.toGlobal(event.wo);
        if ((wo.dot(event.info.Ng) < 0.0f) != (event.wo.z() < 0.0f))
            return Vec3f(0.0f);

        Primitive::IntersectionTemporary data;
        Ray ray(event.info.p, wo, Epsilon);
        if (!light.intersect(ray, data) || light.hitBackside(data))
            return Vec3f(0.0f);

        ray.setFarT(ray.farT()*(1.0f - 1e-4f));
        if (_scene.occluded(ray))
            return Vec3f(0.0f);

        Vec3f bsdfF = light.bsdf()->emission()*event.throughput;

        bsdfF *= Sample::powerHeuristic(event.pdf, light.inboundPdf(data, event.info.p, wo));

        return bsdfF;
    }

    Vec3f sampleDirect(const TangentFrame &frame,
                       const Primitive &light,
                       const Bsdf &bsdf,
                       SurfaceScatterEvent &event)
    {
        Vec3f result(0.0f);

        if (bsdf.flags().isPureSpecular())
            return Vec3f(0.0f);

        result += lightSample(frame, light, bsdf, event);
        if (!light.isDelta())
            result += bsdfSample(frame, light, bsdf, event);

        return result;
    }

    Vec3f estimateDirect(const TangentFrame &frame,
                         const Bsdf &bsdf,
                         SurfaceScatterEvent &event)
    {
        if (_scene.lights().empty())
            return Vec3f(0.0f);
        return sampleDirect(frame, *_scene.lights()[0], bsdf, event);
//      if (_scene.lights().size() == 1)
//          return sampleDirect(frame, *_scene.lights()[0], bsdf, event, p);
//
//      float total = 0.0f;
//      for (size_t i = 0; i < _lightPdfs.size(); ++i) {
//          _lightPdfs[i] = _scene.lights()[i]->approximateRadiance(p).max();
//          total += _lightPdfs[i];
//      }
//      if (total == 0.0f)
//          return Vec3f(0.0f);
//      float t = lightXi.z()*total;
//      for (size_t i = 0; i < _lightPdfs; ++i) {
//          if (t < _lightPdfs[i] || i == _lightPdfs - 1)
//              return sampleDirect(frame, *_scene.lights()[i], bsdf, event, p, Ns, bsdfXi, lightXi.xy())*total/_lightPdfs[i];
//          else
//              t -= _lightPdfs[i];
//      }
//      return Vec3f(0.0f);
    }

public:
    PathTraceIntegrator(const TraceableScene &scene)
    : _scene(scene)
    {
    }

    Vec3f traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler) final override
    {
        Ray ray(_scene.cam().generateSample(pixel, sampler));

        Primitive::IntersectionTemporary data;
        IntersectionInfo info;
        Vec3f throughput(1.0f);
        Vec3f emission(0.0f);

        int bounce = 0;
        bool didHit = _scene.intersect(ray, data, info);
        bool wasSpecular = false;
        while (didHit && bounce < MaxBounces) {
            Vec3f p(ray.hitpoint());
            Vec3f w(-ray.dir());

            const Bsdf &bsdf = *info.primitive->bsdf();

            if (supplementalSampler.next1D() >= bsdf.alpha(info)) {
                ray = Ray(p, ray.dir(), Epsilon);
                didHit = _scene.intersect(ray, data, info);
                continue;
            }

            TangentFrame frame;
            bsdf.setupTangentFrame(*info.primitive, data, info, frame);

            SurfaceScatterEvent event(info, sampler, supplementalSampler, frame.toLocal(w), BsdfLobes::AllLobes);

#if ENABLE_LIGHT_SAMPLING
            if (event.wi.z() >= 0.0f && (bounce == 0 || wasSpecular))
                emission += bsdf.emission()*throughput;

            if (bounce < MaxBounces - 1)
                emission += estimateDirect(frame, bsdf, event)*throughput;
#else
            if (event.wi.z() >= 0.0f)
                emission += bsdf.emission()*throughput;
#endif

            event.requestedLobe = BsdfLobes::AllLobes;
            if (!bsdf.sample(event))
                break;

            Vec3f wo = frame.toGlobal(event.wo);

            if ((wo.dot(info.Ng) < 0.0f) != (event.wo.z() < 0.0f))
                break;

            throughput *= event.throughput;

            if (throughput.max() == 0.0f)
                break;

            float roulettePdf = throughput.max();
            if (bounce > 5 && roulettePdf < 0.1f) {
                if (supplementalSampler.next1D() < roulettePdf)
                    throughput /= roulettePdf;
                else
                    break;
            }

            wasSpecular = event.sampledLobe.hasSpecular();

            bounce++;
            if (bounce < MaxBounces) {
                ray = Ray(p, wo, Epsilon);
                didHit = _scene.intersect(ray, data, info);
            }
        }
        //if (!didHit)
        //  emission += throughput;
        return emission;
    }
};

}

#endif /* PATHTRACEINTEGRATOR_HPP_ */

#ifndef RAYSTREAMINTEGRATOR_HPP_
#define RAYSTREAMINTEGRATOR_HPP_

#include <embree/common/ray.h>
#include <vector>
#include <memory>
#include <cmath>

#include "integrators/Integrator.hpp"

#include "primitives/PackedGeometry.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/ScatterEvent.hpp"
#include "sampling/LightSample.hpp"

#include "lights/EnvironmentLight.hpp"
#include "lights/Light.hpp"

#include "math/TangentSpace.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "Camera.hpp"

#include <fstream>


namespace Tungsten
{

class RayStreamIntegrator : public Integrator
{
    static constexpr int SensorDimensions = 2;
    static constexpr int DimensionsPerBounce = 10;

    static constexpr float Epsilon = 5e-3f;
    static constexpr int MaxBounces = 4;

    const PackedGeometry &_mesh;
    const std::vector<std::shared_ptr<Light>> &_lights;
    std::shared_ptr<Light> _environmentLight;
    std::vector<float> _lightPdfs;

    embree::Vec3fa toE(const Vec3f &v) const
    {
        return embree::Vec3fa(v.x(), v.y(), v.z());
    }

    Vec3f fromE(const embree::Vec3fa v) const
    {
        return Vec3f(v.x, v.y, v.z);
    }

    struct Ray {
        Vec3f o;
        Vec3f d;
        float tMin;
        float tMax;
        float u, v;
        uint32 hit;
    };


    std::vector<Ray> _primaryRays, _indirectRays;

    void intersect(const embree::RTCIntersector1 *isector, embree::Ray &ray, int bounce)
    {
        isector->intersect(ray);
        Ray r{fromE(ray.org), fromE(ray.dir), ray.tnear, ray.tfar, ray.u, ray.v, uint32(ray.id0)};
        if (bounce == 0)
            _primaryRays.push_back(r);
        else
            _indirectRays.push_back(r);
    }


    float powerHeuristic(float pdf0, float pdf1) const
    {
        return (pdf0*pdf0)/(pdf0*pdf0 + pdf1*pdf1);
    }

    Vec3f lightSample(const TangentSpace &frame,
                      const Light &light,
                      const Bsdf &bsdf,
                      const Vec3f &p,
                      const Vec3f &n,
                      const Vec3f &wo,
                      const Vec2f &xi)
    {
        LightSample sample;
        sample.n = n;
        sample.p = p;
        sample.xi = xi;

        if (!light.sample(sample))
            return Vec3f(0.0f);

        Vec3f wi = frame.toLocal(sample.w);

        Vec3f f = bsdf.eval(wo, wi);
        if (f == 0.0f)
            return Vec3f(0.0f);

        embree::Ray ray(toE(sample.p), toE(sample.w), Epsilon, sample.r);
        if (_mesh.intersector()->occluded(ray))
            return Vec3f(0.0f);

        Vec3f lightF = f*std::abs(wi.z())*sample.L/sample.pdf;

        if (!light.isDelta())
            lightF *= powerHeuristic(sample.pdf, bsdf.pdf(wo, wi));

        return lightF;
    }

    Vec3f bsdfSample(const TangentSpace &frame,
                     const Light &light,
                     const Bsdf &bsdf,
                     const ScatterEvent &event,
                     const Vec3f &p,
                     const Vec3f &Ns,
                     const Vec3f &xi)
    {
        ScatterEvent sample(event);
        sample.xi = xi;

        if (!bsdf.sample(sample))
            return Vec3f(0.0f);
        if (sample.throughput == 0.0f)
            return Vec3f(0.0f);
        if (!sample.isConsistent())
            return Vec3f(0.0f);

        Vec3f wi = frame.toGlobal(sample.wi);
        float t;
        Vec3f q;
        if (!light.intersect(p, wi, t, q))
            return Vec3f(0.0f);

        embree::Ray ray(toE(p), toE(wi), Epsilon, t);
        if (_mesh.intersector()->occluded(ray))
            return Vec3f(0.0f);

        Vec3f bsdfF = light.eval(-wi)*std::abs(sample.wi.z())*sample.throughput/sample.pdf;

        if (!bsdf.flags().isSpecular())
            bsdfF *= powerHeuristic(sample.pdf, light.pdf(p, Ns, wi));

        return bsdfF;
    }

    Vec3f sampleDirect(const TangentSpace &frame,
                       const Light &light,
                       const Bsdf &bsdf,
                       const ScatterEvent &event,
                       const Vec3f &p,
                       const Vec3f &Ns,
                       const Vec3f &bsdfXi,
                       const Vec2f &lightXi)
    {
        Vec3f result;

        if (bsdf.flags().isSpecular())
            return Vec3f(0.0f);

        if (!bsdf.flags().isSpecular())
            result += lightSample(frame, light, bsdf, p, Ns, event.wo, lightXi);
        if (!light.isDelta())
            result += bsdfSample(frame, light, bsdf, event, p, Ns, bsdfXi);

        return result;
    }

    Vec3f estimateDirect(const TangentSpace &frame,
                         const Bsdf &bsdf,
                         const ScatterEvent &event,
                         const Vec3f &p,
                         const Vec3f &Ns,
                         const Vec3f &bsdfXi,
                         const Vec3f &lightXi)
    {
        if (_lights.size() == 1)
            return sampleDirect(frame, *_lights[0], bsdf, event, p, Ns, bsdfXi, lightXi.xy());

        float total = 0.0f;
        for (size_t i = 0; i < _lights.size(); ++i) {
            _lightPdfs[i] = _lights[i]->approximateRadiance(p).max();
            total += _lightPdfs[i];
        }
        if (total == 0.0f)
            return Vec3f(0.0f);
        float t = lightXi.z()*total;
        for (size_t i = 0; i < _lights.size(); ++i) {
            if (t < _lightPdfs[i] || i == _lights.size() - 1)
                return sampleDirect(frame, *_lights[i], bsdf, event, p, Ns, bsdfXi, lightXi.xy())*total/_lightPdfs[i];
            else
                t -= _lightPdfs[i];
        }
        return Vec3f(0.0f);
    }

public:
    RayStreamIntegrator(const PackedGeometry &mesh, const std::vector<std::shared_ptr<Light>> &lights)
    : _mesh(mesh), _lights(lights), _lightPdfs(lights.size())
    {
        for (const std::shared_ptr<Light> &l : lights)
            if (dynamic_cast<EnvironmentLight *>(l.get()))
                _environmentLight = l;
    }

    ~RayStreamIntegrator()
    {
        std::ofstream out1("../BVH/Coherent.rays", std::ios_base::out | std::ios_base::binary);
        std::ofstream out2("../BVH/Incoherent.rays", std::ios_base::out | std::ios_base::binary);
        FileUtils::streamWrite(out1, _primaryRays);
        FileUtils::streamWrite(out2, _indirectRays);
    }

    Vec3f traceSample(const Camera &cam, Vec2u pixel, SampleGenerator &sampler) final override
    {
        Vec3f dir(cam.generateSample(pixel, Vec2f(sampler.fSample(0), sampler.fSample(1))));

        const embree::RTCIntersector1 *isector = _mesh.intersector();
        embree::Ray ray(toE(cam.pos()), toE(dir));
        intersect(isector, ray, 0);

        ScatterEvent event;
        Vec3f throughput(1.0f);
        Vec3f emission;

        int bounce = 0;
        while (ray && bounce++ < MaxBounces) {
            uint32_t dim = bounce*DimensionsPerBounce + SensorDimensions;

            Vec3f p(fromE(ray.org + ray.tfar*ray.dir));
            Vec3f w(-fromE(ray.dir));
            Vec3f Ng(fromE(ray.Ng));
            Vec2f lambda(ray.u, ray.v);
            Ng.normalize();

            const Triangle &triangle = _mesh.tris()[ray.id0];
            const Material &material = _mesh.materials()[triangle.material()];
            Vec2f uv = triangle.uvAt(lambda);

            if (sampler.fSample(dim) > material.alpha(uv)) {
                ray = embree::Ray(toE(p), ray.dir, Epsilon);
                intersect(isector, ray, bounce - 1);
                bounce--;
                continue;
            }

            Vec3f Ns;
            if (ray.id1 & PackedGeometry::FlatFlag)
                Ns = Ng;
            else
                Ns = triangle.normalAt(lambda).normalized();

            if (Ng.dot(w) < 0.0f)
                Ng = -Ng;
            if (Ns.dot(w) < 0.0f)
                Ns = -Ns;

            TangentSpace frame(Ns);

            emission += material.emission()*throughput;

            Vec3f bsdfXi0 = Vec3f(sampler.fSample(dim + 1), sampler.fSample(dim + 2), sampler.fSample(dim + 3));
            Vec3f bsdfXi1 = Vec3f(sampler.fSample(dim + 4), sampler.fSample(dim + 5), sampler.fSample(dim + 6));
            Vec3f lightXi = Vec3f(sampler.fSample(dim + 7), sampler.fSample(dim + 8), sampler.fSample(dim + 9));

            event.xi = bsdfXi0;
            event.wo = frame.toLocal(w);
            event.Ng = frame.toLocal(Ng);

            throughput *= material.color(uv);
            if (!_lights.empty())
                emission += throughput*estimateDirect(frame, *material.bsdf(), event, p, Ns, bsdfXi1, lightXi);

            if (!material.bsdf()->sample(event))
                break;
            if (!event.isConsistent())
                break;

            if (event.switchHemisphere)
                event.space = triangle.other(event.space);

            Vec3f wi = frame.toGlobal(event.wi);

            throughput *= std::abs(event.wi.z())*event.throughput/event.pdf;

            if (std::isnan(throughput.x()) || std::isnan(throughput.y()) || std::isnan(throughput.z())) {
                DBG("NAN!!! %f %f %f\n", throughput.x(), throughput.y(), throughput.z());
            }

            if (throughput.max() == 0.0f)
                break;

            if (bounce < MaxBounces) {
                ray = embree::Ray(toE(p), toE(wi), Epsilon);
                intersect(isector, ray, bounce - 1);
            }
        }

        if (_environmentLight && bounce == 0)
            emission += throughput*_environmentLight->eval(fromE(ray.dir));

        if (std::isnan(emission.x()) || std::isnan(emission.y()) || std::isnan(emission.z())) {
            DBG("NAN: %f %f %f\n", emission.x(), emission.y(), emission.z());
            return Vec3f(0.0f);
        }

        return emission;
    }
};

}

#endif /* RAYSTREAMINTEGRATOR_HPP_ */

#ifndef PATHTRACEINTEGRATOR_HPP_
#define PATHTRACEINTEGRATOR_HPP_

#include "samplerecords/SurfaceScatterEvent.hpp"
#include "samplerecords/VolumeScatterEvent.hpp"
#include "samplerecords/LightSample.hpp"

#include "integrators/Integrator.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"
#include "sampling/SampleWarp.hpp"

#include "renderer/TraceableScene.hpp"

#include "cameras/Camera.hpp"

#include "volume/Medium.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "bsdfs/Bsdf.hpp"

#include <vector>
#include <memory>
#include <cmath>

namespace Tungsten {

class PathTraceIntegrator : public Integrator
{
    static CONSTEXPR bool GeneralizedShadowRays = true;

    const TraceableScene *_scene;

    bool _enableLightSampling;
    bool _enableVolumeLightSampling;
    bool _enableConsistencyChecks;
    bool _enableTwoSidedShading;
    int _minBounces;
    int _maxBounces;
    std::vector<float> _lightPdf;
    uint32 _threadId;

    SurfaceScatterEvent makeLocalScatterEvent(IntersectionTemporary &data, IntersectionInfo &info,
            Ray &ray, SampleGenerator *sampler, UniformSampler *supplementalSampler) const;

    bool isConsistent(const SurfaceScatterEvent &event, const Vec3f &w) const;

    Vec3f generalizedShadowRay(Ray &ray,
                               const Medium *medium,
                               const Primitive *endCap,
                               int bounce);

    Vec3f attenuatedEmission(const Primitive &light,
                             const Medium *medium,
                             float expectedDist,
                             IntersectionTemporary &data,
                             IntersectionInfo &info,
                             int bounce,
                             Ray &ray);

    Vec3f lightSample(const TangentFrame &frame,
                      const Primitive &light,
                      const Bsdf &bsdf,
                      SurfaceScatterEvent &event,
                      const Medium *medium,
                      int bounce,
                      float epsilon,
                      const Ray &parentRay);

    Vec3f bsdfSample(const TangentFrame &frame,
                     const Primitive &light,
                     const Bsdf &bsdf,
                     SurfaceScatterEvent &event,
                     const Medium *medium,
                     int bounce,
                     float epsilon,
                     const Ray &parentRay);

    Vec3f volumeLightSample(VolumeScatterEvent &event,
                            const Primitive &light,
                            const Medium *medium,
                            bool performMis,
                            int bounce,
                            const Ray &parentRay);

    Vec3f volumePhaseSample(const Primitive &light,
                            VolumeScatterEvent &event,
                            const Medium *medium,
                            int bounce,
                            const Ray &parentRay);

    Vec3f sampleDirect(const TangentFrame &frame,
                       const Primitive &light,
                       const Bsdf &bsdf,
                       SurfaceScatterEvent &event,
                       const Medium *medium,
                       int bounce,
                       float epsilon,
                       const Ray &parentRay);

    Vec3f volumeSampleDirect(const Primitive &light,
                             VolumeScatterEvent &event,
                             const Medium *medium,
                             int bounce,
                             const Ray &parentRay);

    const Primitive *chooseLight(SampleGenerator &sampler, const Vec3f &p, float &weight);

    Vec3f volumeEstimateDirect(VolumeScatterEvent &event,
                               const Medium *medium,
                               int bounce,
                               const Ray &parentRay);

    Vec3f estimateDirect(const TangentFrame &frame,
                         const Bsdf &bsdf,
                         SurfaceScatterEvent &event,
                         const Medium *medium,
                         int bounce,
                         float epsilon,
                         const Ray &parentRay);

    bool handleVolume(SampleGenerator &sampler, UniformSampler &supplementalSampler,
               const Medium *&medium, int bounce, Ray &ray,
               Vec3f &throughput, Vec3f &emission, bool &wasSpecular, bool &hitSurface,
               Medium::MediumState &state);

    bool handleSurface(IntersectionTemporary &data, IntersectionInfo &info,
                       SampleGenerator &sampler, UniformSampler &supplementalSampler,
                       const Medium *&medium, int bounce, Ray &ray,
                       Vec3f &throughput, Vec3f &emission, bool &wasSpecular,
                       Medium::MediumState &state);

    void setScene(TraceableScene *scene);

public:
    PathTraceIntegrator();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual Vec3f traceSample(Vec2u pixel, SampleGenerator &sampler, UniformSampler &supplementalSampler) override;
    virtual Integrator *cloneThreadSafe(uint32 threadId, TraceableScene *scene) const override;
};

}

#endif /* PATHTRACEINTEGRATOR_HPP_ */

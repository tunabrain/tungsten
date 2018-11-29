#ifndef TRACEBASE_HPP_
#define TRACEBASE_HPP_

#include "TraceSettings.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"
#include "samplerecords/MediumSample.hpp"
#include "samplerecords/LightSample.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"
#include "sampling/Distribution1D.hpp"
#include "sampling/SampleWarp.hpp"

#include "renderer/TraceableScene.hpp"

#include "cameras/Camera.hpp"

#include "media/Medium.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "bsdfs/Bsdf.hpp"

#include <vector>
#include <memory>
#include <cmath>

namespace Tungsten {

class TraceBase
{
protected:
    const TraceableScene *_scene;
    TraceSettings _settings;
    uint32 _threadId;

    // For computing direct lighting probabilities
    std::vector<float> _lightPdf;
    // For sampling light sources in adjoint light tracing
    std::unique_ptr<Distribution1D> _lightSampler;

    TraceBase(TraceableScene *scene, const TraceSettings &settings, uint32 threadId);

    bool isConsistent(const SurfaceScatterEvent &event, const Vec3f &w) const;

    template<bool ComputePdfs>
    inline Vec3f generalizedShadowRayImpl(PathSampleGenerator &sampler,
                               Ray &ray,
                               const Medium *medium,
                               const Primitive *endCap,
                               int bounce,
                               bool startsOnSurface,
                               bool endsOnSurface,
                               float &pdfForward,
                               float &pdfBackward) const;

    Vec3f attenuatedEmission(PathSampleGenerator &sampler,
                             const Primitive &light,
                             const Medium *medium,
                             float expectedDist,
                             IntersectionTemporary &data,
                             IntersectionInfo &info,
                             int bounce,
                             bool startsOnSurface,
                             Ray &ray,
                             Vec3f *transmittance);

    bool volumeLensSample(const Camera &camera,
                    PathSampleGenerator &sampler,
                    MediumSample &mediumSample,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay,
                    Vec3f &weight,
                    Vec2f &pixel);

    bool surfaceLensSample(const Camera &camera,
                    SurfaceScatterEvent &event,
                    const Medium *medium,
                    int bounce,
                    const Ray &parentRay,
                    Vec3f &weight,
                    Vec2f &pixel);

    Vec3f lightSample(const Primitive &light,
                      SurfaceScatterEvent &event,
                      const Medium *medium,
                      int bounce,
                      const Ray &parentRay,
                      Vec3f *transmittance);

    Vec3f bsdfSample(const Primitive &light,
                     SurfaceScatterEvent &event,
                     const Medium *medium,
                     int bounce,
                     const Ray &parentRay);

    Vec3f volumeLightSample(PathSampleGenerator &sampler,
                        MediumSample &mediumSample,
                        const Primitive &light,
                        const Medium *medium,
                        int bounce,
                        const Ray &parentRay);

    Vec3f volumePhaseSample(const Primitive &light,
                        PathSampleGenerator &sampler,
                        MediumSample &mediumSample,
                        const Medium *medium,
                        int bounce,
                        const Ray &parentRay);

    Vec3f sampleDirect(const Primitive &light,
                       SurfaceScatterEvent &event,
                       const Medium *medium,
                       int bounce,
                       const Ray &parentRay,
                       Vec3f *transmittance);

    Vec3f volumeSampleDirect(const Primitive &light,
                        PathSampleGenerator &sampler,
                        MediumSample &mediumSample,
                        const Medium *medium,
                        int bounce,
                        const Ray &parentRay);

    const Primitive *chooseLight(PathSampleGenerator &sampler, const Vec3f &p, float &weight);
    const Primitive *chooseLightAdjoint(PathSampleGenerator &sampler, float &pdf);

    Vec3f volumeEstimateDirect(PathSampleGenerator &sampler,
                        MediumSample &mediumSample,
                        const Medium *medium,
                        int bounce,
                        const Ray &parentRay);

    Vec3f estimateDirect(SurfaceScatterEvent &event,
                         const Medium *medium,
                         int bounce,
                         const Ray &parentRay,
                         Vec3f *transmission);

public:
    SurfaceScatterEvent makeLocalScatterEvent(IntersectionTemporary &data, IntersectionInfo &info,
            Ray &ray, PathSampleGenerator *sampler) const;

    Vec3f generalizedShadowRay(PathSampleGenerator &sampler,
                               Ray &ray,
                               const Medium *medium,
                               const Primitive *endCap,
                               bool startsOnSurface,
                               bool endsOnSurface,
                               int bounce) const;
    Vec3f generalizedShadowRayAndPdfs(PathSampleGenerator &sampler,
                               Ray &ray,
                               const Medium *medium,
                               const Primitive *endCap,
                               int bounce,
                               bool startsOnSurface,
                               bool endsOnSurface,
                               float &pdfForward,
                               float &pdfBackward) const;

    bool handleVolume(PathSampleGenerator &sampler, MediumSample &mediumSample,
               const Medium *&medium, int bounce, bool adjoint, bool enableLightSampling,
               Ray &ray, Vec3f &throughput, Vec3f &emission, bool &wasSpecular);

    bool handleSurface(SurfaceScatterEvent &event, IntersectionTemporary &data,
               IntersectionInfo &info, const Medium *&medium,
               int bounce, bool adjoint, bool enableLightSampling, Ray &ray,
               Vec3f &throughput, Vec3f &emission, bool &wasSpecular,
               Medium::MediumState &state, Vec3f *transmittance = nullptr);

    void handleInfiniteLights(IntersectionTemporary &data,
            IntersectionInfo &info, bool enableLightSampling, Ray &ray,
            Vec3f throughput, bool wasSpecular, Vec3f &emission);
};

}

#endif /* TRACEBASE_HPP_ */

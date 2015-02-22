#ifndef PHOTONTRACER_HPP_
#define PHOTONTRACER_HPP_

#include "PhotonMapSettings.hpp"
#include "KdTree.hpp"
#include "Photon.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"
#include "samplerecords/VolumeScatterEvent.hpp"
#include "samplerecords/LightSample.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"
#include "sampling/Distribution1D.hpp"
#include "sampling/SampleWarp.hpp"

#include "renderer/TraceableScene.hpp"

#include "cameras/Camera.hpp"

#include "bsdfs/Bsdf.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include <vector>
#include <memory>
#include <cmath>

namespace Tungsten {

class PhotonTracer
{
    const TraceableScene *_scene;
    PhotonMapSettings _settings;
    uint32 _threadId;
    std::unique_ptr<Distribution1D> _lightSampler;
    std::unique_ptr<const Photon *[]> _photonQuery;
    std::unique_ptr<float[]> _distanceQuery;

    SurfaceScatterEvent makeLocalScatterEvent(IntersectionTemporary &data, IntersectionInfo &info,
            Ray &ray, SampleGenerator *sampler, UniformSampler *supplementalSampler) const;

    bool handleSurface(IntersectionTemporary &data, IntersectionInfo &info,
                       SampleGenerator &sampler, UniformSampler &supplementalSampler,
                       Ray &ray, Vec3f &throughput);

public:
    PhotonTracer(TraceableScene *scene, const PhotonMapSettings &settings, uint32 threadId);

    int tracePhoton(Photon *dst, int maxCount, SampleGenerator &sampler, UniformSampler &supplementalSampler);
    Vec3f traceSample(Vec2u pixel, const KdTree &tree, SampleGenerator &sampler, UniformSampler &supplementalSampler);
};

}



#endif /* PHOTONTRACER_HPP_ */

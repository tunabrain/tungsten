#ifndef PHOTONTRACER_HPP_
#define PHOTONTRACER_HPP_

#include "PhotonMapSettings.hpp"
#include "KdTree.hpp"
#include "Photon.hpp"

#include "integrators/TraceBase.hpp"

#include "sampling/Distribution1D.hpp"

namespace Tungsten {

class PhotonTracer : public TraceBase
{
    PhotonMapSettings _settings;
    std::unique_ptr<Distribution1D> _lightSampler;
    std::unique_ptr<const Photon *[]> _photonQuery;
    std::unique_ptr<float[]> _distanceQuery;

public:
    PhotonTracer(TraceableScene *scene, const PhotonMapSettings &settings, uint32 threadId);

    int tracePhoton(Photon *dst, int maxCount, SampleGenerator &sampler, UniformSampler &supplementalSampler);
    Vec3f traceSample(Vec2u pixel, const KdTree &tree, SampleGenerator &sampler, UniformSampler &supplementalSampler);
};

}



#endif /* PHOTONTRACER_HPP_ */

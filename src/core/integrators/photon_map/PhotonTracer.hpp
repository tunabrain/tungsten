#ifndef PHOTONTRACER_HPP_
#define PHOTONTRACER_HPP_

#include "PhotonMapSettings.hpp"
#include "PhotonRange.hpp"
#include "KdTree.hpp"
#include "Photon.hpp"

#include "integrators/TraceBase.hpp"

namespace Tungsten {

class PhotonTracer : public TraceBase
{
    PhotonMapSettings _settings;
    std::unique_ptr<const Photon *[]> _photonQuery;
    std::unique_ptr<float[]> _distanceQuery;

public:
    PhotonTracer(TraceableScene *scene, const PhotonMapSettings &settings, uint32 threadId);

    void tracePhoton(SurfacePhotonRange &surfaceRange, VolumePhotonRange &volumeRange,
            PathSampleGenerator &sampler);
    Vec3f traceSample(Vec2u pixel, const KdTree<Photon> &surfaceTree,
            const KdTree<VolumePhoton> *mediumTree, PathSampleGenerator &sampler,
            float gatherRadius);
};

}



#endif /* PHOTONTRACER_HPP_ */

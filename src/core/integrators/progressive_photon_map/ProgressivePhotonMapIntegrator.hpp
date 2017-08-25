#ifndef PROGRESSIVEPHOTONMAPINTEGRATOR_HPP_
#define PROGRESSIVEPHOTONMAPINTEGRATOR_HPP_

#include "ProgressivePhotonMapSettings.hpp"

#include "integrators/photon_map/PhotonMapIntegrator.hpp"
#include "integrators/photon_map/PhotonTracer.hpp"
#include "integrators/photon_map/KdTree.hpp"
#include "integrators/photon_map/Photon.hpp"
#include "integrators/Integrator.hpp"
#include "integrators/ImageTile.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/UniformPathSampler.hpp"
#include "sampling/UniformSampler.hpp"

#include "thread/TaskGroup.hpp"

#include "math/MathUtil.hpp"

#include <atomic>
#include <memory>
#include <vector>

namespace Tungsten {

class ProgressivePhotonMapIntegrator : public PhotonMapIntegrator
{
    ProgressivePhotonMapSettings _progressiveSettings;

    std::vector<UniformPathSampler> _shadowSamplers;

    uint32 _iteration;

    void renderSegment(std::function<void()> completionCallback);

public:
    ProgressivePhotonMapIntegrator();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void prepareForRender(TraceableScene &scene, uint32 seed) override;

    virtual void startRender(std::function<void()> completionCallback) override;
};

}

#endif /* PROGRESSIVEPHOTONMAPINTEGRATOR_HPP_ */

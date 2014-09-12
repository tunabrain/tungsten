#ifndef HETEROGENEOUSMEDIUM_HPP_
#define HETEROGENEOUSMEDIUM__HPP_

#include "VoxelVolume.hpp"
#include "Medium.hpp"

#include "sampling/UniformSampler.hpp"

namespace Tungsten {

class HeterogeneousMedium : public Medium
{
    Vec3f _sigmaA, _sigmaS;

    Vec3f _sigmaT;
    Vec3f _albedo;
    float _avgAlbedo;
    float _absorptionWeight;
    std::shared_ptr<VoxelVolumeA> _density;

    void init()
    {
        _sigmaT = _sigmaA + _sigmaS;
        _albedo = _sigmaS/_sigmaT;
        _avgAlbedo = _albedo.avg();
        _absorptionWeight = 1.0f/_avgAlbedo;
    }

public:
    HeterogeneousMedium()
    : _sigmaA(0.0f),
      _sigmaS(0.0f)
    {
        init();
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene)
    {
        Medium::fromJson(v, scene);
        JsonUtils::fromJson(v, "sigmaA", _sigmaA);
        JsonUtils::fromJson(v, "sigmaS", _sigmaS);
        std::string path;
        if (!JsonUtils::fromJson(v, "density", path))
            FAIL("Heterogeneous volume needs a density voxel grid");
        _density = VoxelUtils::loadVolume<true>(path);
        if (!_density)
            FAIL("Failed to load voxel volume at '%s'", path);

        init();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(Medium::toJson(allocator));
        v.AddMember("type", "heterogeneous", allocator);
        v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
        v.AddMember("sigmaS", JsonUtils::toJsonValue(_sigmaS, allocator), allocator);
        JsonUtils::addObjectMember(v, "density", *_density, allocator);

        return std::move(v);
    }

    bool isHomogeneous() const override
    {
        return false;
    }

    virtual void prepareForRender() override final
    {
    }

    virtual void cleanupAfterRender() override final
    {
    }

//  Vec3f avgSigmaA() const override { return _sigmaA; }
//  Vec3f avgSigmaS() const override { return _sigmaS; }
//  Vec3f minSigmaA() const override { return _sigmaA; }
//  Vec3f minSigmaS() const override { return _sigmaS; }
//  Vec3f maxSigmaA() const override { return _sigmaA; }
//  Vec3f maxSigmaS() const override { return _sigmaS; }

    bool sampleDistance(VolumeScatterEvent &event, MediumState &/*data*/) const override
    {
        int component = event.supplementalSampler->nextI() % 3;
        float sigmaTc = _sigmaT[component];

        float targetOpticalDepth = -std::log(1.0f - event.sampler->next1D())/sigmaTc;
        float opticalDepth = 0.0f;
        float t = 0.0f;
        const float dT = 0.02f;
        while (t < event.maxT) {
            float d = (*_density)[event.p + event.wi*(t + dT*0.5f)];
            opticalDepth += dT*d;
            t += dT;
            if (opticalDepth >= targetOpticalDepth) {
                t -= (opticalDepth - targetOpticalDepth)/d;
                opticalDepth = targetOpticalDepth;
                break;
            }
        }
//      _density->dda(event.p, event.wi, 0.0f, event.maxT, [&](float d, float dT) {
//          t += dT;
//          opticalDepth += d*dT;
//          if (opticalDepth >= targetOpticalDepth) {
//              t -= (opticalDepth - targetOpticalDepth)/d;
//              return true;
//          } else {
//              return false;
//          }
//      });
        event.t = min(t, event.maxT);
        //event.throughput = std::exp(-event.t*(_sigmaT - sigmaTc));
        event.throughput = std::exp(-opticalDepth*(_sigmaT - sigmaTc));

        return true;
    }

    bool absorb(VolumeScatterEvent &event, MediumState &/*data*/) const
    {
        if (event.sampler->next1D() >= _avgAlbedo)
            return true;
        event.throughput *= _absorptionWeight;
        return false;
    }

    bool scatter(VolumeScatterEvent &event) const
    {
        event.wo = PhaseFunction::sample(_phaseFunction, _phaseG, event.sampler->next2D());
        event.pdf = PhaseFunction::eval(_phaseFunction, event.wo.z(), _phaseG);
        event.throughput *= _albedo;
        TangentFrame frame(event.wi);
        event.wo = frame.toGlobal(event.wo);
        return true;
    }

    Vec3f transmittance(const VolumeScatterEvent &event) const
    {
        float opticalDepth = 0.0f;
        float t = 0.0f;
        const float dT = 0.02f;
        while (t < event.t) {
            float d = (*_density)[event.p + event.wi*(t + dT*0.5f)];
            opticalDepth += dT*d;
            t += dT;
        }
//      _density->dda(event.p, event.wi, 0.0f, event.t, [&](float d, float dT) {
//          opticalDepth += d*dT;
//          return false;
//      });

        return std::exp(-_sigmaT*opticalDepth);
    }

    Vec3f emission(const VolumeScatterEvent &/*event*/) const
    {
        return Vec3f(0.0f);
    }

    Vec3f eval(const VolumeScatterEvent &event) const
    {
        return (*_density)[event.p]*_sigmaS*PhaseFunction::eval(_phaseFunction, event.wi.dot(event.wo), _phaseG);
    }
};

}

#endif /* HETEROGENEOUSMEDIUM_HPP_ */

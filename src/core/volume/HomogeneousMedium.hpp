#ifndef HOMOGENEOUSMEDIUM_HPP_
#define HOMOGENEOUSMEDIUM_HPP_

#include "Medium.hpp"

#include <iostream>

namespace Tungsten {

class HomogeneousMedium : public Medium
{
    Vec3f _sigmaA, _sigmaS;

    Vec3f _sigmaT;
    Vec3f _albedo;
    float _maxAlbedo;
    float _absorptionWeight;
    bool _absorptionOnly;

    void init()
    {
        _sigmaT = _sigmaA + _sigmaS;
        _albedo = _sigmaS/_sigmaT;
        _maxAlbedo = _albedo.max();
        _absorptionWeight = 1.0f/_maxAlbedo;
        _absorptionOnly = _maxAlbedo == 0.0f;
    }

public:
    HomogeneousMedium()
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

        init();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(Medium::toJson(allocator));
        v.AddMember("type", "homogeneous", allocator);
        v.AddMember("sigmaA", JsonUtils::toJsonValue(_sigmaA, allocator), allocator);
        v.AddMember("sigmaS", JsonUtils::toJsonValue(_sigmaS, allocator), allocator);

        return std::move(v);
    }

    bool isHomogeneous() const override
    {
        return true;
    }

    virtual void prepareForRender() override final
    {
    }

    virtual void cleanupAfterRender() override final
    {
    }

    Vec3f sigmaA() const { return _sigmaA; }
    Vec3f sigmaS() const { return _sigmaS; }

    bool sampleDistance(VolumeScatterEvent &event, MediumState &state) const override
    {
        if (state.bounce > _maxBounce)
            return false;

        if (_absorptionOnly) {
            event.t = event.maxT;
            event.throughput = std::exp(-_sigmaT*event.t);
        } else {
            int component = event.supplementalSampler->nextI() % 3;
            float sigmaTc = _sigmaT[component];

            float t = -std::log(1.0f - event.sampler->next1D())/sigmaTc;
            event.t = min(t, event.maxT);
            event.throughput = std::exp(-event.t*_sigmaT);
            if (t < event.maxT)
                event.throughput /= (_sigmaT*event.throughput).avg();
            else
                event.throughput /= event.throughput.avg();

            state.advance();
        }

        return true;
    }

    bool absorb(VolumeScatterEvent &event, MediumState &/*state*/) const
    {
        if (event.sampler->next1D() >= _maxAlbedo)
            return true;
        event.throughput = Vec3f(_absorptionWeight);
        return false;
    }

    bool scatter(VolumeScatterEvent &event) const
    {
        event.wo = PhaseFunction::sample(_phaseFunction, _phaseG, event.sampler->next2D());
        event.pdf = PhaseFunction::eval(_phaseFunction, event.wo.z(), _phaseG);
        event.throughput *= _sigmaS;
        TangentFrame frame(event.wi);
        event.wo = frame.toGlobal(event.wo);
        return true;
    }

    Vec3f transmittance(const VolumeScatterEvent &event) const
    {
        return std::exp(-_sigmaT*event.t);
    }

    Vec3f emission(const VolumeScatterEvent &/*event*/) const
    {
        return Vec3f(0.0f);
    }

    Vec3f phaseEval(const VolumeScatterEvent &event) const
    {
        return _sigmaS*PhaseFunction::eval(_phaseFunction, event.wi.dot(event.wo), _phaseG);
    }
};

}



#endif /* HOMOGENEOUSMEDIUM_HPP_ */

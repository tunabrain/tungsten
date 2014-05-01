#ifndef HOMOGENEOUSMEDIUM_HPP_
#define HOMOGENEOUSMEDIUM_HPP_

#include "Medium.hpp"

namespace Tungsten
{

class HomogeneousMedium : public Medium
{
    Vec3f _sigmaA, _sigmaS;

    Vec3f _sigmaT;
    Vec3f _albedo;
    float _avgAlbedo;
    float _absorptionWeight;

    void init()
    {
        _sigmaT = _sigmaA + _sigmaS;
        _albedo = _sigmaS/_sigmaT;
        _avgAlbedo = _albedo.avg();
        _absorptionWeight = 1.0f/_avgAlbedo;
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

    Vec3f avgSigmaA() const override { return _sigmaA; }
    Vec3f avgSigmaS() const override { return _sigmaS; }
    Vec3f minSigmaA() const override { return _sigmaA; }
    Vec3f minSigmaS() const override { return _sigmaS; }
    Vec3f maxSigmaA() const override { return _sigmaA; }
    Vec3f maxSigmaS() const override { return _sigmaS; }

    bool sampleDistance(VolumeScatterEvent &event) const override
    {
        int component = event.supplementalSampler->nextI() % 3;
        float sigmaTc = _sigmaT[component];

        float t = -std::log(1.0f - event.sampler->next1D())/sigmaTc;
        t = min(t, event.maxT);
        event.t = t;
        event.throughput = std::exp(-event.t*(_sigmaT - sigmaTc));

        return true;
    }

    bool absorb(VolumeScatterEvent &event) const
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
        return std::exp(-_sigmaT*event.t);
    }

    Vec3f emission(const VolumeScatterEvent &/*event*/) const
    {
        return Vec3f(0.0f);
    }

    Vec3f eval(const VolumeScatterEvent &event) const
    {
        return _albedo*PhaseFunction::eval(_phaseFunction, event.wi.dot(event.wo), _phaseG);
    }
};

}



#endif /* HOMOGENEOUSMEDIUM_HPP_ */

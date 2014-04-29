#ifndef MEDIUM_HPP_
#define MEDIUM_HPP_

#include "PhaseFunction.hpp"

namespace Tungsten {

class SampleGenerator;
class UniformSampler;

class VolumeScatterEvent
{
    SampleGenerator &sampler;
    UniformSampler &supplementalSampler;
    Vec3f p;
    Vec3f wi;
    float maxT;

    Vec3f wo;
    float t;
    Vec3f throughput;
};

class Medium : public JsonSerializable
{
    std::string _phaseFunctionName;
    float _phaseG;
    Vec3f _sigmaA, _sigmaS;

    PhaseFunction::Type _phaseFunction;
    bool _absorptionOnly;

public:
    bool isHomogeneous() const
    {
        return true;
    }

    const Vec3f &sigmaA() const
    {
        return _sigmaA;
    }

    const Vec3f &sigmaS() const
    {
        return _sigmaS;
    }

    bool sampleDistance(VolumeScatterEvent &event) const
    {
        int component = event.supplementalSampler.nextI() % 3;
        Vec3f sigmaT = sigmaA + sigmaS;
        float sigmaAc = _sigmaA[component];
        float sigmaSc = _sigmaS[component];
        float sigmaTc = sigmaA + sigmaS;

        float t = -std::log(1.0f - event.sampler.next1D())/sigmaTc;
        if (t > event.maxT) {
            event.t = event.maxT;
            event.throughput = sigmaTc*std::exp(-event.maxT*(sigmaT - sigmaTc));
        } else {
            event.t = t;
            event.throughput = std::exp(-event.t*(sigmaT - sigmaTc));
        }

        return true;
    }

    bool scatter(VolumeScatterEvent &event) const
    {
        TangentFrame frame(event.wi);
        event.wo = frame.toGlobal(PhaseFunction::sample(_phaseFunction, _phaseG, event.sampler.next2D()));
        return true;
    }

    Vec3f transmittance(const VolumeScatterEvent &event) const
    {
        return std::exp(-(_sigmaA + _sigmaS)*event.t);
    }
};

}



#endif /* MEDIUM_HPP_ */

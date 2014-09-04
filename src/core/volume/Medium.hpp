#ifndef MEDIUM_HPP_
#define MEDIUM_HPP_

#include "samplerecords/VolumeScatterEvent.hpp"

#include "io/JsonSerializable.hpp"

#include "PhaseFunction.hpp"

namespace Tungsten {

class Scene;

class Medium : public JsonSerializable
{
protected:
    std::string _phaseFunctionName;
    float _phaseG;
    int _maxBounce;

    PhaseFunction::Type _phaseFunction;

    void init()
    {
        _phaseFunction = PhaseFunction::stringToType(_phaseFunctionName);
    }

public:
    struct MediumState
    {
        bool firstScatter;
        int component;
        int bounce;

        void reset()
        {
            firstScatter = true;
            bounce = 0;
        }

        void advance()
        {
            firstScatter = false;
            bounce++;
        }
    };

    Medium();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isHomogeneous() const = 0;

    virtual void prepareForRender() = 0;
    virtual void cleanupAfterRender() = 0;

    virtual bool sampleDistance(VolumeScatterEvent &event, MediumState &state) const = 0;
    virtual bool absorb(VolumeScatterEvent &event, MediumState &state) const = 0;
    virtual bool scatter(VolumeScatterEvent &event) const = 0;
    virtual Vec3f transmittance(const VolumeScatterEvent &event) const = 0;
    virtual Vec3f emission(const VolumeScatterEvent &event) const = 0;

    virtual Vec3f phaseEval(const VolumeScatterEvent &event) const = 0;
    float phasePdf(const VolumeScatterEvent &event) const
    {
        return PhaseFunction::eval(_phaseFunction, event.wi.dot(event.wo), _phaseG);
    }

    bool suggestMis() const
    {
        if (_phaseFunction == PhaseFunction::Isotropic)
            return false;
        if (_phaseFunction == PhaseFunction::HenyeyGreenstein && std::abs(_phaseG) < 0.1f)
            return false;
        return true;
    }

    PhaseFunction::Type phaseFunctionType() const
    {
        return _phaseFunction;
    }

    float phaseG() const
    {
        return _phaseG;
    }
};

}



#endif /* MEDIUM_HPP_ */

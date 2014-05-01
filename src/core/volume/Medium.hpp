#ifndef MEDIUM_HPP_
#define MEDIUM_HPP_

#include "sampling/VolumeScatterEvent.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include "PhaseFunction.hpp"

namespace Tungsten {

class Scene;

class Medium : public JsonSerializable
{
protected:
    std::string _phaseFunctionName;
    float _phaseG;

    PhaseFunction::Type _phaseFunction;

    void init()
    {
        _phaseFunction = PhaseFunction::stringToType(_phaseFunctionName);
    }

public:
    Medium()
    : _phaseFunctionName("isotropic"),
      _phaseG(0.0f)
    {
        init();
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene)
    {
        JsonSerializable::fromJson(v, scene);
        JsonUtils::fromJson(v, "phase_function", _phaseFunctionName);
        JsonUtils::fromJson(v, "phase_g", _phaseG);
        init();
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(JsonSerializable::toJson(allocator));
        v.AddMember("phase_function", _phaseFunctionName.c_str(), allocator);
        v.AddMember("phase_g", _phaseG, allocator);

        return std::move(v);
    }

    virtual bool isHomogeneous() const = 0;

    virtual Vec3f avgSigmaA() const = 0;
    virtual Vec3f avgSigmaS() const = 0;
    virtual Vec3f minSigmaA() const = 0;
    virtual Vec3f minSigmaS() const = 0;
    virtual Vec3f maxSigmaA() const = 0;
    virtual Vec3f maxSigmaS() const = 0;

    virtual bool sampleDistance(VolumeScatterEvent &event) const = 0;
    virtual bool absorb(VolumeScatterEvent &event) const = 0;
    virtual bool scatter(VolumeScatterEvent &event) const = 0;
    virtual Vec3f transmittance(const VolumeScatterEvent &event) const = 0;
    virtual Vec3f emission(const VolumeScatterEvent &event) const = 0;

    virtual Vec3f eval(const VolumeScatterEvent &event) const = 0;
    float pdf(const VolumeScatterEvent &event) const
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

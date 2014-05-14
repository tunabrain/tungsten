#ifndef RENDERERSETTINGS_HPP_
#define RENDERERSETTINGS_HPP_

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class RendererSettings : public JsonSerializable
{
    bool _useAdaptiveSampling;
    bool _useSobol;

public:
    RendererSettings()
    : _useAdaptiveSampling(true),
      _useSobol(false)
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
    {
        JsonUtils::fromJson(v, "adaptive_sampling", _useAdaptiveSampling);
        JsonUtils::fromJson(v, "stratified_sampler", _useSobol);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(JsonSerializable::toJson(allocator));
        v.AddMember("adaptive_sampling", _useAdaptiveSampling, allocator);
        v.AddMember("stratified_sampler", _useSobol, allocator);
        return std::move(v);
    }

    bool useAdaptiveSampling() const
    {
        return _useAdaptiveSampling;
    }

    bool useSobol() const
    {
        return _useSobol;
    }
};

}

#endif /* RENDERERSETTINGS_HPP_ */

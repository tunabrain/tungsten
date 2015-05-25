#ifndef PATHTRACERSETTINGS_HPP_
#define PATHTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct PathTracerSettings : public TraceSettings
{
    bool enableLightSampling;
    bool enableVolumeLightSampling;

    PathTracerSettings()
    : enableLightSampling(true),
      enableVolumeLightSampling(true)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
        JsonUtils::fromJson(v, "enable_light_sampling", enableLightSampling);
        JsonUtils::fromJson(v, "enable_volume_light_sampling", enableVolumeLightSampling);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = TraceSettings::toJson(allocator);
        v.AddMember("type", "path_tracer", allocator);
        v.AddMember("enable_light_sampling", enableLightSampling, allocator);
        v.AddMember("enable_volume_light_sampling", enableVolumeLightSampling, allocator);
        return std::move(v);
    }
};

}

#endif /* PATHTRACERSETTINGS_HPP_ */

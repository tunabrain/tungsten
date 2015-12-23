#ifndef PATHTRACERSETTINGS_HPP_
#define PATHTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

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
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "path_tracer",
            "enable_light_sampling", enableLightSampling,
            "enable_volume_light_sampling", enableVolumeLightSampling
        };
    }
};

}

#endif /* PATHTRACERSETTINGS_HPP_ */

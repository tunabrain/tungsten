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

    void fromJson(JsonValue value)
    {
        TraceSettings::fromJson(value);
        value.getField("enable_light_sampling", enableLightSampling);
        value.getField("enable_volume_light_sampling", enableVolumeLightSampling);
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

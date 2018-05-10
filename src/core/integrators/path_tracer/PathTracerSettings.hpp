#ifndef PATHTRACERSETTINGS_HPP_
#define PATHTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct PathTracerSettings : public TraceSettings
{
    bool enableLightSampling;
    bool enableVolumeLightSampling;
    bool lowOrderScattering;
    bool includeSurfaces;

    PathTracerSettings()
    : enableLightSampling(true),
      enableVolumeLightSampling(true),
      lowOrderScattering(true),
      includeSurfaces(true)
    {
    }

    void fromJson(JsonPtr value)
    {
        TraceSettings::fromJson(value);
        value.getField("enable_light_sampling", enableLightSampling);
        value.getField("enable_volume_light_sampling", enableVolumeLightSampling);
        value.getField("low_order_scattering", lowOrderScattering);
        value.getField("include_surfaces", includeSurfaces);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "path_tracer",
            "enable_light_sampling", enableLightSampling,
            "enable_volume_light_sampling", enableVolumeLightSampling,
            "low_order_scattering", lowOrderScattering,
            "include_surfaces", includeSurfaces
        };
    }
};

}

#endif /* PATHTRACERSETTINGS_HPP_ */

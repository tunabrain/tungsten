#ifndef LIGHTTRACERSETTINGS_HPP_
#define LIGHTTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct LightTracerSettings : public TraceSettings
{
    bool lowOrderScattering;
    bool includeSurfaces;

    LightTracerSettings()
    : lowOrderScattering(true),
      includeSurfaces(true)
    {
    }

    void fromJson(JsonPtr value)
    {
        TraceSettings::fromJson(value);
        value.getField("low_order_scattering", lowOrderScattering);
        value.getField("include_surfaces", includeSurfaces);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "light_tracer",
            "low_order_scattering", lowOrderScattering,
            "include_surfaces", includeSurfaces
        };
    }
};

}

#endif /* LIGHTTRACERSETTINGS_HPP_ */

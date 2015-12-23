#ifndef LIGHTTRACERSETTINGS_HPP_
#define LIGHTTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct LightTracerSettings : public TraceSettings
{
    LightTracerSettings()
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "light_tracer"
        };
    }
};

}

#endif /* LIGHTTRACERSETTINGS_HPP_ */

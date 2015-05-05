#ifndef LIGHTTRACERSETTINGS_HPP_
#define LIGHTTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonUtils.hpp"

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
        rapidjson::Value v = TraceSettings::toJson(allocator);
        v.AddMember("type", "light_tracer", allocator);
        return std::move(v);
    }
};

}

#endif /* LIGHTTRACERSETTINGS_HPP_ */

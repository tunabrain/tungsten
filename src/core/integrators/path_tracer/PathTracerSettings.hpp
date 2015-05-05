#ifndef PATHTRACERSETTINGS_HPP_
#define PATHTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct PathTracerSettings : public TraceSettings
{
    PathTracerSettings()
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = TraceSettings::toJson(allocator);
        v.AddMember("type", "path_tracer", allocator);
        return std::move(v);
    }
};

}

#endif /* PATHTRACERSETTINGS_HPP_ */

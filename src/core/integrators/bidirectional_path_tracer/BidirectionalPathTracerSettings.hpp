#ifndef BIDIRECTIONALPATHTRACERSETTINGS_HPP_
#define BIDIRECTIONALPATHTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct BidirectionalPathTracerSettings : public TraceSettings
{
    BidirectionalPathTracerSettings()
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = TraceSettings::toJson(allocator);
        v.AddMember("type", "bidirectional_path_tracer", allocator);
        return std::move(v);
    }
};

}

#endif /* BIDIRECTIONALPATHTRACERSETTINGS_HPP_ */

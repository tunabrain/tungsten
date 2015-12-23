#ifndef BIDIRECTIONALPATHTRACERSETTINGS_HPP_
#define BIDIRECTIONALPATHTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

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
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "bidirectional_path_tracer"
        };
    }
};

}

#endif /* BIDIRECTIONALPATHTRACERSETTINGS_HPP_ */

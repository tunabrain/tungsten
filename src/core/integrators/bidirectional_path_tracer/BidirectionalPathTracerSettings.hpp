#ifndef BIDIRECTIONALPATHTRACERSETTINGS_HPP_
#define BIDIRECTIONALPATHTRACERSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct BidirectionalPathTracerSettings : public TraceSettings
{
    bool imagePyramid;

    BidirectionalPathTracerSettings()
    : imagePyramid(false)
    {
    }

    void fromJson(JsonPtr value)
    {
        TraceSettings::fromJson(value);
        value.getField("image_pyramid", imagePyramid);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "bidirectional_path_tracer",
            "image_pyramid", imagePyramid
        };
    }
};

}

#endif /* BIDIRECTIONALPATHTRACERSETTINGS_HPP_ */

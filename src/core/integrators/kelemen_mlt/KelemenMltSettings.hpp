#ifndef KELEMENMLTSETTINGS_HPP_
#define KELEMENMLTSETTINGS_HPP_

#include "integrators/path_tracer/PathTracerSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct KelemenMltSettings : public PathTracerSettings
{
    int initialSamplePool;
    bool bidirectional;
    float largeStepProbability;
    bool imagePyramid;

    KelemenMltSettings()
    : initialSamplePool(10000),
      bidirectional(true),
      largeStepProbability(0.1f),
      imagePyramid(false)
    {
    }

    void fromJson(JsonPtr value)
    {
        PathTracerSettings::fromJson(value);
        value.getField("initial_sample_pool", initialSamplePool);
        value.getField("bidirectional", bidirectional);
        value.getField("large_step_probability", largeStepProbability);
        value.getField("image_pyramid", imagePyramid);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = PathTracerSettings::toJson(allocator);
        v.RemoveMember("type");

        return JsonObject{std::move(v), allocator,
            "type", "kelemen_mlt",
            "initial_sample_pool", initialSamplePool,
            "bidirectional", bidirectional,
            "large_step_probability", largeStepProbability,
            "image_pyramid", imagePyramid,
        };
    }
};

}

#endif /* KELEMENMLTSETTINGS_HPP_ */

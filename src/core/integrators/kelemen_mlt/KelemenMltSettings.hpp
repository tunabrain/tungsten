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

    KelemenMltSettings()
    : initialSamplePool(10000),
      bidirectional(true),
      largeStepProbability(0.1f)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        PathTracerSettings::fromJson(v);
        JsonUtils::fromJson(v, "initial_sample_pool", initialSamplePool);
        JsonUtils::fromJson(v, "bidirectional", bidirectional);
        JsonUtils::fromJson(v, "large_step_probability", largeStepProbability);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = PathTracerSettings::toJson(allocator);
        v.RemoveMember("type");

        return JsonObject{std::move(v), allocator,
            "type", "kelemen_mlt",
            "initial_sample_pool", initialSamplePool,
            "bidirectional", bidirectional,
            "large_step_probability", largeStepProbability
        };
    }
};

}

#endif /* KELEMENMLTSETTINGS_HPP_ */

#ifndef KELEMENMLTSETTINGS_HPP_
#define KELEMENMLTSETTINGS_HPP_

#include "integrators/path_tracer/PathTracerSettings.hpp"

#include "io/JsonUtils.hpp"

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
        v.AddMember("type", "kelemen_mlt", allocator);
        v.AddMember("initial_sample_pool", initialSamplePool, allocator);
        v.AddMember("bidirectional", bidirectional, allocator);
        v.AddMember("large_step_probability", largeStepProbability, allocator);
        return std::move(v);
    }
};

}

#endif /* KELEMENMLTSETTINGS_HPP_ */

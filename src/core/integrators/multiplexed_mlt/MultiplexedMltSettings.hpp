#ifndef MULTIPLEXEDMLTSETTINGS_HPP_
#define MULTIPLEXEDMLTSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct MultiplexedMltSettings : public TraceSettings
{
    int initialSamplePool;
    bool imagePyramid;
    float largeStepProbability;

    MultiplexedMltSettings()
    : initialSamplePool(3000000),
      imagePyramid(false),
      largeStepProbability(0.1f)
    {
    }

    void fromJson(JsonPtr v)
    {
        TraceSettings::fromJson(v);
        v.getField("initial_sample_pool", initialSamplePool);
        v.getField("image_pyramid", imagePyramid);
        v.getField("large_step_probability", largeStepProbability);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = TraceSettings::toJson(allocator);
        v.AddMember("type", "multiplexed_mlt", allocator);
        v.AddMember("initial_sample_pool", initialSamplePool, allocator);
        v.AddMember("image_pyramid", imagePyramid, allocator);
        v.AddMember("large_step_probability", largeStepProbability, allocator);
        return std::move(v);
    }
};

}

#endif /* MULTIPLEXEDMLTSETTINGS_HPP_ */

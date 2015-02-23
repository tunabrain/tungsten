#ifndef TRACESETTINGS_HPP_
#define TRACESETTINGS_HPP_

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct TraceSettings
{
    bool enableLightSampling;
    bool enableVolumeLightSampling;
    bool enableConsistencyChecks;
    bool enableTwoSidedShading;
    int minBounces;
    int maxBounces;

    TraceSettings()
    : enableLightSampling(true),
      enableVolumeLightSampling(true),
      enableConsistencyChecks(false),
      enableTwoSidedShading(true),
      minBounces(0),
      maxBounces(64)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        JsonUtils::fromJson(v, "min_bounces", minBounces);
        JsonUtils::fromJson(v, "max_bounces", maxBounces);
        JsonUtils::fromJson(v, "enable_light_sampling", enableLightSampling);
        JsonUtils::fromJson(v, "enable_volume_light_sampling", enableVolumeLightSampling);
        JsonUtils::fromJson(v, "enable_consistency_checks", enableConsistencyChecks);
        JsonUtils::fromJson(v, "enable_two_sided_shading", enableTwoSidedShading);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v(rapidjson::kObjectType);
        v.AddMember("type", "path_trace", allocator);
        v.AddMember("min_bounces", minBounces, allocator);
        v.AddMember("max_bounces", maxBounces, allocator);
        v.AddMember("enable_light_sampling", enableLightSampling, allocator);
        v.AddMember("enable_volume_light_sampling", enableVolumeLightSampling, allocator);
        v.AddMember("enable_consistency_checks", enableConsistencyChecks, allocator);
        v.AddMember("enable_two_sided_shading", enableTwoSidedShading, allocator);
        return std::move(v);
    }
};

}

#endif /* TRACESETTINGS_HPP_ */

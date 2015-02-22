#ifndef PHOTONMAPSETTINGS_HPP_
#define PHOTONMAPSETTINGS_HPP_

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct PhotonMapSettings
{
    bool enableConsistencyChecks;
    bool enableTwoSidedShading;
    int minBounces;
    int maxBounces;
    uint32 photonCount;
    uint32 gatherCount;
    float gatherRadius;
    uint32 photonsPerIteration;

    PhotonMapSettings()
    : enableConsistencyChecks(false),
      enableTwoSidedShading(true),
      minBounces(0),
      maxBounces(64),
      photonCount(1000000),
      gatherCount(20),
      gatherRadius(1e30f),
      photonsPerIteration(100000)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        JsonUtils::fromJson(v, "min_bounces", minBounces);
        JsonUtils::fromJson(v, "max_bounces", maxBounces);
        JsonUtils::fromJson(v, "photon_count", photonCount);
        JsonUtils::fromJson(v, "gather_photon_count", gatherCount);
        JsonUtils::fromJson(v, "gather_radius", gatherRadius);
        JsonUtils::fromJson(v, "photons_per_iteration", photonsPerIteration);
        JsonUtils::fromJson(v, "enable_consistency_checks", enableConsistencyChecks);
        JsonUtils::fromJson(v, "enable_two_sided_shading", enableTwoSidedShading);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v(rapidjson::kObjectType);
        v.AddMember("type", "photon_map", allocator);
        v.AddMember("min_bounces", minBounces, allocator);
        v.AddMember("max_bounces", maxBounces, allocator);
        v.AddMember("photon_count", photonCount, allocator);
        v.AddMember("gather_photon_count", gatherCount, allocator);
        v.AddMember("gather_radius", gatherRadius, allocator);
        v.AddMember("photons_per_iteration", photonsPerIteration, allocator);
        v.AddMember("enable_consistency_checks", enableConsistencyChecks, allocator);
        v.AddMember("enable_two_sided_shading", enableTwoSidedShading, allocator);
        return std::move(v);
    }
};

}



#endif /* PHOTONMAPSETTINGS_HPP_ */

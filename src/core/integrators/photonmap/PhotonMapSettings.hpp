#ifndef PHOTONMAPSETTINGS_HPP_
#define PHOTONMAPSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct PhotonMapSettings : public TraceSettings
{
    uint32 photonCount;
    uint32 gatherCount;
    float gatherRadius;
    uint32 photonsPerIteration;

    PhotonMapSettings()
    : photonCount(1000000),
      gatherCount(20),
      gatherRadius(1e30f),
      photonsPerIteration(100000)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
        JsonUtils::fromJson(v, "photon_count", photonCount);
        JsonUtils::fromJson(v, "gather_photon_count", gatherCount);
        JsonUtils::fromJson(v, "gather_radius", gatherRadius);
        JsonUtils::fromJson(v, "photons_per_iteration", photonsPerIteration);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = TraceSettings::toJson(allocator);
        v.AddMember("type", "photon_map", allocator);
        v.AddMember("photon_count", photonCount, allocator);
        v.AddMember("gather_photon_count", gatherCount, allocator);
        v.AddMember("gather_radius", gatherRadius, allocator);
        v.AddMember("photons_per_iteration", photonsPerIteration, allocator);
        return std::move(v);
    }
};

}

#endif /* PHOTONMAPSETTINGS_HPP_ */

#ifndef PHOTONMAPSETTINGS_HPP_
#define PHOTONMAPSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

struct PhotonMapSettings : public TraceSettings
{
    uint32 photonCount;
    uint32 volumePhotonCount;
    uint32 gatherCount;
    float gatherRadius;

    PhotonMapSettings()
    : photonCount(1000000),
      volumePhotonCount(100000),
      gatherCount(20),
      gatherRadius(1e30f)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
        JsonUtils::fromJson(v, "photon_count", photonCount);
        JsonUtils::fromJson(v, "volume_photon_count", volumePhotonCount);
        JsonUtils::fromJson(v, "gather_photon_count", gatherCount);
        JsonUtils::fromJson(v, "gather_radius", gatherRadius);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = TraceSettings::toJson(allocator);
        v.AddMember("type", "photon_map", allocator);
        v.AddMember("photon_count", photonCount, allocator);
        v.AddMember("volume_photon_count", volumePhotonCount, allocator);
        v.AddMember("gather_photon_count", gatherCount, allocator);
        v.AddMember("gather_radius", gatherRadius, allocator);
        return std::move(v);
    }
};

}

#endif /* PHOTONMAPSETTINGS_HPP_ */

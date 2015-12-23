#ifndef PHOTONMAPSETTINGS_HPP_
#define PHOTONMAPSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct PhotonMapSettings : public TraceSettings
{
    uint32 photonCount;
    uint32 volumePhotonCount;
    uint32 gatherCount;
    float gatherRadius;

    PhotonMapSettings()
    : photonCount(1000000),
      volumePhotonCount(1000000),
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
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "photon_map",
            "photon_count", photonCount,
            "volume_photon_count", volumePhotonCount,
            "gather_photon_count", gatherCount,
            "gather_radius", gatherRadius
        };
    }
};

}

#endif /* PHOTONMAPSETTINGS_HPP_ */

#ifndef PHOTONMAPSETTINGS_HPP_
#define PHOTONMAPSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

#include "Debug.hpp"

namespace Tungsten {

struct PhotonMapSettings : public TraceSettings
{
    enum VolumePhotonType
    {
        VOLUME_POINTS,
        VOLUME_BEAMS,
    };

    uint32 photonCount;
    uint32 volumePhotonCount;
    uint32 gatherCount;
    float gatherRadius;
    VolumePhotonType volumePhotonType;
    std::string volumePhotonTypeString;

    PhotonMapSettings()
    : photonCount(1000000),
      volumePhotonCount(1000000),
      gatherCount(20),
      gatherRadius(1e30f),
      volumePhotonType(VOLUME_POINTS),
      volumePhotonTypeString("points")
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
        JsonUtils::fromJson(v, "photon_count", photonCount);
        JsonUtils::fromJson(v, "volume_photon_count", volumePhotonCount);
        JsonUtils::fromJson(v, "gather_photon_count", gatherCount);
        JsonUtils::fromJson(v, "gather_radius", gatherRadius);
        JsonUtils::fromJson(v, "volume_photon_type", volumePhotonTypeString);
        if (volumePhotonTypeString == "points") {
            volumePhotonType = VOLUME_POINTS;
        } else if (volumePhotonTypeString == "beams") {
            volumePhotonType = VOLUME_BEAMS;
        } else {
            DBG("Unknown volume photon type '%s'", volumePhotonTypeString);
            volumePhotonType = VOLUME_POINTS;
        }
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "photon_map",
            "photon_count", photonCount,
            "volume_photon_count", volumePhotonCount,
            "gather_photon_count", gatherCount,
            "gather_radius", gatherRadius,
            "volume_photon_type", volumePhotonTypeString
        };
    }
};

}

#endif /* PHOTONMAPSETTINGS_HPP_ */

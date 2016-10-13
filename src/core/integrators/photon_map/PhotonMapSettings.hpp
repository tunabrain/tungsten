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
    float volumeGatherRadius;
    VolumePhotonType volumePhotonType;
    std::string volumePhotonTypeString;
    bool fixedVolumeRadius;

    PhotonMapSettings()
    : photonCount(1000000),
      volumePhotonCount(1000000),
      gatherCount(20),
      gatherRadius(1e30f),
      volumeGatherRadius(gatherRadius),
      volumePhotonType(VOLUME_POINTS),
      volumePhotonTypeString("points"),
      fixedVolumeRadius(false)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        TraceSettings::fromJson(v);
        JsonUtils::fromJson(v, "photon_count", photonCount);
        JsonUtils::fromJson(v, "volume_photon_count", volumePhotonCount);
        JsonUtils::fromJson(v, "gather_photon_count", gatherCount);
        JsonUtils::fromJson(v, "volume_photon_type", volumePhotonTypeString);
        if (volumePhotonTypeString == "points") {
            volumePhotonType = VOLUME_POINTS;
        } else if (volumePhotonTypeString == "beams") {
            volumePhotonType = VOLUME_BEAMS;
            // Set default value to something more sensible for photon beams
            volumeGatherRadius = 0.01f;
        } else {
            DBG("Unknown volume photon type '%s'", volumePhotonTypeString);
            volumePhotonType = VOLUME_POINTS;
        }
        bool gatherRadiusSet = JsonUtils::fromJson(v, "gather_radius", gatherRadius);
        if (!JsonUtils::fromJson(v, "volume_gather_radius", volumeGatherRadius) && gatherRadiusSet)
            volumeGatherRadius = gatherRadius;
        JsonUtils::fromJson(v, "fixed_volume_radius", fixedVolumeRadius);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        return JsonObject{TraceSettings::toJson(allocator), allocator,
            "type", "photon_map",
            "photon_count", photonCount,
            "volume_photon_count", volumePhotonCount,
            "gather_photon_count", gatherCount,
            "gather_radius", gatherRadius,
            "volume_gather_radius", volumeGatherRadius,
            "volume_photon_type", volumePhotonTypeString,
            "fixed_volume_radius", fixedVolumeRadius
        };
    }
};

}

#endif /* PHOTONMAPSETTINGS_HPP_ */

#ifndef PHOTONMAPSETTINGS_HPP_
#define PHOTONMAPSETTINGS_HPP_

#include "integrators/TraceSettings.hpp"

#include "io/JsonObject.hpp"

#include "StringableEnum.hpp"

#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

struct PhotonMapSettings : public TraceSettings
{
    enum VolumePhotonEnum
    {
        VOLUME_POINTS,
        VOLUME_BEAMS,
        VOLUME_PLANES,
        VOLUME_PLANES_1D,
    };

    typedef StringableEnum<VolumePhotonEnum> VolumePhotonType;
    friend VolumePhotonType;

    uint32 photonCount;
    uint32 volumePhotonCount;
    uint32 gatherCount;
    float gatherRadius;
    float volumeGatherRadius;
    VolumePhotonType volumePhotonType;
    bool includeSurfaces;
    bool lowOrderScattering;
    bool fixedVolumeRadius;
    bool useGrid;
    bool useFrustumGrid;
    int gridMemBudgetKb;

    PhotonMapSettings()
    : photonCount(1000000),
      volumePhotonCount(1000000),
      gatherCount(20),
      gatherRadius(1e30f),
      volumeGatherRadius(gatherRadius),
      volumePhotonType("points"),
      includeSurfaces(true),
      lowOrderScattering(true),
      fixedVolumeRadius(false),
      useGrid(false),
      useFrustumGrid(false),
      gridMemBudgetKb(32*1024)
    {
    }

    void fromJson(JsonPtr value)
    {
        TraceSettings::fromJson(value);
        value.getField("photon_count", photonCount);
        value.getField("volume_photon_count", volumePhotonCount);
        value.getField("gather_photon_count", gatherCount);
        if (auto type = value["volume_photon_type"])
            volumePhotonType = type;
        bool gatherRadiusSet = value.getField("gather_radius", gatherRadius);
        if (!value.getField("volume_gather_radius", volumeGatherRadius) && gatherRadiusSet)
            volumeGatherRadius = gatherRadius;
        value.getField("low_order_scattering", lowOrderScattering);
        value.getField("include_surfaces", includeSurfaces);
        value.getField("fixed_volume_radius", fixedVolumeRadius);
        value.getField("use_grid", useGrid);
        value.getField("use_frustum_grid", useFrustumGrid);
        value.getField("grid_memory", gridMemBudgetKb);

        if (useFrustumGrid && volumePhotonType == VOLUME_POINTS)
            value.parseError("Photon points cannot be used with a frustum aligned grid");
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
            "volume_photon_type", volumePhotonType.toString(),
            "low_order_scattering", lowOrderScattering,
            "include_surfaces", includeSurfaces,
            "fixed_volume_radius", fixedVolumeRadius,
            "use_grid", useGrid,
            "use_frustum_grid", useFrustumGrid,
            "grid_memory", gridMemBudgetKb
        };
    }
};

}

#endif /* PHOTONMAPSETTINGS_HPP_ */

#ifndef PROGRESSIVEPHOTONMAPSETTINGS_HPP_
#define PROGRESSIVEPHOTONMAPSETTINGS_HPP_

#include "integrators/photon_map/PhotonMapSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct ProgressivePhotonMapSettings
{
    float alpha;

    ProgressivePhotonMapSettings()
    : alpha(0.3f)
    {
    }

    void fromJson(JsonPtr value)
    {
        value.getField("alpha", alpha);
    }

    rapidjson::Value toJson(const PhotonMapSettings &settings, rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = settings.toJson(allocator);
        v.RemoveMember("type");

        return JsonObject{std::move(v), allocator,
            "type", "progressive_photon_map",
            "alpha", alpha
        };
    }
};

}

#endif /* PROGRESSIVEPHOTONMAPSETTINGS_HPP_ */

#ifndef PROGRESSIVEPHOTONMAPSETTINGS_HPP_
#define PROGRESSIVEPHOTONMAPSETTINGS_HPP_

#include "integrators/photon_map/PhotonMapSettings.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

struct ProgressivePhotonMapSettings : public PhotonMapSettings
{
    float alpha;

    ProgressivePhotonMapSettings()
    : alpha(0.3f)
    {
    }

    void fromJson(const rapidjson::Value &v)
    {
        PhotonMapSettings::fromJson(v);
        JsonUtils::fromJson(v, "alpha", alpha);
    }

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value v = PhotonMapSettings::toJson(allocator);
        v.RemoveMember("type");

        return JsonObject{std::move(v), allocator,
            "type", "progressive_photon_map",
            "alpha", alpha
        };
    }
};

}

#endif /* PROGRESSIVEPHOTONMAPSETTINGS_HPP_ */

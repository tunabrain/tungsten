#include "PhotonMapSettings.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(PhotonMapSettings::VolumePhotonType, "volume_photon_type", ({
    {"points", PhotonMapSettings::VOLUME_POINTS},
    {"beams", PhotonMapSettings::VOLUME_BEAMS},
    {"planes", PhotonMapSettings::VOLUME_PLANES},
    {"planes_1d", PhotonMapSettings::VOLUME_PLANES_1D},
}))

}

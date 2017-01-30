#include "CameraFactory.hpp"

#include "EquirectangularCamera.hpp"
#include "ThinlensCamera.hpp"
#include "CubemapCamera.hpp"
#include "PinholeCamera.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(CameraFactory, "camera", ({
    {"pinhole", std::make_shared<PinholeCamera>},
    {"thinlens", std::make_shared<ThinlensCamera>},
    {"equirectangular", std::make_shared<EquirectangularCamera>},
    {"cubemap", std::make_shared<CubemapCamera>},
}))

}

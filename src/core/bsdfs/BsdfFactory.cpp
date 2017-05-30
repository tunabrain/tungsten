#include "BsdfFactory.hpp"

#include "DiffuseTransmissionBsdf.hpp"
#include "LambertianFiberBcsdf.hpp"
#include "RoughDielectricBsdf.hpp"
#include "RoughConductorBsdf.hpp"
#include "RoughPlasticBsdf.hpp"
#include "TransparencyBsdf.hpp"
#include "RoughWireBcsdf.hpp"
#include "DielectricBsdf.hpp"
#include "SmoothCoatBsdf.hpp"
#include "RoughCoatBsdf.hpp"
#include "ConductorBsdf.hpp"
#include "OrenNayarBsdf.hpp"
#include "ThinSheetBsdf.hpp"
#include "ForwardBsdf.hpp"
#include "LambertBsdf.hpp"
#include "PlasticBsdf.hpp"
#include "MirrorBsdf.hpp"
#include "ErrorBsdf.hpp"
#include "PhongBsdf.hpp"
#include "MixedBsdf.hpp"
#include "HairBcsdf.hpp"
#include "NullBsdf.hpp"
#include "Bsdf.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(BsdfFactory, "BSDF", ({
    {"lambert", std::make_shared<LambertBsdf>},
    {"phong", std::make_shared<PhongBsdf>},
    {"mixed", std::make_shared<MixedBsdf>},
    {"dielectric", std::make_shared<DielectricBsdf>},
    {"conductor", std::make_shared<ConductorBsdf>},
    {"mirror", std::make_shared<MirrorBsdf>},
    {"rough_conductor", std::make_shared<RoughConductorBsdf>},
    {"rough_dielectric", std::make_shared<RoughDielectricBsdf>},
    {"smooth_coat", std::make_shared<SmoothCoatBsdf>},
    {"null", std::make_shared<NullBsdf>},
    {"forward", std::make_shared<ForwardBsdf>},
    {"thinsheet", std::make_shared<ThinSheetBsdf>},
    {"oren_nayar", std::make_shared<OrenNayarBsdf>},
    {"plastic", std::make_shared<PlasticBsdf>},
    {"rough_plastic", std::make_shared<RoughPlasticBsdf>},
    {"rough_coat", std::make_shared<RoughCoatBsdf>},
    {"transparency", std::make_shared<TransparencyBsdf>},
    {"lambertian_fiber", std::make_shared<LambertianFiberBcsdf>},
    {"rough_wire", std::make_shared<RoughWireBcsdf>},
    {"hair", std::make_shared<HairBcsdf>},
    {"diffuse_transmission", std::make_shared<DiffuseTransmissionBsdf>}
}))

}

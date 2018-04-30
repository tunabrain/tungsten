#include "Tonemap.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(Tonemap::Type, "tonemap operator", ({
    {"linear", Tonemap::LinearOnly},
    {"gamma", Tonemap::GammaOnly},
    {"reinhard", Tonemap::Reinhard},
    {"filmic", Tonemap::Filmic},
    {"pbrt", Tonemap::Pbrt},
}))

}

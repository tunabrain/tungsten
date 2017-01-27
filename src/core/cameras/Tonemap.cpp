#include "Tonemap.hpp"

namespace Tungsten {

DECLARE_STRINGABLE_ENUM(Tonemap::Type, "tonemap operator", ({
    {"linear", Tonemap::LinearOnly},
    {"gamma", Tonemap::GammaOnly},
    {"reinhard", Tonemap::Reinhard},
    {"filmic", Tonemap::Filmic}
}))

}

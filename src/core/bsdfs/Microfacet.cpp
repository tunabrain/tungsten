#include "Microfacet.hpp"

namespace Tungsten {

DECLARE_STRINGABLE_ENUM(Microfacet::Distribution, "microfacet distribution", ({
    {"beckmann", Microfacet::Beckmann},
    {"phong", Microfacet::Phong},
    {"ggx", Microfacet::GGX}
}))

}

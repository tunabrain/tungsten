#ifndef QUADMATERIAL_HPP_
#define QUADMATERIAL_HPP_

#include "materials/BitmapTexture.hpp"

#include "bsdfs/Bsdf.hpp"

#include <memory>

namespace Tungsten {

struct QuadMaterial
{
    std::shared_ptr<Bsdf> bsdf;
    std::shared_ptr<BitmapTexture> emission;
    Vec3f emissionColor;
};

}

#endif /* QUADMATERIAL_HPP_ */

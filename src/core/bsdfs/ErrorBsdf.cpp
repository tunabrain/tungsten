#ifndef ERRORBSDF_CPP_
#define ERRORBSDF_CPP_

#include "ErrorBsdf.hpp"

#include "textures/ConstantTexture.hpp"

namespace Tungsten {

ErrorBsdf::ErrorBsdf()
{
    _albedo = std::make_shared<ConstantTexture>(Vec3f(1.0f, 0.0f, 0.0f));
}

}


#endif /* ERRORBSDF_CPP_ */

#ifndef ERRORBSDF_CPP_
#define ERRORBSDF_CPP_

#include "ErrorBsdf.hpp"

#include "materials/Texture.hpp"

namespace Tungsten
{

ErrorBsdf::ErrorBsdf()
{
    _albedo = std::make_shared<ConstantTextureRgb>(Vec3f(1.0f, 0.0f, 0.0f));
}

}


#endif /* ERRORBSDF_CPP_ */

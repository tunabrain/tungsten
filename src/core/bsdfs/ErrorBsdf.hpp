#ifndef ERRORBSDF_HPP_
#define ERRORBSDF_HPP_

#include "LambertBsdf.hpp"
namespace Tungsten
{

class ErrorBsdf : public LambertBsdf
{
public:
    ErrorBsdf()
    {
        _emission = Vec3f(1.0f, 0.0f, 0.0f);
    }
};

}

#endif /* ERRORBSDF_HPP_ */

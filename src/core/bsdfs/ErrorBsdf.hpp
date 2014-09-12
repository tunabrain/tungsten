#ifndef ERRORBSDF_HPP_
#define ERRORBSDF_HPP_

#include "LambertBsdf.hpp"

namespace Tungsten {

class ErrorBsdf : public LambertBsdf
{
public:
    ErrorBsdf();
};

}

#endif /* ERRORBSDF_HPP_ */

#ifndef COMPLEXIOR_HPP_
#define COMPLEXIOR_HPP_

#include "math/Vec.hpp"

#include <string>

namespace Tungsten {

namespace ComplexIorList {

bool lookup(const std::string &name, Vec3f &eta, Vec3f &k);

}

}

#endif /* COMPLEXIOR_HPP_ */

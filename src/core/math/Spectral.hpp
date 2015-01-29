#ifndef SPECTRAL_HPP_
#define SPECTRAL_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

namespace Spectral {

void spectralXyzWeights(int samples, float lambdas[], Vec3f weights[]);

inline Vec3f xyzToRgb(Vec3f xyz) {
    return Vec3f(
        3.240479f*xyz.x() + -1.537150f*xyz.y() + -0.498535f*xyz.z(),
       -0.969256f*xyz.x() +  1.875991f*xyz.y() +  0.041556f*xyz.z(),
        0.055648f*xyz.x() + -0.204043f*xyz.y() +  1.057311f*xyz.z()
    );
}

}

}

#endif /* SPECTRAL_HPP_ */

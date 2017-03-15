#ifndef REGRESSION_HPP_
#define REGRESSION_HPP_

#include "Pixmap.hpp"

namespace Tungsten {

Pixmap3f collaborativeRegression(const Pixmap3f &image, const Pixmap3f &guide,
        const std::vector<PixmapF> &features, const Pixmap3f &imageVariance,
        int F, int R, float k);

}

#endif /* REGRESSION_HPP_ */

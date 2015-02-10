#ifndef IMAGEIO_HPP_
#define IMAGEIO_HPP_

#include "Path.hpp"

#include "IntTypes.hpp"

#include <string>
#include <memory>

namespace Tungsten {

enum class TexelConversion
{
    REQUEST_RGB,
    REQUEST_AVERAGE,
    REQUEST_RED,
    REQUEST_GREEN,
    REQUEST_BLUE,
    REQUEST_ALPHA,
    REQUEST_AUTO,
};

namespace ImageIO {

bool isHdr(const Path &file);

std::unique_ptr<float[]> loadHdr(const Path &path, TexelConversion request, int &w, int &h);
std::unique_ptr<uint8[]> loadLdr(const Path &path, TexelConversion request, int &w, int &h,
        bool gammaCorrect = true);

bool saveHdr(const Path &path, const float *img, int w, int h, int channels);
bool saveLdr(const Path &path, const uint8 *img, int w, int h, int channels);

}

}

#endif /* IMAGEIO_HPP_ */

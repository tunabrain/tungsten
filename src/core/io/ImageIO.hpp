#ifndef IMAGEIO_HPP_
#define IMAGEIO_HPP_

#include "IntTypes.hpp"

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

bool isHdr(const std::string &file);

std::unique_ptr<float[]> loadHdr(const std::string &file, TexelConversion request, int &w, int &h);
std::unique_ptr<uint8[]> loadLdr(const std::string &file, TexelConversion request, int &w, int &h);

bool saveHdr(const std::string &file, const float *img, int w, int h, int channels);
bool saveLdr(const std::string &file, const uint8 *img, int w, int h, int channels);

}

}

#endif /* IMAGEIO_HPP_ */

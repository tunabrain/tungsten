#ifndef TEXTURECACHE_HPP_
#define TEXTURECACHE_HPP_

#include "ImageIO.hpp"

#include <utility>
#include <memory>
#include <string>
#include <map>

namespace Tungsten {

class BitmapTexture;

class TextureCache
{
    std::map<std::pair<std::string, TexelConversion>, std::shared_ptr<BitmapTexture>> _textures;
public:
    TextureCache() = default;

    std::shared_ptr<BitmapTexture> &fetchTexture(const std::string &path, TexelConversion conversion);

    void prune();
};

}

#endif /* TEXTURECACHE_HPP_ */

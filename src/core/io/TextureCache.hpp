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
    typedef std::pair<std::string, TexelConversion> KeyType;

    std::map<KeyType, std::shared_ptr<BitmapTexture>> _textures;
public:
    TextureCache() = default;

    std::shared_ptr<BitmapTexture> &fetchTexture(const std::string &path, TexelConversion conversion);

    void prune();

    const std::map<KeyType, std::shared_ptr<BitmapTexture>> &textures() const
    {
        return _textures;
    }

    std::map<KeyType, std::shared_ptr<BitmapTexture>> &textures()
    {
        return _textures;
    }
};

}

#endif /* TEXTURECACHE_HPP_ */

#ifndef TEXTURECACHE_HPP_
#define TEXTURECACHE_HPP_

#include "ImageIO.hpp"

#include <rapidjson/document.h>
#include <utility>
#include <memory>
#include <string>
#include <set>

namespace Tungsten {

class BitmapTexture;
class Scene;

class TextureCache
{
    typedef std::shared_ptr<BitmapTexture> KeyType;

    std::set<KeyType, std::function<bool(const KeyType &, const KeyType &)>> _textures;
public:
    TextureCache();

    std::shared_ptr<BitmapTexture> fetchTexture(const rapidjson::Value &value, TexelConversion conversion,
            const Scene *scene);
    std::shared_ptr<BitmapTexture> fetchTexture(PathPtr path, TexelConversion conversion,
            bool gammaCorrect = true, bool linear = true, bool clamp = false);

    void loadResources();
    void prune();

    const std::set<KeyType, std::function<bool(const KeyType &, const KeyType &)>> &textures() const
    {
        return _textures;
    }

    std::set<KeyType, std::function<bool(const KeyType &, const KeyType &)>> &textures()
    {
        return _textures;
    }
};

}

#endif /* TEXTURECACHE_HPP_ */

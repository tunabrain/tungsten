#ifndef TEXTURECACHE_HPP_
#define TEXTURECACHE_HPP_

#include "ImageIO.hpp"

#include <rapidjson/document.h>
#include <functional>
#include <utility>
#include <memory>
#include <string>
#include <set>

namespace Tungsten {

class BitmapTexture;
class IesTexture;
class JsonPtr;
class Scene;

class TextureCache
{
    typedef std::shared_ptr<BitmapTexture> BitmapKeyType;
    typedef std::shared_ptr<IesTexture> IesKeyType;

    std::set<BitmapKeyType, std::function<bool(const BitmapKeyType &, const BitmapKeyType &)>> _textures;
    std::set<IesKeyType, std::function<bool(const IesKeyType &, const IesKeyType &)>> _iesTextures;

public:
    TextureCache();

    std::shared_ptr<BitmapTexture> fetchTexture(JsonPtr value, TexelConversion conversion, const Scene *scene);
    std::shared_ptr<BitmapTexture> fetchTexture(PathPtr path, TexelConversion conversion,
            bool gammaCorrect = true, bool linear = true, bool clamp = false);

    std::shared_ptr<IesTexture> fetchIesTexture(JsonPtr value, const Scene *scene);
    std::shared_ptr<IesTexture> fetchIesTexture(PathPtr path, int resolution);

    void loadResources();
    void prune();
};

}

#endif /* TEXTURECACHE_HPP_ */

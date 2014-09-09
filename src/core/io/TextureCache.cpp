#include "TextureCache.hpp"
#include "FileUtils.hpp"

#include "materials/BitmapTexture.hpp"

namespace Tungsten {

std::shared_ptr<BitmapTexture> &TextureCache::fetchTexture(const std::string &path, TexelConversion conversion)
{
    KeyType key = std::make_pair(FileUtils::toAbsolutePath(path), conversion);
    auto iter = _textures.find(key);

    if (iter == _textures.end())
        iter = _textures.insert(std::make_pair(key, BitmapTexture::loadTexture(path, conversion))).first;

    if (!iter->second)
        DBG("Unable to load texture at '%s'", path);

    return iter->second;
}

void TextureCache::prune()
{
    for (auto i = _textures.cbegin(); i != _textures.cend();) {
        if (i->second.use_count() == 1)
            i = _textures.erase(i);
        else
            ++i;
    }
}

}

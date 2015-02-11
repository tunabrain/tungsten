#include "TextureCache.hpp"
#include "FileUtils.hpp"

#include "materials/BitmapTexture.hpp"

namespace Tungsten {

std::shared_ptr<BitmapTexture> &TextureCache::fetchTexture(PathPtr path, TexelConversion conversion)
{
    KeyType key = std::make_pair(path->absolute(), conversion);
    auto iter = _textures.find(key);

    if (iter == _textures.end())
        iter = _textures.insert(std::make_pair(key, std::make_shared<BitmapTexture>(std::move(path),
                conversion, true, true, false))).first;

    return iter->second;
}

void TextureCache::loadResources()
{
    for (auto &i : _textures)
        i.second->loadResources();
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

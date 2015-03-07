#include "TextureCache.hpp"
#include "FileUtils.hpp"

#include "materials/BitmapTexture.hpp"

namespace Tungsten {

TextureCache::TextureCache()
: _textures([](const KeyType &a, const KeyType &b) { return (!a || !b) ? a < b : (*a) < (*b); })
{
}

std::shared_ptr<BitmapTexture> TextureCache::fetchTexture(const rapidjson::Value &value, TexelConversion conversion,
        const Scene *scene)
{
    KeyType key = std::make_shared<BitmapTexture>();
    key->setTexelConversion(conversion);
    key->fromJson(value, *scene);

    auto iter = _textures.find(key);
    if (iter == _textures.end())
        iter = _textures.insert(key).first;

    return *iter;
}

std::shared_ptr<BitmapTexture> TextureCache::fetchTexture(PathPtr path, TexelConversion conversion,
        bool gammaCorrect, bool linear, bool clamp)
{
    KeyType key = std::make_shared<BitmapTexture>(std::move(path),
            conversion, gammaCorrect, linear, clamp);

    auto iter = _textures.find(key);
    if (iter == _textures.end())
        iter = _textures.insert(key).first;

    return *iter;
}

void TextureCache::loadResources()
{
    for (auto &i : _textures)
        i->loadResources();
}

void TextureCache::prune()
{
    for (auto i = _textures.cbegin(); i != _textures.cend();) {
        if (i->use_count() == 1)
            i = _textures.erase(i);
        else
            ++i;
    }
}

}

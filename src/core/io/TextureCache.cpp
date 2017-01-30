#include "TextureCache.hpp"
#include "FileUtils.hpp"

#include "textures/BitmapTexture.hpp"
#include "textures/IesTexture.hpp"

namespace Tungsten {

TextureCache::TextureCache()
: _textures([](const BitmapKeyType &a, const BitmapKeyType &b) { return (!a || !b) ? a < b : (*a) < (*b); }),
  _iesTextures([](const IesKeyType &a, const IesKeyType &b)    { return (!a || !b) ? a < b : (*a) < (*b); })
{
}

std::shared_ptr<BitmapTexture> TextureCache::fetchTexture(JsonPtr value, TexelConversion conversion, const Scene *scene)
{
    BitmapKeyType key = std::make_shared<BitmapTexture>();
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
    BitmapKeyType key = std::make_shared<BitmapTexture>(std::move(path),
            conversion, gammaCorrect, linear, clamp);

    auto iter = _textures.find(key);
    if (iter == _textures.end())
        iter = _textures.insert(key).first;

    return *iter;
}

std::shared_ptr<IesTexture> TextureCache::fetchIesTexture(JsonPtr value, const Scene *scene)
{
    IesKeyType key = std::make_shared<IesTexture>();
    key->fromJson(value, *scene);

    auto iter = _iesTextures.find(key);
    if (iter == _iesTextures.end())
        iter = _iesTextures.insert(key).first;

    return *iter;
}

std::shared_ptr<IesTexture> TextureCache::fetchIesTexture(PathPtr path, int resolution)
{
    IesKeyType key = std::make_shared<IesTexture>(std::move(path), resolution);

    auto iter = _iesTextures.find(key);
    if (iter == _iesTextures.end())
        iter = _iesTextures.insert(key).first;

    return *iter;
}

void TextureCache::loadResources()
{
    for (auto &i : _textures)
        i->loadResources();
    for (auto &i : _iesTextures)
        i->loadResources();
}

template<typename T, typename Comparator>
void pruneSet(std::set<std::shared_ptr<T>, Comparator> &set)
{
    for (auto i = set.cbegin(); i != set.cend();) {
        if (i->use_count() == 1)
            i = set.erase(i);
        else
            ++i;
    }
}

void TextureCache::prune()
{
    pruneSet(_textures);
    pruneSet(_iesTextures);
}

}

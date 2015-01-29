#ifndef RESOURCEPACKLOADER_HPP_
#define RESOURCEPACKLOADER_HPP_

#include "BlockDescriptor.hpp"
#include "ModelResolver.hpp"
#include "Model.hpp"
#include "File.hpp"

#include "materials/BitmapTexture.hpp"

#include "io/FileUtils.hpp"
#include "io/JsonUtils.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

namespace Tungsten {

static inline std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        elems.push_back(item);
    return std::move(elems);
}

struct BiomeColor {
    Vec3f foliageBottom, foliageTop;
    Vec3f grassBottom, grassTop;
    float height;
};

class ResourcePackLoader {
public:
    enum TintType
    {
        TINT_NONE = -1,
        TINT_FOLIAGE,
        TINT_GRASS,
        TINT_REDSTONE
    };

private:
    std::string _packPath;

    std::vector<BlockDescriptor> _blockDescriptors;
    std::vector<Model> _models;
    std::vector<const BlockVariant *> _blockMapping;

    std::vector<BiomeColor> _biomes;

    std::unique_ptr<ModelResolver> _resolver;

    void buildBlockMapping()
    {
        _blockMapping.resize(65536, nullptr);

        std::string json;
        try {
            json = FileUtils::loadText(_packPath + blockMapPath);
        } catch(std::runtime_error &e) {
            return;
        }
        rapidjson::Document document;
        document.Parse<0>(json.c_str());
        if (document.HasParseError() || !document.IsArray())
            return;

        std::unordered_map<std::string, const BlockVariant *> blockMap;
        for (const BlockDescriptor &b : _blockDescriptors) {
            for (const BlockVariant &v : b.variants())
                blockMap.insert(std::make_pair(b.name() + "." + v.variant(), &v));
            blockMap.insert(std::make_pair(b.name(), &b.variants().front()));
        }

        for (unsigned i = 0; i < document.Size(); ++i) {
            int id   = JsonUtils::as<int>(document[i], "id");
            int data = JsonUtils::as<int>(document[i], "data");
            std::string blockState = JsonUtils::as<std::string>(document[i], "blockstate");
            std::string variant = "normal";
            JsonUtils::fromJson(document[i], "variant", variant);
            std::string key = blockState + "." + variant;

            int index = (id << 4) | data;

            if (!blockMap.count(key)) {
                std::cout << "Warning: Could not find block " << blockState << " with variant " << variant;
                if (blockMap.count(blockState)) {
                    std::cout << " Using variant " << blockMap[blockState]->variant() << " instead";
                    _blockMapping[index] = blockMap[blockState];
                }
                std::cout << std::endl;
            } else {
                _blockMapping[index] = blockMap[key];
            }
        }
    }

    void loadStates()
    {
        File stateFolder(_packPath + stateBase);
        for (File iter = stateFolder.beginScan(File::extFilter("json")); iter.valid(); iter = stateFolder.scan()) {
            std::string json = FileUtils::loadText(iter.path().c_str());

            if (json.empty())
                continue;

            rapidjson::Document document;
            document.Parse<0>(json.c_str());
            if (document.HasParseError() || !document.IsObject())
                continue;

            _blockDescriptors.emplace_back(iter.baseName(), document, *_resolver);
        }
    }

    void loadModels(File dir, std::string base = "")
    {
        base = FileUtils::addSeparator(base);

        for (File iter = dir.beginScan(File::dirFilter()); iter.valid(); iter = dir.scan())
            loadModels(iter, base + iter.name());

        for (File iter = dir.beginScan(File::extFilter("json")); iter.valid(); iter = dir.scan()) {
            std::string json = FileUtils::loadText(iter.path().c_str());

            if (json.empty())
                continue;

            rapidjson::Document document;
            document.Parse<0>(json.c_str());
            if (document.HasParseError() || !document.IsObject())
                continue;

            _models.emplace_back(base + iter.baseName(), document);
        }
    }

    void fixTintIndices()
    {
        // Minecraft models with tint always use tint index 0,
        // even though there are three different types of
        // tinting used. This is fixed here.
        for (auto &iter : _resolver->builtModels()) {
            TintType type;
            if (iter.first.find("leaves") != std::string::npos)
                type = TINT_FOLIAGE;
            else if (iter.first.find("redstone") != std::string::npos)
                type = TINT_REDSTONE;
            else
                type = TINT_GRASS;

            for (TexturedQuad &quad : iter.second)
                if (quad.tintIndex != -1)
                    quad.tintIndex = type;
        }
    }

    void printVariants()
    {
        std::map<std::string, std::pair<std::set<std::string>, std::set<std::string>>> variants;

        for (const BlockDescriptor &desc : _blockDescriptors) {
            for (const BlockVariant &variant : desc.variants()) {
                for (std::string subVariant : split(variant.variant(), ',')) {
                    auto keyValue = split(subVariant, '=');
                    variants[keyValue[0]].first.insert(desc.name());
                    if (keyValue.size() == 2)
                        variants[keyValue[0]].second.insert(keyValue[1]);
                }
            }
        }

        for (const auto &variant : variants) {
            std::cout << variant.first << " {";
            size_t i = 0;
            for (const std::string &client : variant.second.first)
                std::cout << client << (++i != variant.second.first.size() ? ", " : "}");
            std::cout << std::endl;

            for (const std::string &value : variant.second.second)
                std::cout << "  " << value << std::endl;
        }
    }

    void generateBiomeColors()
    {
        CONSTEXPR float coolingRate = 1.0f/600.0f;

        BiomeColor defaultColor{
            Vec3f(0.62f, 0.5f, 0.3f), Vec3f(0.62f, 0.5f, 0.3f),
            Vec3f(0.56f, 0.5f, 0.3f), Vec3f(0.56f, 0.5f, 0.3f),
            1.0f
        };
        _biomes.resize(256, defaultColor);

        std::shared_ptr<BitmapTexture> grass(BitmapTexture::loadTexture(
                _packPath + textureBase + "colormap/grass.png", TexelConversion::REQUEST_RGB, false));
        std::shared_ptr<BitmapTexture> foliage(BitmapTexture::loadTexture(
                _packPath + textureBase + "colormap/foliage.png", TexelConversion::REQUEST_RGB, false));

        if (!grass || !foliage)
            return;

        std::string json = FileUtils::loadText(_packPath + biomePath);
        if (json.empty())
            return;

        rapidjson::Document document;
        document.Parse<0>(json.c_str());
        if (document.HasParseError() || !document.IsArray())
            return;

        for (unsigned i = 0; i < document.Size(); ++i) {
            int id = 0;
            float temperature = 0.0f, rainfall = 0.0f;

            JsonUtils::fromJson(document[i], "id", id);
            JsonUtils::fromJson(document[i], "temperature", temperature);
            JsonUtils::fromJson(document[i], "rainfall", rainfall);

            float tempBottom = clamp(temperature, 0.0f, 1.0f);
            float rainfallBottom = clamp(rainfall, 0.0f, 1.0f)*tempBottom;

            _biomes[id].foliageBottom = (*foliage)[Vec2f(1.0f - tempBottom, rainfallBottom)];
            _biomes[id].grassBottom   = (*grass)  [Vec2f(1.0f - tempBottom, rainfallBottom)];
            _biomes[id].foliageTop    = (*foliage)[Vec2f(1.0f, 0.0f)];
            _biomes[id].grassTop      = (*grass  )[Vec2f(1.0f, 0.0f)];
            _biomes[id].height        = tempBottom/coolingRate;
        }

        // Swampland. We're not going to do Perlin noise, so constant color will have to do
        _biomes[6].foliageBottom = _biomes[6].foliageTop = Vec3f(0.41f, 0.43f, 0.22f);
        _biomes[6].  grassBottom = _biomes[6].  grassTop = Vec3f(0.41f, 0.43f, 0.22f);
        // Sampland M
        _biomes[134] = _biomes[6];

        // Roofed forest
        _biomes[ 29].grassBottom = (_biomes[ 29].grassBottom + Vec3f(0.16f, 0.2f, 0.04f))*0.5f;
        _biomes[ 29].grassTop    = (_biomes[ 29].grassTop    + Vec3f(0.16f, 0.2f, 0.04f))*0.5f;
        _biomes[157].grassBottom = (_biomes[157].grassBottom + Vec3f(0.16f, 0.2f, 0.04f))*0.5f;
        _biomes[157].grassTop    = (_biomes[157].grassTop    + Vec3f(0.16f, 0.2f, 0.04f))*0.5f;

        // Mesa biomes
        for (int i = 0; i < 3; ++i) {
            _biomes[ 37 + i].grassBottom = _biomes[ 37 + i].grassTop = Vec3f(0.56f, 0.5f, 0.3f);
            _biomes[165 + i].grassBottom = _biomes[165 + i].grassTop = Vec3f(0.56f, 0.5f, 0.3f);
            _biomes[ 37 + i].foliageBottom = _biomes[ 37 + i].foliageTop = Vec3f(0.62f, 0.5f, 0.3f);
            _biomes[165 + i].foliageBottom = _biomes[165 + i].foliageTop = Vec3f(0.62f, 0.5f, 0.3f);
        }

        // In theory, we should undo gamma correction on biome colors.
        // However, they end up looking way too dark this way, so disabled for now.
        /*for (int i = 0; i < 256; ++i) {
            _biomes[i].grassBottom   = std::pow(_biomes[i].grassBottom,   2.2f);
            _biomes[i].grassTop      = std::pow(_biomes[i].grassTop,      2.2f);
            _biomes[i].foliageBottom = std::pow(_biomes[i].foliageBottom, 2.2f);
            _biomes[i].foliageTop    = std::pow(_biomes[i].foliageTop,    2.2f);
        }*/
    }

public:
    static const char *modelBase;
    static const char *stateBase;
    static const char *textureBase;
    static const char *blockMapPath;
    static const char *biomePath;

    ResourcePackLoader(const std::string packPath)
    : _packPath(FileUtils::addSeparator(packPath))
    {
        loadModels(File(_packPath + modelBase));
        _resolver.reset(new ModelResolver(_models));
        loadStates();
        buildBlockMapping();
        fixTintIndices();
        generateBiomeColors();
    }

    const std::vector<BlockDescriptor> &blockDescriptors() const
    {
        return _blockDescriptors;
    }

    const std::vector<BiomeColor> &biomeColors() const
    {
        return _biomes;
    }

    std::string textureBasePath() const
    {
        return FileUtils::addSeparator(_packPath) + textureBase;
    }

    const ModelRef *mapBlock(uint16 id, int idx) const
    {
        const BlockVariant *variant = _blockMapping[id];
        if (!variant)
            variant = _blockMapping[id & 0xFFF0];
        if (!variant)
            return nullptr;

        int model = MathUtil::hash32(idx) % variant->models().size();

        return &variant->models()[model];
    }
};

}

#endif /* RESOURCEPACKLOADER_HPP_ */

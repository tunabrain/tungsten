#ifndef RESOURCEPACKLOADER_HPP_
#define RESOURCEPACKLOADER_HPP_

#include "BlockDescriptor.hpp"
#include "ModelResolver.hpp"
#include "Model.hpp"
#include "File.hpp"

#include "../TraceableMinecraftMap.hpp"

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
    enum SpecialCase
    {
        CASE_NONE,
        CASE_GRASS,
        CASE_DOOR,
        CASE_PANE,
        CASE_FENCE,
        CASE_WALL,
        CASE_VINE,
        CASE_FENCE_GATE,
        CASE_TWO_FLOWER,
        CASE_STEM,

        CASE_REDSTONE,
        CASE_TRIPWIRE,
        CASE_STAIRS,
        CASE_REPEATER,
    };

    static CONSTEXPR int ID_REDSTONE      = 55;
    static CONSTEXPR int ID_SNOW          = 78;
    static CONSTEXPR int ID_SNOW_BLOCK    = 80;
    static CONSTEXPR int ID_PUMPKIN       = 86;
    static CONSTEXPR int ID_MELON         = 103;
    static CONSTEXPR int ID_PUMPKIN_STEM  = 104;
    static CONSTEXPR int ID_MELON_STEM    = 105;
    static CONSTEXPR int ID_TRIPWIRE_HOOK = 131;
    static CONSTEXPR int ID_TRIPWIRE      = 132;
    static CONSTEXPR int ID_WALL          = 139;

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

    std::vector<SpecialCase> _specialCases;
    std::vector<int> _blockOpacity, _connectsRedstone;

    std::vector<const BlockVariant *> _blockMapping;
    std::unordered_map<uint32, const BlockVariant *> _specialMapping;


    std::vector<BiomeColor> _biomes;

    std::unique_ptr<ModelResolver> _resolver;

    SpecialCase caseStringToType(const std::string &special) const
    {
        if (special == "grass")
            return CASE_GRASS;
        else if (special == "door")
            return CASE_DOOR;
        else if (special == "pane")
            return CASE_PANE;
        else if (special == "fence")
            return CASE_FENCE;
        else if (special == "fence_gate")
            return CASE_FENCE_GATE;
        else if (special == "wall")
            return CASE_WALL;
        else if (special == "vine")
            return CASE_VINE;
        else if (special == "two_flower")
            return CASE_TWO_FLOWER;
        else if (special == "stem")
            return CASE_STEM;
        else if (special == "redstone")
            return CASE_REDSTONE;
        else if (special == "tripwire")
            return CASE_TRIPWIRE;
        else if (special == "stairs")
            return CASE_STAIRS;
        else if (special == "repeater")
            return CASE_REPEATER;

        std::cout << "Don't understand special case " << special << std::endl;
        return CASE_NONE;
    }

    uint32 caseDataSize(SpecialCase type) const
    {
        switch (type) {
        case CASE_GRASS:      return 2;
        case CASE_DOOR:       return 32;
        case CASE_PANE:       return 16;
        case CASE_FENCE:      return 16;
        case CASE_FENCE_GATE: return 16;
        case CASE_WALL:       return 32;
        case CASE_VINE:       return 32;
        case CASE_TWO_FLOWER: return 2;
        case CASE_STEM:       return 12;
        case CASE_REDSTONE:   return 81;
        case CASE_TRIPWIRE:   return 64;
        case CASE_STAIRS:     return 40;
        default:              return 0;
        }
    }

    std::string caseDataToVariant(SpecialCase type, uint32 data) const
    {
        switch (type) {
        case CASE_GRASS:
            return data ? "snowy=true" : "snowy=false";
        case CASE_DOOR: {
            const char *directions[] = {"west", "north", "east", "south"};
            return tfm::format("facing=%s,half=%s,hinge=%s,open=%s",
                directions[data & 3],
                (data & 4) ? "upper" : "lower",
                (data & 8) ? "left" : "right",
                (data & 16) ? "true" : "false");
        }
        case CASE_PANE:
        case CASE_FENCE:
            return tfm::format("east=%s,north=%s,south=%s,west=%s",
                (data & 1) ? "true" : "false",
                (data & 2) ? "true" : "false",
                (data & 4) ? "true" : "false",
                (data & 8) ? "true" : "false");
        case CASE_WALL:
        case CASE_VINE:
            return tfm::format("east=%s,north=%s,south=%s,up=%s,west=%s",
                (data &  1) ? "true" : "false",
                (data &  2) ? "true" : "false",
                (data &  4) ? "true" : "false",
                (data & 16) ? "true" : "false",
                (data &  8) ? "true" : "false");
        case CASE_FENCE_GATE: {
            const char *directions[] = {"south", "west", "north", "east"};
            return tfm::format("facing=%s,in_wall=%s,open=%s",
                directions[data & 3],
                (data & 4) ? "true" : "false",
                (data & 8) ? "true" : "false");
        } case CASE_TWO_FLOWER:
            return tfm::format("half=%s", (data & 1) ? "upper" : "lower");
        case CASE_STEM:
                 if (data  <  8) return tfm::format("age=%i,facing=up", data);
            else if (data ==  8) return "facing=west";
            else if (data ==  9) return "facing=east";
            else if (data == 10) return "facing=north";
            else                 return "facing=south";
        case CASE_REDSTONE: {
            const char *types[] = {"none", "side", "up"};
            int east  = data % 3;
            int north = (data/3) % 3;
            int south = (data/9) % 3;
            int west  = data/27;
            return tfm::format("east=%s,north=%s,south=%s,west=%s",
                types[east],
                types[north],
                types[south],
                types[west]);
        } case CASE_TRIPWIRE:
            return tfm::format("attached=%s,east=%s,north=%s,south=%s,suspended=%s,west=%s",
                (data & 32) ? "true" : "false",
                (data &  1) ? "true" : "false",
                (data &  2) ? "true" : "false",
                (data &  4) ? "true" : "false",
                (data & 16) ? "true" : "false",
                (data &  8) ? "true" : "false");
        case CASE_STAIRS: {
            const char *directions[] = {"east", "west", "south", "north"};
            const char *shapes[] = {"straight", "outer_right", "outer_left", "inner_right", "inner_left"};
            return tfm::format("facing=east,half=bottom,shape=straight",
                directions[data & 3],
                (data & 4) ? "top" : "bottom",
                shapes[data >> 3]);
        }
        default:
            return "";
        }
    }

    void buildSpecialCase(std::unordered_map<std::string, const BlockVariant *> &blockMap,
            std::string blockState, std::string special, uint32 id, uint32 data, uint32 mask)
    {
        SpecialCase type = caseStringToType(special);

        for (int j = 0; j < 16; ++j)
            if ((j & mask) == data)
                _specialCases[(id << 4) | j] = type;

        for (uint32 i = 0; i < caseDataSize(type); ++i) {
            std::string variant = caseDataToVariant(type, i);
            std::string key = blockState + "." + variant;

            auto iter = blockMap.find(key);
            if (iter == blockMap.end()) {
                std::cout << "Warning: Could not find special cased block " << blockState <<
                        " with variant " << variant << std::endl;
                continue;
            }

            const BlockVariant *ref = iter->second;
            for (uint32 j = 0; j < 16; ++j)
                if ((j & mask) == data)
                    _specialMapping.insert(std::make_pair((((id << 4) | j) << 16) | i, ref));
        }
    }

    void buildBlockMapping()
    {
        _blockMapping.resize(65536, nullptr);
        _blockOpacity.resize(4096, 3);
        _connectsRedstone.resize(4096, 0);
        _specialCases.resize(65536, CASE_NONE);

        _blockOpacity[0] = 0;

        std::string json = FileUtils::loadText(_packPath + blockMapPath);
        if (json.empty())
            return;

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
            std::string variant = "normal";
            JsonUtils::fromJson(document[i], "variant", variant);
            int mask = 15;
            JsonUtils::fromJson(document[i], "mask", mask);
            std::string specialCase;
            JsonUtils::fromJson(document[i], "special_case", specialCase);
            int opacity = 3;
            JsonUtils::fromJson(document[i], "opacity", opacity);
            bool connectsRedstone = false;
            JsonUtils::fromJson(document[i], "connects_redstone", connectsRedstone);

            _blockOpacity[id] = opacity;
            _connectsRedstone[id] = connectsRedstone;

            std::string blockState = JsonUtils::as<std::string>(document[i], "blockstate");
            std::string key = blockState + "." + variant;

            if (!specialCase.empty()) {
                buildSpecialCase(blockMap, std::move(blockState), std::move(specialCase), id, data, mask);
                continue;
            }

            const BlockVariant *ref = nullptr;
            if (!blockMap.count(key)) {
                std::cout << "Warning: Could not find block " << blockState << " with variant " << variant;
                if (blockMap.count(blockState)) {
                    std::cout << " Using variant " << blockMap[blockState]->variant() << " instead";
                    ref = blockMap[blockState];
                }
                std::cout << std::endl;
            } else {
                ref = blockMap[key];
            }

            if (ref)
                for (int j = 0; j < 16; ++j)
                    if ((j & mask) == data)
                        _blockMapping[(id << 4) | j] = ref;
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
        return _packPath + textureBase;
    }

    const ModelRef *mapBlock(uint16 id, int idx) const
    {
        const BlockVariant *variant = _blockMapping[id];
        if (!variant) {
            variant = _blockMapping[id & 0xFFF0];
            if (variant)
                std::cout << (id >> 4) << " " << (id & 0xF) << std::endl;
        }
        if (!variant)
            return nullptr;

        int model = MathUtil::hash32(idx) % variant->models().size();

        return &variant->models()[model];
    }

    bool isSpecialBlock(uint16 id) const
    {
        return _specialCases[id] != CASE_NONE || _specialCases[id & 0xFFF0] != CASE_NONE;
    }

    const ModelRef *mapSpecialBlock(const TraceableMinecraftMap &map, int x, int y, int z, int idx, uint32 id) const
    {
        uint32 block = id >> 4;
        SpecialCase type = _specialCases[id];
        if (type == CASE_NONE)
            type = _specialCases[id & 0xFFF0];

        uint32 data = 0;
        switch (type) {
        case CASE_NONE:
            return nullptr;
        case CASE_GRASS: {
            uint32 topBlock = map.getBlock(x, y + 1, z) >> 4;
            if (topBlock == ID_SNOW || topBlock == ID_SNOW_BLOCK)
                data = 1;
            break;
        } case CASE_DOOR: {
            uint32 topHalf, bottomHalf;
            if (id & 8) {
                topHalf = id;
                bottomHalf = map.getBlock(x, y - 1, z);
                data = 4;
            } else {
                topHalf = map.getBlock(x, y + 1, z);
                bottomHalf = id;
            }
            data |= bottomHalf & 3;
            if (   topHalf & 1) data |= 8;
            if (bottomHalf & 4) data |= 16;
            break;
        } case CASE_PANE:
        case CASE_FENCE:
        case CASE_WALL: {
            uint32 northBlock = map.getBlock(x, y, z - 1) >> 4;
            uint32 southBlock = map.getBlock(x, y, z + 1) >> 4;
            uint32  eastBlock = map.getBlock(x + 1, y, z) >> 4;
            uint32  westBlock = map.getBlock(x - 1, y, z) >> 4;
            uint32    upBlock = map.getBlock(x, y + 1, z) >> 4;

            int opacityLevel = (type == CASE_PANE) ? 1 : 2;
            if ( eastBlock == block || _blockOpacity[ eastBlock] >= opacityLevel) data |= 1;
            if (northBlock == block || _blockOpacity[northBlock] >= opacityLevel) data |= 2;
            if (southBlock == block || _blockOpacity[southBlock] >= opacityLevel) data |= 4;
            if ( westBlock == block || _blockOpacity[ westBlock] >= opacityLevel) data |= 8;
            if (type == CASE_WALL && upBlock) data |= 16;
            break;
        } case CASE_VINE: {
            uint32 northBlock = map.getBlock(x, y, z - 1) >> 4;
            uint32 southBlock = map.getBlock(x, y, z + 1) >> 4;
            uint32  eastBlock = map.getBlock(x + 1, y, z) >> 4;
            uint32  westBlock = map.getBlock(x - 1, y, z) >> 4;
            uint32    upBlock = map.getBlock(x, y + 1, z) >> 4;

            if (_blockOpacity[ eastBlock] >= 2) data |= 1;
            if (_blockOpacity[northBlock] >= 2) data |= 2;
            if (_blockOpacity[southBlock] >= 2) data |= 4;
            if (_blockOpacity[ westBlock] >= 2) data |= 8;
            if (_blockOpacity[   upBlock] >= 2) data |= 16;
            break;
        } case CASE_FENCE_GATE: {
            data = id & 3;
            if (id & 4) data |= 8;

            bool xAxis = (id & 1) != 0;
            uint32 leftBlock, rightBlock;
            if (xAxis) {
                 leftBlock = map.getBlock(x - 1, y, z);
                rightBlock = map.getBlock(x + 1, y, z);
            } else {
                 leftBlock = map.getBlock(x, y, z - 1);
                rightBlock = map.getBlock(x, y, z + 1);
            }
            if (leftBlock == ID_WALL || rightBlock == ID_WALL)
                data |= 4;

            break;
        } case CASE_TWO_FLOWER: {
            if (id & 8) {
                data = 1;
                id = (id & 0xFFF0) | (map.getBlock(x, y - 1, z) & 7);
            }
            break;
        } case CASE_STEM: {
            uint32 fruitId = block == ID_PUMPKIN_STEM ? ID_PUMPKIN : ID_MELON;

            uint32 northBlock = map.getBlock(x, y, z - 1) >> 4;
            uint32 southBlock = map.getBlock(x, y, z + 1) >> 4;
            uint32  eastBlock = map.getBlock(x + 1, y, z) >> 4;
            uint32  westBlock = map.getBlock(x - 1, y, z) >> 4;

                 if ( westBlock == fruitId) data = 8;
            else if ( eastBlock == fruitId) data = 9;
            else if (northBlock == fruitId) data = 10;
            else if (southBlock == fruitId) data = 11;
            else                            data = id & 0xF;
            id = id & 0xFFF0;

            break;
        } case CASE_REDSTONE: {
            int north = 0, south = 0, east = 0, west = 0;
            uint32 northBlock = map.getBlock(x, y, z - 1) >> 4;
            uint32 southBlock = map.getBlock(x, y, z + 1) >> 4;
            uint32  eastBlock = map.getBlock(x + 1, y, z) >> 4;
            uint32  westBlock = map.getBlock(x - 1, y, z) >> 4;
            bool upBlocked = map.getBlock(x, y + 1, z) != 0;

            if (_connectsRedstone[northBlock]) north = 1;
            if (_connectsRedstone[southBlock]) south = 1;
            if (_connectsRedstone[ eastBlock])  east = 1;
            if (_connectsRedstone[ westBlock])  west = 1;
            if (!upBlocked && !_connectsRedstone[northBlock] && (map.getBlock(x, y + 1, z - 1) >> 4) == ID_REDSTONE) north = 2;
            if (!upBlocked && !_connectsRedstone[southBlock] && (map.getBlock(x, y + 1, z + 1) >> 4) == ID_REDSTONE) south = 2;
            if (!upBlocked && !_connectsRedstone[ eastBlock] && (map.getBlock(x + 1, y + 1, z) >> 4) == ID_REDSTONE)  east = 2;
            if (!upBlocked && !_connectsRedstone[ westBlock] && (map.getBlock(x - 1, y + 1, z) >> 4) == ID_REDSTONE)  west = 2;
            if (northBlock == 0 && (map.getBlock(x, y - 1, z - 1) >> 4) == ID_REDSTONE) north = 1;
            if (southBlock == 0 && (map.getBlock(x, y - 1, z + 1) >> 4) == ID_REDSTONE) south = 1;
            if ( eastBlock == 0 && (map.getBlock(x + 1, y - 1, z) >> 4) == ID_REDSTONE)  east = 1;
            if ( westBlock == 0 && (map.getBlock(x - 1, y - 1, z) >> 4) == ID_REDSTONE)  west = 1;

            data = ((west*3 + south)*3 + north)*3 + east;
            id = id & 0xFFF0;
            break;
        } case CASE_TRIPWIRE: {
            uint32 northBlock = map.getBlock(x, y, z - 1) >> 4;
            uint32 southBlock = map.getBlock(x, y, z + 1) >> 4;
            uint32  eastBlock = map.getBlock(x + 1, y, z) >> 4;
            uint32  westBlock = map.getBlock(x - 1, y, z) >> 4;

            if ( eastBlock == ID_TRIPWIRE ||  eastBlock == ID_TRIPWIRE_HOOK) data |= 1;
            if (northBlock == ID_TRIPWIRE || northBlock == ID_TRIPWIRE_HOOK) data |= 2;
            if (southBlock == ID_TRIPWIRE || southBlock == ID_TRIPWIRE_HOOK) data |= 4;
            if ( westBlock == ID_TRIPWIRE ||  westBlock == ID_TRIPWIRE_HOOK) data |= 8;
            if (id & 2) data |= 16;
            if (id & 4) data |= 32;
            id = id & 0xFFF0;
            break;
        } case CASE_STAIRS: {

        }}

        auto iter = _specialMapping.find((id << 16) | data);
        if (iter == _specialMapping.end())
            iter = _specialMapping.find(((id & 0xFFF0) << 16) | data);
        if (iter == _specialMapping.end()) {
            std::cout << "Unable to map " << block << ":" << (id & 0xF) << " with type " << type << " and data " << data << std::endl;
            return nullptr;
        }
        const BlockVariant *variant = iter->second;

        int model = MathUtil::hash32(idx) % variant->models().size();

        return &variant->models()[model];
    }
};

}

#endif /* RESOURCEPACKLOADER_HPP_ */

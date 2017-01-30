#include "ResourcePackLoader.hpp"
#include "TraceableMinecraftMap.hpp"

#include "textures/BitmapTexture.hpp"

#include "io/FileIterables.hpp"
#include "io/JsonDocument.hpp"
#include "io/FileUtils.hpp"
#include "io/Path.hpp"

#include "sampling/UniformSampler.hpp"

#include "Debug.hpp"

namespace Tungsten {
namespace MinecraftLoader {

const char *ResourcePackLoader::modelBase = "assets/minecraft/models/";
const char *ResourcePackLoader::stateBase = "assets/minecraft/blockstates/";
const char *ResourcePackLoader::textureBase = "assets/minecraft/textures/";
const char *ResourcePackLoader::blockMapPath = "mapping.json";
const char *ResourcePackLoader::biomePath = "biomes.json";
const char *ResourcePackLoader::emitterPath = "emitters.json";

static CONSTEXPR int RandSourceSize = 19937;

ResourcePackLoader::ResourcePackLoader(std::vector<Path> packPaths)
: _packPaths(std::move(packPaths))
{
    std::unordered_set<std::string> existing;
    for (const Path &packPath : _packPaths) {
        if (!packPath.exists())
            DBG("Note: Ignoring resource pack at %s: Directory does not exist", packPath);
        loadModels(packPath/modelBase, existing);
    }
    existing.clear();
    if (_models.empty())
        FAIL("Failed to load models");

    _resolver.reset(new ModelResolver(_models));

    UniformSampler sampler(0xBA5EBA11);
    _randSource.reset(new float[RandSourceSize]);
    for (int i = 0; i < RandSourceSize; ++i)
        _randSource[i] = sampler.next1D();

    for (const Path &packPath : _packPaths)
        loadStates(packPath, existing);
    existing.clear();

    buildBlockMapping();
    fixTintIndices();
    generateBiomeColors();
    loadEmitters();
}

ResourcePackLoader::SpecialCase ResourcePackLoader::caseStringToType(const std::string &special) const
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
    else if (special == "fire")
        return CASE_FIRE;

    std::cout << "Don't understand special case " << special << std::endl;
    return CASE_NONE;
}

uint32 ResourcePackLoader::caseDataSize(SpecialCase type) const
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
    case CASE_REDSTONE:   return 81*16;
    case CASE_TRIPWIRE:   return 64;
    case CASE_STAIRS:     return 40;
    case CASE_REPEATER:   return 32;
    case CASE_FIRE:       return 48;
    default:              return 0;
    }
}

std::string ResourcePackLoader::caseDataToVariant(SpecialCase type, uint32 data) const
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
        int west  = (data/27) % 3;
        int level = data/81;
        return tfm::format("east=%s,north=%s,south=%s,west=%s,level=%i",
            types[east],
            types[north],
            types[south],
            types[west],
            level);
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
        return tfm::format("facing=%s,half=%s,shape=%s",
            directions[data & 3],
            (data & 4) ? "top" : "bottom",
            shapes[data >> 3]);
    } case CASE_REPEATER: {
        const char *directions[] = {"south", "west", "north", "east"};
        return tfm::format("delay=%i,facing=%s,locked=%s",
            ((data >> 2) & 3) + 1,
            directions[data & 3],
            (data & 16) ? "true" : "false");
    } case CASE_FIRE:
        return tfm::format("alt=false,east=%s,flip=false,north=%s,south=%s,upper=%i,west=%s",
            (data &  1) ? "true" : "false",
            (data &  2) ? "true" : "false",
            (data &  4) ? "true" : "false",
            (data/16),
            (data &  8) ? "true" : "false");
    default:
        return "";
    }
}

void ResourcePackLoader::buildSpecialCase(std::unordered_map<std::string, const BlockVariant *> &blockMap,
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

void ResourcePackLoader::buildBlockMapping()
{
    _blockMapping.resize(65536, nullptr);
    _specialCases.resize(65536, CASE_NONE);
    _blockFlags.resize(4096, FLAG_OPAQUE | FLAG_CONNECTS_FENCE | FLAG_CONNECTS_PANE);
    _blockFlags[0] = 0;

    JsonDocument document(resolvePath(blockMapPath));
    if (!document.isArray())
        return;

    std::unordered_map<std::string, const BlockVariant *> blockMap;
    for (const BlockDescriptor &b : _blockDescriptors) {
        for (const BlockVariant &v : b.variants())
            blockMap.insert(std::make_pair(b.name() + "." + v.variant(), &v));
        blockMap.insert(std::make_pair(b.name(), &b.variants().front()));
    }

    for (unsigned i = 0; i < document.size(); ++i) {
        int id   = document[i].castField<int>("id");
        int data = document[i].castField<int>("data");
        std::string variant = "normal";
        document[i].getField("variant", variant);
        int mask = 15;
        document[i].getField("mask", mask);
        std::string specialCase;
        document[i].getField("special_case", specialCase);

        bool opaque = true, connectsFence = true, connectsPane = true, connectsRedstone = false, flammable = false;
        document[i].getField("opaque", opaque);
        document[i].getField("connects_fence", connectsFence);
        document[i].getField("connects_pane", connectsPane);
        document[i].getField("flammable", flammable);
        document[i].getField("connects_redstone", connectsRedstone);

        _blockFlags[id] =
            opaque          *FLAG_OPAQUE            |
            connectsFence   *FLAG_CONNECTS_FENCE    |
            connectsPane    *FLAG_CONNECTS_PANE     |
            connectsRedstone*FLAG_CONNECTS_REDSTONE |
            flammable       *FLAG_FLAMMABLE;

        std::string blockState = document[i].castField<std::string>("blockstate");
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

// For instancing reasons, we have to create artificial variants of
// redstone wire for each power level. This is to properly do tinting.
void ResourcePackLoader::duplicateRedstoneLevels(BlockDescriptor &state)
{
    std::vector<BlockVariant> variants = std::move(state.variants());
    state.variants().clear();

    std::unordered_map<const std::vector<TexturedQuad> *, int> modelCache;
    for (BlockVariant &variant : variants) {
        for (ModelRef &ref : variant.models()) {
            auto iter = modelCache.find(ref.builtModel());
            if (iter == modelCache.end()) {
                modelCache.insert(std::make_pair(ref.builtModel(), int(_redstoneDuplicates.size())));
                for (int i = 0; i < 16; ++i) {
                    _redstoneDuplicates.emplace_back(*ref.builtModel());
                    for (TexturedQuad &quad : _redstoneDuplicates.back()) {
                        quad.tintIndex = TINT_REDSTONE0 + i;
                        quad.overlay = "";
                    }
                }
            }
        }
    }

    for (BlockVariant &variant : variants) {
        for (int i = 0; i < 16; ++i) {
            state.variants().push_back(variant);
            state.variants().back().variant() += tfm::format(",level=%i", i);
            for (ModelRef &ref : state.variants().back().models())
                ref.setBuiltModel(&_redstoneDuplicates[modelCache[ref.builtModel()] + i]);
        }
    }
}

void ResourcePackLoader::loadStates(const Path &dir, std::unordered_set<std::string> &existing)
{
    for (const Path &p : (dir/stateBase).files("json")) {
        JsonDocument document(p);
        std::string name = p.baseName().asString();
        if (!existing.count(name)) {
            existing.insert(name);

            _blockDescriptors.emplace_back(std::move(name), document, *_resolver);
            if (_blockDescriptors.back().name() == "redstone_wire")
                duplicateRedstoneLevels(_blockDescriptors.back());
        }
    }
}

void ResourcePackLoader::loadModels(const Path &dir, std::unordered_set<std::string> &existing, std::string base)
{
    if (!base.empty())
        base += '/';

    for (const Path &p : dir.directories())
        loadModels(p, existing, base + p.fileName().asString());

    for (const Path &p : dir.files("json")) {
        JsonDocument document(p);
        std::string name = base + p.baseName().asString();
        if (!existing.count(name)) {
            existing.insert(name);
            _models.emplace_back(std::move(name), document);
        }
    }
}

void ResourcePackLoader::fixTintIndices()
{
    // Minecraft models with tint always use tint index 0,
    // even though there are three different types of
    // tinting used. This is fixed here.
    for (auto &iter : _resolver->builtModels()) {
        TintType type;
        if (iter.first.find("leaves") != std::string::npos)
            type = TINT_FOLIAGE;
        else if (iter.first.find("redstone") != std::string::npos)
            continue;
        else
            type = TINT_GRASS;

        for (TexturedQuad &quad : iter.second)
            if (quad.tintIndex != -1)
                quad.tintIndex = type;
    }
}

void ResourcePackLoader::generateBiomeColors()
{
    CONSTEXPR float coolingRate = 1.0f/600.0f;

    BiomeColor defaultColor{
        Vec3f(0.62f, 0.5f, 0.3f), Vec3f(0.62f, 0.5f, 0.3f),
        Vec3f(0.56f, 0.5f, 0.3f), Vec3f(0.56f, 0.5f, 0.3f),
        1.0f
    };
    _biomes.resize(256, defaultColor);

    BitmapTexture grass(resolvePath(Path(textureBase)/"colormap/grass.png"), TexelConversion::REQUEST_RGB, false, true, true);
    BitmapTexture foliage(resolvePath(Path(textureBase)/"colormap/foliage.png"), TexelConversion::REQUEST_RGB, false, true, true);
    grass.loadResources();
    foliage.loadResources();

    if (!grass.isValid() || !foliage.isValid())
        return;

    JsonDocument document(resolvePath(biomePath));
    for (unsigned i = 0; i < document.size(); ++i) {
        int id = 0;
        float temperature = 0.0f, rainfall = 0.0f;

        document[i].getField("id", id);
        document[i].getField("temperature", temperature);
        document[i].getField("rainfall", rainfall);

        float tempBottom = clamp(temperature, 0.0f, 1.0f);
        float rainfallBottom = clamp(rainfall, 0.0f, 1.0f)*tempBottom;

        _biomes[id].foliageBottom = foliage[Vec2f(1.0f - tempBottom, rainfallBottom)];
        _biomes[id].grassBottom   = grass  [Vec2f(1.0f - tempBottom, rainfallBottom)];
        _biomes[id].foliageTop    = foliage[Vec2f(1.0f, 0.0f)];
        _biomes[id].grassTop      = grass  [Vec2f(1.0f, 0.0f)];
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
}

void ResourcePackLoader::loadEmitters()
{
    JsonDocument document(resolvePath(emitterPath));
    for (unsigned i = 0; i < document.size(); ++i) {
        std::string texture, mask;
        float primaryScale = 1.0f;
        float secondaryScale = 1.0f;

        if (!document[i].getField("texture", texture))
            continue;

        document[i].getField("mask", mask);
        document[i].getField("primary_scale", primaryScale);
        document[i].getField("secondary_scale", secondaryScale);

        _emitters.insert(std::make_pair(std::move(texture), EmitterInfo{
            primaryScale, secondaryScale, mask
        }));
    }
}

static const ModelRef *selectModel(const std::vector<ModelRef> &models, int idx, const float *randSource)
{
    int model = 0;
    if (models.size() > 1) {
        float f = randSource[idx % RandSourceSize];
        model = int(models.size()) - 1;
        for (size_t i = 0; i < models.size(); ++i) {
            if (f < models[i].weight()) {
                model = int(i);
                break;
            }
        }
    }
    return &models[model];
}

const ModelRef *ResourcePackLoader::mapBlock(uint16 id, int idx) const
{
    const BlockVariant *variant = _blockMapping[id];
    if (!variant) {
        variant = _blockMapping[id & 0xFFF0];
        if (variant)
            std::cout << (id >> 4) << " " << (id & 0xF) << std::endl;
    }
    if (!variant)
        return nullptr;

    return selectModel(variant->models(), idx, _randSource.get());
}

const ModelRef *ResourcePackLoader::mapSpecialBlock(const TraceableMinecraftMap &map, int x, int y, int z,
        int idx, uint32 id) const
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

        uint32 flags = (type == CASE_PANE) ? FLAG_CONNECTS_PANE : FLAG_CONNECTS_FENCE;
        if ( eastBlock == block || (_blockFlags[ eastBlock] & flags) != 0) data |= 1;
        if (northBlock == block || (_blockFlags[northBlock] & flags) != 0) data |= 2;
        if (southBlock == block || (_blockFlags[southBlock] & flags) != 0) data |= 4;
        if ( westBlock == block || (_blockFlags[ westBlock] & flags) != 0) data |= 8;
        if (type == CASE_WALL && upBlock) data |= 16;
        break;
    } case CASE_VINE: {
        uint32 northBlock = map.getBlock(x, y, z - 1) >> 4;
        uint32 southBlock = map.getBlock(x, y, z + 1) >> 4;
        uint32  eastBlock = map.getBlock(x + 1, y, z) >> 4;
        uint32  westBlock = map.getBlock(x - 1, y, z) >> 4;
        uint32    upBlock = map.getBlock(x, y + 1, z) >> 4;

        if (_blockFlags[ eastBlock] & FLAG_CONNECTS_PANE) data |= 1;
        if (_blockFlags[northBlock] & FLAG_CONNECTS_PANE) data |= 2;
        if (_blockFlags[southBlock] & FLAG_CONNECTS_PANE) data |= 4;
        if (_blockFlags[ westBlock] & FLAG_CONNECTS_PANE) data |= 8;
        if (_blockFlags[   upBlock] & FLAG_CONNECTS_PANE) data |= 16;
        break;
    } case CASE_FENCE_GATE: {
        data = id & 3;
        if (id & 4) data |= 8;

        bool xAxis = (id & 1) != 0;
        uint32 leftBlock, rightBlock;
        if (xAxis) {
             leftBlock = map.getBlock(x, y, z - 1) >> 4;
            rightBlock = map.getBlock(x, y, z + 1) >> 4;
        } else {
             leftBlock = map.getBlock(x - 1, y, z) >> 4;
            rightBlock = map.getBlock(x + 1, y, z) >> 4;
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
        bool connectsNorth = (_blockFlags[northBlock] & FLAG_CONNECTS_REDSTONE) != 0;
        bool connectsSouth = (_blockFlags[southBlock] & FLAG_CONNECTS_REDSTONE) != 0;
        bool connectsEast  = (_blockFlags[ eastBlock] & FLAG_CONNECTS_REDSTONE) != 0;
        bool connectsWest  = (_blockFlags[ westBlock] & FLAG_CONNECTS_REDSTONE) != 0;
        bool upBlocked = map.getBlock(x, y + 1, z) != 0;

        if (connectsNorth) north = 1;
        if (connectsSouth) south = 1;
        if (connectsEast )  east = 1;
        if (connectsWest )  west = 1;
        if (!upBlocked && !connectsNorth && (map.getBlock(x, y + 1, z - 1) >> 4) == ID_REDSTONE) north = 2;
        if (!upBlocked && !connectsSouth && (map.getBlock(x, y + 1, z + 1) >> 4) == ID_REDSTONE) south = 2;
        if (!upBlocked && !connectsEast  && (map.getBlock(x + 1, y + 1, z) >> 4) == ID_REDSTONE)  east = 2;
        if (!upBlocked && !connectsWest  && (map.getBlock(x - 1, y + 1, z) >> 4) == ID_REDSTONE)  west = 2;
        if (northBlock == 0 && (map.getBlock(x, y - 1, z - 1) >> 4) == ID_REDSTONE) north = 1;
        if (southBlock == 0 && (map.getBlock(x, y - 1, z + 1) >> 4) == ID_REDSTONE) south = 1;
        if ( eastBlock == 0 && (map.getBlock(x + 1, y - 1, z) >> 4) == ID_REDSTONE)  east = 1;
        if ( westBlock == 0 && (map.getBlock(x - 1, y - 1, z) >> 4) == ID_REDSTONE)  west = 1;

        data = ((((id & 15)*3 + west)*3 + south)*3 + north)*3 + east;
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
        uint32 front, back;
        if (id & 2) {
            front = map.getBlock(x, y, z - 1);
            back = map.getBlock(x, y, z + 1);
        } else {
            front = map.getBlock(x - 1, y, z);
            back = map.getBlock(x + 1, y, z);
        }
        if (id & 1)
            std::swap(front, back);

        uint32 shape = 0;
        if (_specialCases[front & 0xFFF0] == CASE_STAIRS && ((front ^ id) & 6) == 2)
            shape = 3 + ((front ^ id ^ (id >> 1) ^ (id >> 2)) & 1);
        else if (_specialCases[back & 0xFFF0] == CASE_STAIRS && ((back ^ id) & 6) == 2)
            shape = 1 + ((back ^ id ^ (id >> 1) ^ (id >> 2)) & 1);

        data = (id & 7) | (shape << 3);
        id = id & 0xFFF0;
        break;
    } case CASE_REPEATER: {
        bool isLocked;
        if (id & 1) {
            uint32 left = map.getBlock(x, y, z - 1);
            uint32 right = map.getBlock(x, y, z + 1);
            isLocked =
                (((left  >> 4) == ID_REPEATER_ON || (left  >> 4) == ID_COMPARATOR_ON) && (left & 3) == 2) ||
                (((right >> 4) == ID_REPEATER_ON || (right >> 4) == ID_COMPARATOR_ON) && (right & 3) == 0);
        } else {
            uint32 left = map.getBlock(x - 1, y, z);
            uint32 right = map.getBlock(x + 1, y, z);
            isLocked =
                (((left  >> 4) == ID_REPEATER_ON || (left  >> 4) == ID_COMPARATOR_ON) && (left & 3) == 1) ||
                (((right >> 4) == ID_REPEATER_ON || (right >> 4) == ID_COMPARATOR_ON) && (right & 3) == 3);
        }

        data = (id & 15) | (isLocked ? 16 : 0);
        id = id & 0xFFF0;
        break;
    } case CASE_FIRE: {
        uint32 northBlock = map.getBlock(x, y, z - 1) >> 4;
        uint32 southBlock = map.getBlock(x, y, z + 1) >> 4;
        uint32  eastBlock = map.getBlock(x + 1, y, z) >> 4;
        uint32  westBlock = map.getBlock(x - 1, y, z) >> 4;
        uint32    upBlock = map.getBlock(x, y + 1, z) >> 4;
        uint32  downBlock = map.getBlock(x, y - 1, z) >> 4;

        if (_blockFlags[ eastBlock] & FLAG_FLAMMABLE) data |= 1;
        if (_blockFlags[northBlock] & FLAG_FLAMMABLE) data |= 2;
        if (_blockFlags[southBlock] & FLAG_FLAMMABLE) data |= 4;
        if (_blockFlags[ westBlock] & FLAG_FLAMMABLE) data |= 8;
        if (_blockFlags[   upBlock] & FLAG_FLAMMABLE) data |= (((x ^ z) & 1) + 1)*16;
        if (_blockFlags[ downBlock] & (FLAG_FLAMMABLE | FLAG_OPAQUE)) data = 0;
        id = id & 0xFFF0;
        break;
    }}

    auto iter = _specialMapping.find((id << 16) | data);
    if (iter == _specialMapping.end())
        iter = _specialMapping.find(((id & 0xFFF0) << 16) | data);
    if (iter == _specialMapping.end()) {
        std::cout << "Unable to map " << block << ":" << (id & 0xF) << " with type " << type << " and data " << data << std::endl;
        return nullptr;
    }

    return selectModel(iter->second->models(), idx, _randSource.get());
}

Path ResourcePackLoader::resolvePath(const Path &p) const
{
    for (const Path &packPath : _packPaths)
        if ((packPath/p).exists())
            return packPath/p;
    return p;
}

}
}

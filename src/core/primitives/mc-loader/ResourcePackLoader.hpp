#ifndef RESOURCEPACKLOADER_HPP_
#define RESOURCEPACKLOADER_HPP_

#include "BlockDescriptor.hpp"
#include "ModelResolver.hpp"
#include "Model.hpp"

#include "io/Path.hpp"

#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class TraceableMinecraftMap;

struct BiomeColor
{
    Vec3f foliageBottom, foliageTop;
    Vec3f grassBottom, grassTop;
    float height;
};

struct EmitterInfo
{
    float primaryScale;
    float secondaryScale;
    std::string mask;
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
        CASE_FIRE,
    };

    enum BlockFlags
    {
        FLAG_OPAQUE            = 0x01,
        FLAG_CONNECTS_FENCE    = 0x02,
        FLAG_CONNECTS_PANE     = 0x04,
        FLAG_CONNECTS_REDSTONE = 0x08,
        FLAG_FLAMMABLE         = 0x10,
    };

    static CONSTEXPR int ID_WATER_FLOWING = 8;
    static CONSTEXPR int ID_WATER         = 9;
    static CONSTEXPR int ID_LAVA_FLOWING  = 10;
    static CONSTEXPR int ID_LAVA          = 11;
    static CONSTEXPR int ID_REDSTONE      = 55;
    static CONSTEXPR int ID_SNOW          = 78;
    static CONSTEXPR int ID_SNOW_BLOCK    = 80;
    static CONSTEXPR int ID_PUMPKIN       = 86;
    static CONSTEXPR int ID_REPEATER_ON   = 94;
    static CONSTEXPR int ID_MELON         = 103;
    static CONSTEXPR int ID_PUMPKIN_STEM  = 104;
    static CONSTEXPR int ID_MELON_STEM    = 105;
    static CONSTEXPR int ID_TRIPWIRE_HOOK = 131;
    static CONSTEXPR int ID_TRIPWIRE      = 132;
    static CONSTEXPR int ID_WALL          = 139;
    static CONSTEXPR int ID_COMPARATOR_ON = 150;

public:
    enum TintType
    {
        TINT_NONE = -1,
        TINT_FOLIAGE,
        TINT_GRASS,
        TINT_REDSTONE0,
        TINT_REDSTONE1,
        TINT_REDSTONE2,
        TINT_REDSTONE3,
        TINT_REDSTONE4,
        TINT_REDSTONE5,
        TINT_REDSTONE6,
        TINT_REDSTONE7,
        TINT_REDSTONE8,
        TINT_REDSTONE9,
        TINT_REDSTONE10,
        TINT_REDSTONE11,
        TINT_REDSTONE12,
        TINT_REDSTONE13,
        TINT_REDSTONE14,
        TINT_REDSTONE15,
    };

private:
    std::vector<Path> _packPaths;

    std::vector<BlockDescriptor> _blockDescriptors;
    std::vector<Model> _models;

    std::vector<SpecialCase> _specialCases;
    std::vector<uint32> _blockFlags;

    std::vector<const BlockVariant *> _blockMapping;
    std::unordered_map<uint32, const BlockVariant *> _specialMapping;
    std::unordered_map<std::string, EmitterInfo> _emitters;

    std::vector<BiomeColor> _biomes;

    std::unique_ptr<ModelResolver> _resolver;
    std::vector<std::vector<TexturedQuad>> _redstoneDuplicates;

    std::unique_ptr<float[]> _randSource;

    SpecialCase caseStringToType(const std::string &special) const;
    uint32 caseDataSize(SpecialCase type) const;
    std::string caseDataToVariant(SpecialCase type, uint32 data) const;

    void buildSpecialCase(std::unordered_map<std::string, const BlockVariant *> &blockMap,
            std::string blockState, std::string special, uint32 id, uint32 data, uint32 mask);
    void buildBlockMapping();

    void duplicateRedstoneLevels(BlockDescriptor &state);
    void loadStates(const Path &dir, std::unordered_set<std::string> &existing);
    void loadModels(const Path &dir, std::unordered_set<std::string> &existing, std::string base = "");
    void fixTintIndices();
    void generateBiomeColors();
    void loadEmitters();

public:
    static const char *modelBase;
    static const char *stateBase;
    static const char *textureBase;
    static const char *blockMapPath;
    static const char *biomePath;
    static const char *emitterPath;

    ResourcePackLoader(std::vector<Path> packPaths);

    const ModelRef *mapBlock(uint16 id, int idx) const;
    const ModelRef *mapSpecialBlock(const TraceableMinecraftMap &map, int x, int y, int z, int idx, uint32 id) const;

    Path resolvePath(const Path &p) const;

    const std::vector<BlockDescriptor> &blockDescriptors() const
    {
        return _blockDescriptors;
    }

    const std::vector<BiomeColor> &biomeColors() const
    {
        return _biomes;
    }

    bool isEmissive(const std::string &texture) const
    {
        return _emitters.count(texture) != 0;
    }

    const EmitterInfo *emitterInfo(const std::string &texture) const
    {
        auto iter = _emitters.find(texture);
        if (iter == _emitters.end())
            return nullptr;
        else
            return &iter->second;
    }

    bool isSpecialBlock(uint32 id) const
    {
        return _specialCases[id] != CASE_NONE || _specialCases[id & 0xFFF0] != CASE_NONE;
    }

    bool isOpaque(uint32 id) const
    {
        return _blockFlags[id >> 4] & FLAG_OPAQUE;
    }

    bool isWater(uint32 id) const
    {
        return (id >> 4) == ID_WATER || (id >> 4) == ID_WATER_FLOWING;
    }

    bool isLava(uint32 id) const
    {
        return (id >> 4) == ID_LAVA || (id >> 4) == ID_LAVA_FLOWING;
    }

    bool isLiquid(uint32 id) const
    {
        return isLava(id) || isWater(id);
    }

    int liquidLevel(uint32 id) const
    {
        return ((id & 8) != 0) ? 8 : (8 - (id & 0x7));
    }

    std::string liquidTexture(bool lava, bool still) const
    {
        if (still)
            return lava ? "blocks/lava_still" : "blocks/water_still";
        else
            return lava ? "blocks/lava_flow" : "blocks/water_flow";
    }
};

}
}

#endif /* RESOURCEPACKLOADER_HPP_ */

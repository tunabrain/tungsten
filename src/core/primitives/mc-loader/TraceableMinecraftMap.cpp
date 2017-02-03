#include "TraceableMinecraftMap.hpp"
#include "ResourcePackLoader.hpp"
#include "BiomeTexture.hpp"
#include "MapLoader.hpp"
#include "ModelRef.hpp"

#include "primitives/TriangleMesh.hpp"

#include "textures/ConstantTexture.hpp"

#include "cameras/PinholeCamera.hpp"

#include "bsdfs/TransparencyBsdf.hpp"
#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/NullBsdf.hpp"

#include "math/Mat4f.hpp"

#include "io/JsonDocument.hpp"
#include "io/JsonObject.hpp"
#include "io/ImageIO.hpp"
#include "io/MeshIO.hpp"
#include "io/Scene.hpp"

#include <tinyformat/tinyformat.hpp>
#include <unordered_set>

namespace Tungsten {
namespace MinecraftLoader {

struct MapIntersection
{
    QuadGeometry::Intersection isect;
    bool wasPrimary;
};

TraceableMinecraftMap::TraceableMinecraftMap()
: _missingBsdf(std::make_shared<LambertBsdf>())
{
    _missingBsdf->setAlbedo(std::make_shared<ConstantTexture>(0.2f));

    _materials.emplace_back();
    _materials.back().bsdf = _missingBsdf;
    _materials.back().opaqueBounds = Box2f(Vec2f(0.0f), Vec2f(1.0f));
}

TraceableMinecraftMap::TraceableMinecraftMap(const TraceableMinecraftMap &o)
: Primitive(o)
{
    _mapPath = o._mapPath;
    _packPaths = o._packPaths;

    _missingBsdf = o._missingBsdf;
    _bsdfCache = o._bsdfCache;
}

void TraceableMinecraftMap::getTexProperties(const Path &path, int w, int h,
        int &tileW, int &tileH, bool &clamp, bool &linear)
{
    tileW = w;
    tileH = w;
    linear = false;
    clamp = false;

    Path meta(path + ".mcmeta");
    if (!meta.exists())
        return;

    JsonDocument document(meta);
    if (auto animation = document["animation"]) {
        int numTilesX, numTilesY;
        if (animation.getField("width", numTilesX))
            tileW = w/numTilesX;
        if (animation.getField("height", numTilesY))
            tileH = h/numTilesY;
    }

    if (auto texture = document["texture"]) {
        texture.getField("blur", linear);
        texture.getField("clamp", clamp);
    }
}

void TraceableMinecraftMap::loadTexture(ResourcePackLoader &pack, const std::string &name,
        std::shared_ptr<BitmapTexture> &albedo, std::shared_ptr<BitmapTexture> &opacity,
        Box2f &opaqueBounds, Vec4c tint, const uint8 *mask, int maskW, int maskH)
{
    Path path = pack.resolvePath(Path(ResourcePackLoader::textureBase)/name + ".png");

    int w, h;
    std::unique_ptr<uint8[]> img = ImageIO::loadLdr(path, TexelConversion::REQUEST_RGB, w, h);

    if (!img)
        return;

    int tileW, tileH;
    bool linear, clamp;
    getTexProperties(path, w, h, tileW, tileH, clamp, linear);

    int yOffset = ((h/tileH)/2)*tileH;
    bool opaque = true;
    Box2i bounds;
    std::unique_ptr<uint8[]> tile(new uint8[tileW*tileH*4]), alpha;
    for (int y = 0; y < tileH; ++y) {
        for (int x = 0; x < tileW; ++x) {
            for (int i = 0; i < 4; ++i)
                tile[i + 4*(x + y*tileW)] = (uint32(img[i + 4*(x + (y + yOffset)*w)])*tint[i])/255;
            uint8 &alpha = tile[3 + 4*(x + y*tileW)];

            if (mask)
                alpha = (alpha*uint32(mask[(x*maskW)/tileW + ((y*maskH)/tileH)*maskW]))/255;

            opaque = opaque && (alpha == 0xFF);
            if (alpha > 0) {
                bounds.grow(Vec2i(x, y));
                bounds.grow(Vec2i(x + 1, y + 1));
            }
            if (alpha == 0)
                for (int i = 0; i < 3; ++i)
                    tile[i + 4*(x + y*tileW)] = 0;
        }
    }
    opaqueBounds = Box2f(Vec2f(bounds.min())/float(tileW), Vec2f(bounds.max())/float(tileH));

    if (!opaque) {
        alpha.reset(new uint8[tileH*tileW]);
        for (int i = 0; i < tileH*tileW; ++i)
            alpha[i] = tile[i*4 + 3];
    }

    albedo = std::make_shared<BitmapTexture>(tile.release(), tileW, tileH,
            BitmapTexture::TexelType::RGB_LDR, linear, clamp);

    if (!opaque)
        opacity = std::make_shared<BitmapTexture>(alpha.release(), tileW, tileH,
                BitmapTexture::TexelType::SCALAR_LDR, linear, clamp);
}

void TraceableMinecraftMap::loadMaskedBsdf(std::shared_ptr<Bsdf> &bsdf, Box2f &opaqueBounds,
        std::shared_ptr<BitmapTexture> &emission, ResourcePackLoader &pack, const TexturedQuad &quad,
        Vec4c filter, bool emissive, const uint8 *mask, int maskW, int maskH)
{
    std::shared_ptr<BitmapTexture> albedo, opacity;
    loadTexture(pack, quad.texture, albedo, opacity, opaqueBounds, filter, mask, maskW, maskH);

    if (!albedo || opaqueBounds.empty())
        return;

    std::shared_ptr<BitmapTexture> overlayAlbedo, overlayMask;
    Box2f overlayBounds;
    if (!quad.overlay.empty())
        loadTexture(pack, quad.overlay, overlayAlbedo, overlayMask, overlayBounds, Vec4c(255), mask, maskW, maskH);

    std::shared_ptr<BitmapTexture> substrate, overlay, overlayOpacity;
    if (overlayAlbedo) {
        substrate = albedo;
        overlay = overlayAlbedo;
        overlayOpacity = overlayMask;
    } else {
        overlay = albedo;
    }

    std::shared_ptr<Texture> base;
    bool hasBiomeTint =
        quad.tintIndex == ResourcePackLoader::TINT_FOLIAGE ||
        quad.tintIndex == ResourcePackLoader::TINT_GRASS;

    if (overlayAlbedo || hasBiomeTint)
        base = std::make_shared<BiomeTexture>(substrate, overlay, overlayOpacity, _biomeMap, quad.tintIndex);
    else
        base = albedo;

    if (emissive) {
        emission = albedo;
        bsdf = std::make_shared<NullBsdf>();
    } else {
        bsdf = std::make_shared<LambertBsdf>();
        bsdf->setAlbedo(base);
    }

    if (opacity)
        bsdf = std::make_shared<TransparencyBsdf>(opacity, bsdf);
}

int TraceableMinecraftMap::fetchBsdf(ResourcePackLoader &pack, const TexturedQuad &quad)
{
    std::string key = quad.texture;

    if (!quad.overlay.empty())
        key += "&" + quad.overlay;

    Vec4c filter(255);
    if (quad.tintIndex == ResourcePackLoader::TINT_FOLIAGE)
        key += "-BIOME_FOLIAGE";
    else if (quad.tintIndex == ResourcePackLoader::TINT_GRASS)
        key += "-BIOME_GRASS";
    else if (quad.tintIndex != ResourcePackLoader::TINT_NONE) {
        int level = quad.tintIndex - ResourcePackLoader::TINT_REDSTONE0;
        key += tfm::format("-REDSTONE_TINT%i", level);
        filter = Vec4c(Vec4i((191*level)/15 + 64, (64*level)/15, 0, 255));
    }

    auto iter = _bsdfCache.find(key);
    if (iter != _bsdfCache.end())
        return iter->second;

    _materials.emplace_back();
    QuadMaterial &material = _materials.back();
    material.bsdf = _missingBsdf;

    bool isEmissive = pack.isEmissive(quad.texture);

    std::unique_ptr<uint8[]> emitterMask;
    int emitterMaskW, emitterMaskH;
    if (isEmissive) {
        const EmitterInfo &info = *pack.emitterInfo(quad.texture);
        if (!info.mask.empty())
            emitterMask = ImageIO::loadLdr(pack.resolvePath(Path(info.mask)),
                    TexelConversion::REQUEST_AVERAGE, emitterMaskW, emitterMaskH, false);

        material.primaryScale = info.primaryScale;
        material.secondaryScale = info.secondaryScale;
    }

    if (isEmissive) {
        loadMaskedBsdf(material.emitterBsdf, material.emitterOpaqueBounds, material.emission, pack,
                quad, filter, true, emitterMask.get(), emitterMaskW, emitterMaskH);

        material.sampleWeight = material.emission->maximum().max()*material.secondaryScale;
    }

    if (emitterMask)
        for (int i = 0; i < emitterMaskW*emitterMaskH; ++i)
            emitterMask[i] = 0xFF - emitterMask[i];

    if (!isEmissive || emitterMask)
        loadMaskedBsdf(material.bsdf, material.opaqueBounds, material.emission, pack,
                quad, filter, false, emitterMask.get(), emitterMaskW, emitterMaskH);

    _bsdfCache.insert(std::make_pair(key, int(_materials.size()) - 1));

    return _materials.size() - 1;
}

void TraceableMinecraftMap::buildBiomeColors(ResourcePackLoader &pack, int rx, int rz, uint8 *biomes)
{
    std::unique_ptr<uint8[]> grassTop(new uint8[256*256*4]), grassBottom(new uint8[256*256*4]);
    std::unique_ptr<uint8[]> foliageTop(new uint8[256*256*4]), foliageBottom(new uint8[256*256*4]);
    std::unique_ptr<uint8[]> tmp(new uint8[256*256*4]);
    std::unique_ptr<float[]> heights(new float[256*256*4]);

    auto get = [](std::unique_ptr<uint8[]> &dst, int x, int z) -> Vec4c& {
        return *(reinterpret_cast<Vec4c *>(dst.get()) + x + z*256);
    };
    auto toRgba = [](Vec3f x) {
        return Vec4c(Vec4f(x.x(), x.y(), x.z(), 1.0f)*255.0f);
    };

    for (int z = 0; z < 256; ++z) {
        for (int x = 0; x < 256; ++x) {
            BiomeColor color = pack.biomeColors()[biomes[x + z*256]];

            get(grassTop,      x, z) = toRgba(color.grassTop);
            get(grassBottom,   x, z) = toRgba(color.grassBottom);
            get(foliageTop,    x, z) = toRgba(color.foliageTop);
            get(foliageBottom, x, z) = toRgba(color.foliageBottom);
            heights[x + z*256] = color.height;
        }
    }

    const int dx[9] = {-1, 0, 1, -1, 0, 1, -1,  0,  1};
    const int dz[9] = { 1, 1, 1,  0, 0, 0, -1, -1, -1};

    const uint8 gaussianKernel[9] = {
        16, 8, 16,
         8, 4,  8,
        16, 8, 16,
    };

    auto blurColors = [&](std::unique_ptr<uint8[]> &dst) {
        std::memset(tmp.get(), 0, 256*256*4*sizeof(uint8));

        for (int z = 0; z < 256; ++z)
            for (int x = 0; x < 256; ++x)
                for (int i = 0; i < 9; ++i)
                    get(tmp, x, z) += get(dst, clamp(x + dx[i], 0, 255), clamp(z + dz[i], 0, 255))/gaussianKernel[i];

        tmp.swap(dst);
    };
    auto makeTexture = [](uint8 *tex) {
        return std::unique_ptr<BitmapTexture>(new BitmapTexture(tex, 256, 256,
                BitmapTexture::TexelType::RGB_LDR, true, true));
    };

    blurColors(grassTop);
    blurColors(grassBottom);
    blurColors(foliageTop);
    blurColors(foliageBottom);

    _biomes.emplace_back(new BiomeTileTexture{
        makeTexture(grassTop.release()),
        makeTexture(grassBottom.release()),
        makeTexture(foliageTop.release()),
        makeTexture(foliageBottom.release()),
        std::move(heights)
    });

    _biomeMap.insert(std::make_pair(Vec2i(rx, rz), _biomes.back().get()));
}

void TraceableMinecraftMap::convertQuads(ResourcePackLoader &pack, const std::vector<TexturedQuad> &model, const Mat4f &transform)
{
    _emitterTemplates.beginModel();
    _geometry.beginModel();

    for (const TexturedQuad &quad : model) {
        int material = fetchBsdf(pack, quad);

        if (_materials[material].emitterBsdf)
            _emitterTemplates.addQuad(quad, material, transform, _materials[material].emitterOpaqueBounds);
        if (_materials[material].bsdf)
            _geometry.addQuad(quad, material, transform, _materials[material].opaqueBounds);
    }

    _emitterTemplates.endModel();
    _geometry.endModel();
}

void TraceableMinecraftMap::buildModel(ResourcePackLoader &pack, const ModelRef &model)
{
    if (!model.builtModel())
        return;

    Mat4f tform =
         Mat4f::translate(Vec3f(0.5f))
        *Mat4f::rotXYZ(Vec3f(0.0f, float(-model.yRot()), 0.0f))
        *Mat4f::rotXYZ(Vec3f(float(model.xRot()), 0.0f, 0.0f))
        *Mat4f::rotXYZ(Vec3f(0.0f, 0.0f, float(model.zRot())))
        *Mat4f::scale(Vec3f(1.0f/16.0f))
        *Mat4f::translate(Vec3f(-8.0f));
    _modelToPrimitive.insert(std::make_pair(&model, _geometry.size()));

    convertQuads(pack, *model.builtModel(), tform);
}

void TraceableMinecraftMap::buildModels(ResourcePackLoader &pack)
{
    for (const BlockDescriptor &desc : pack.blockDescriptors())
        for (const BlockVariant &var : desc.variants())
            for (const ModelRef &model : var.models())
                buildModel(pack, model);
}

int TraceableMinecraftMap::resolveLiquidBlock(ResourcePackLoader &pack, int x, int y, int z)
{
    uint32 blocks[18];
    int levels[9] = {0};
    int isAir[9] = {0};

    for (int ny = y, idx = 0; ny <= y + 1; ++ny) {
        for (int nz = z - 1; nz <= z + 1; ++nz) {
            for (int nx = x - 1; nx <= x + 1; ++nx, ++idx) {
                blocks[idx] = getBlock(nx, ny, nz);
                if (idx < 9 && blocks[idx] == 0)
                    isAir[idx] = 1;
                if (ny > y && pack.isLiquid(blocks[idx]))
                    levels[idx - 9] = 9;
                else if (pack.isLiquid(blocks[idx]))
                    levels[idx] = pack.liquidLevel(blocks[idx]);
            }
        }
    }
    bool isLava = pack.isLava(blocks[4]);
    bool hasFace[] = {
        pack.isLiquid(blocks[3]),
        pack.isLiquid(blocks[5]),
        pack.isLiquid(getBlock(x, y - 1, z)),
        pack.isLiquid(blocks[13]),
        pack.isLiquid(blocks[1]),
        pack.isLiquid(blocks[7]),
    };

    int heights[] = {
        max(levels[0], levels[1], levels[3], levels[4]),
        max(levels[1], levels[2], levels[4], levels[5]),
        max(levels[3], levels[4], levels[6], levels[7]),
        max(levels[4], levels[5], levels[7], levels[8])
    };
    int scale[] = {
        1 + isAir[0] + isAir[1] + isAir[3] + isAir[4],
        1 + isAir[1] + isAir[2] + isAir[4] + isAir[5],
        1 + isAir[3] + isAir[4] + isAir[6] + isAir[7],
        1 + isAir[4] + isAir[5] + isAir[7] + isAir[8]
    };
    for (int i = 0; i < 4; ++i)
        if (heights[i] >= 8)
            scale[i] = 1;

    uint32 key = 0;
    for (int i = 0; i < 4; ++i)
        key = (key << 4) | uint32(heights[i]);
    for (int i = 0; i < 6; ++i)
        key = (key << 1) | (hasFace[i] ? 1 : 0);
    key = (key << 1) | (isLava ? 1 : 0);

    auto iter = _liquidMap.find(key);
    if (iter != _liquidMap.end())
        return iter->second;

    static Vec3f faceVerts[6][4] = {
        {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    };
    static const int indices[6][4] = {
        {0, 2, 2, 0}, {3, 1, 1, 3}, {2, 3, 1, 0},
        {0, 1, 3, 2}, {1, 0, 0, 1}, {2, 3, 3, 2}
    };
    static const int indexToUv[4][4] = {{4, 5, 7, 8}, {3, 4, 6, 7}, {1, 2, 4, 5}, {0, 1, 3, 4}};
    CONSTEXPR float neg = 0.5f - 0.70711f;
    CONSTEXPR float pos = 0.5f + 0.70711f;
    static const Vec2f uvs[10][4] = {
        {{0.5f,  pos}, { neg, 0.5f}, {0.5f,  neg}, { pos, 0.5f}},
        {{1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}},
        {{ pos, 0.5f}, {0.5f,  pos}, { neg, 0.5f}, {0.5f,  neg}},

        {{1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}},
        {{1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}},

        {{ neg, 0.5f}, {0.5f,  neg}, { pos, 0.5f}, {0.5f,  pos}},
        {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f,  neg}, { pos, 0.5f}, {0.5f,  pos}, { neg, 0.5f}},

        {{1.0f, -1.0f}, {-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}},
    };

    std::vector<TexturedQuad> model;

    auto buildVertex = [&](int i, int t, int uvIndex, Vec3f &posDst, Vec2f &uvDst) {
        int idx = indices[i][t];
        posDst = faceVerts[i][t];
        posDst.y() *= heights[idx]/(9.0f*min(scale[idx], 4));

        Vec3f x = faceVerts[i][1] - faceVerts[i][0];
        Vec3f y = faceVerts[i][3] - faceVerts[i][0];
        float u = x.dot(posDst - faceVerts[i][0])/x.lengthSq();
        float v = y.dot(posDst - faceVerts[i][0])/y.lengthSq();
        uvDst = uvs[uvIndex][0]*(1.0f - u - v) + uvs[uvIndex][1]*u + uvs[uvIndex][3]*v;
        uvDst = uvDst*0.5f + 0.5f;
    };

    for (int i = 0; i < 6; ++i) {
        if (hasFace[i])
            continue;

        int maxDiff = 0;
        int idx = 4;
        if (i/2 == 1) {
            for (int j = 0, k = 3, l = 2; j < 4; l = k, k = j, ++j) {
                int diffS = heights[indices[i][k]] - heights[indices[i][j]];
                int diffD = heights[indices[i][l]] - heights[indices[i][j]];
                if (diffS > maxDiff) maxDiff = diffS, idx = indexToUv[indices[i][k]][indices[i][j]];
                if (diffD > maxDiff) maxDiff = diffD, idx = indexToUv[indices[i][l]][indices[i][j]];
            }
            if (idx == 4)
                idx = 9;
        }

        TexturedQuad quad;
        quad.texture = pack.liquidTexture(isLava, idx == 9);
        quad.tintIndex = -1;
        buildVertex(i, 0, idx, quad.p0, quad.uv0);
        buildVertex(i, 1, idx, quad.p1, quad.uv1);
        buildVertex(i, 2, idx, quad.p2, quad.uv2);
        buildVertex(i, 3, idx, quad.p3, quad.uv3);
        model.emplace_back(std::move(quad));
    }

    _liquidMap.insert(std::make_pair(key, _geometry.size()));

    convertQuads(pack, model, Mat4f());

    return _geometry.size() - 1;
}

void TraceableMinecraftMap::resolveBlocks(ResourcePackLoader &pack)
{
    struct DeferredBlock
    {
        ElementType &dst;
        ElementType value;
    };
    std::vector<DeferredBlock> deferredBlocks;

    for (auto &region : _regions) {
        region.second->iterateNonZeroVoxels([&](ElementType &voxel, int x, int y, int z) {
            int globalX = region.first.x()*256 + x;
            int globalZ = region.first.y()*256 + z;
            if (pack.isSpecialBlock(voxel)) {
                const ModelRef *ref = pack.mapSpecialBlock(*this, globalX, y, globalZ,
                        x + 256*y + 256*256*z, voxel);
                ElementType value = 0;
                auto iter = _modelToPrimitive.find(ref);
                if (iter != _modelToPrimitive.end())
                    value = iter->second + 1;

                deferredBlocks.push_back(DeferredBlock{voxel, value});
            } else if (pack.isLiquid(voxel)) {
                deferredBlocks.push_back(DeferredBlock{
                    voxel, ElementType(resolveLiquidBlock(pack, globalX, y, globalZ)) + 1
                });
            }
        });
    }

    for (auto &region : _regions) {
        region.second->iterateNonZeroVoxels([&](ElementType &voxel, int x, int y, int z) {
            if (!pack.isSpecialBlock(voxel) && !pack.isLiquid(voxel)) {
                const ModelRef *ref = pack.mapBlock(voxel, x + 256*y + 256*256*z);
                auto iter = _modelToPrimitive.find(ref);
                if (iter == _modelToPrimitive.end())
                    voxel = 0;
                else
                    voxel = iter->second + 1;
            }
        });
    }
    for (DeferredBlock &block : deferredBlocks)
        block.dst = block.value;

    QuadGeometry emitters;

    for (auto &region : _regions) {
        region.second->iterateNonZeroVoxels([&](ElementType &voxel, int x, int y, int z) {
            int globalX = region.first.x()*256 + x;
            int globalZ = region.first.y()*256 + z;
            if (voxel && _emitterTemplates.nonEmpty(voxel - 1))
                emitters.addQuads(_emitterTemplates, voxel - 1, Mat4f::translate(Vec3f(Vec3i(globalX, y, globalZ))));
        });
    }

    _lights = std::make_shared<MultiQuadLight>(std::move(emitters), _materials);
}

void TraceableMinecraftMap::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);

    if (auto mapPath = value["map_path"])
        _mapPath = scene.fetchResource(mapPath);

    if (auto packs = value["resource_packs"]) {
        if (packs.isArray())
            for (unsigned i = 0; i < packs.size(); ++i)
                _packPaths.emplace_back(scene.fetchResource(packs[i]));
        else
            _packPaths.emplace_back(scene.fetchResource(packs));
    }
}

rapidjson::Value TraceableMinecraftMap::toJson(Allocator &allocator) const
{
    JsonObject result{Primitive::toJson(allocator), allocator,
        "type", "minecraft_map"
    };
    if (_mapPath)
        result.add("map_path", *_mapPath);
    if (_packPaths.size() == 1) {
        result.add("resource_packs", *_packPaths[0]);
    } else if (!_packPaths.empty()) {
        rapidjson::Value a(rapidjson::kArrayType);
        for (const PathPtr &p : _packPaths)
            a.PushBack(JsonUtils::toJson(*p, allocator), allocator);
        result.add("resource_packs", std::move(a));
    }

    return result;
}

void TraceableMinecraftMap::loadResources()
{
    Bvh::PrimVector prims;

    _bounds = Box3f();

    if (!_packPaths.empty() && _mapPath) {
        try {
            std::vector<Path> packs;
            for (int i = _packPaths.size() - 1; i >= 0; --i)
                packs.push_back(*_packPaths[i]);
            packs.push_back(FileUtils::getDataPath()/"mc-loader");

            ResourcePackLoader pack(packs);
            buildModels(pack);

            MapLoader<ElementType> loader(*_mapPath);
            loader.loadRegions([&](int x, int z, int height, ElementType *data, uint8 *biomes) {
                Box3f bounds(Vec3f(x*256.0f, 0.0f, z*256.0f), Vec3f((x + 1)*256.0f, float(height), (z + 1)*256.0f));
                Vec3f centroid((x + 0.5f)*256.0f, height*0.5f, (z + 0.5f)*256.0f);

                _bounds.grow(bounds);

                prims.emplace_back(bounds, centroid, int(_grids.size()));

                buildBiomeColors(pack, x, z, biomes);

                _grids.emplace_back(new HierarchicalGrid(bounds.min(), data));
                _regions[Vec2i(x, z)] = _grids.back().get();
            });

            resolveBlocks(pack);
        } catch (const std::runtime_error &e) {
            DBG("Failed to load Minecraft map: %s", e.what());

            _bounds = Box3f();
            prims.clear();
        }
    }

    _chunkBvh.reset(new Bvh::BinaryBvh(std::move(prims), 1));
}

bool TraceableMinecraftMap::intersect(Ray &ray, IntersectionTemporary &data) const
{
    MapIntersection *isect = data.as<MapIntersection>();
    isect->wasPrimary = ray.isPrimaryRay();

    float farT = ray.farT();
    Vec3f dT = std::abs(1.0f/ray.dir());

    _chunkBvh->trace(ray, [&](Ray &ray, uint32 id, float tMin, const Vec3pf &/*bounds*/) {
        _grids[id]->trace(ray, dT, tMin, [&](uint32 idx, const Vec3f &offset, float /*t*/) {
            Vec3f oldPos = ray.pos();
            ray.setPos(oldPos - offset);

            _geometry.intersect(ray, idx, isect->isect);

            ray.setPos(oldPos);
            return ray.farT() < farT;
        });
    });

    if (ray.farT() < farT) {
        data.primitive = this;

        return true;
    }

    return false;
}

bool TraceableMinecraftMap::occluded(const Ray &ray) const
{
    IntersectionTemporary data;
    Ray temp(ray);
    return intersect(temp, data);
}

bool TraceableMinecraftMap::hitBackside(const IntersectionTemporary &/*data*/) const
{
    return false;
}

void TraceableMinecraftMap::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    const MapIntersection *isect = data.as<MapIntersection>();

    info.Ng = info.Ns = _geometry.normal(isect->isect);
    info.uv = _geometry.uv(isect->isect);
    info.bsdf = _materials[_geometry.material(isect->isect)].bsdf.get();
    info.primitive = this;
}

bool TraceableMinecraftMap::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
        Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool TraceableMinecraftMap::isSamplable() const
{
    return false;
}

void TraceableMinecraftMap::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

bool TraceableMinecraftMap::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool TraceableMinecraftMap::isDirac() const
{
    return false;
}

bool TraceableMinecraftMap::isInfinite() const
{
    return false;
}

float TraceableMinecraftMap::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    return -1.0f;
}

Box3f TraceableMinecraftMap::bounds() const
{
    return _bounds;
}

const TriangleMesh &TraceableMinecraftMap::asTriangleMesh()
{
    if (!_proxy) {
        _proxy.reset(new TriangleMesh());
        _proxy->makeCube();
    }
    return *_proxy;
}

int TraceableMinecraftMap::numBsdfs() const
{
    return _materials.size();
}

std::shared_ptr<Bsdf> &TraceableMinecraftMap::bsdf(int index)
{
    return _materials[index].bsdf;
}

void TraceableMinecraftMap::setBsdf(int index, std::shared_ptr<Bsdf> &bsdf)
{
    _materials[index].bsdf = bsdf;
}

void TraceableMinecraftMap::prepareForRender()
{

}

void TraceableMinecraftMap::teardownAfterRender()
{
    /*_grids.clear();
    _chunkBvh.reset();

    for (auto &m : _models)
        m->teardownAfterRender();*/
}

Primitive *TraceableMinecraftMap::clone()
{
    return new TraceableMinecraftMap(*this);
}

std::vector<std::shared_ptr<Primitive>> TraceableMinecraftMap::createHelperPrimitives()
{
    if (_lights)
        return std::vector<std::shared_ptr<Primitive>>(1, _lights);
    else
        return std::vector<std::shared_ptr<Primitive>>();
}

}
}

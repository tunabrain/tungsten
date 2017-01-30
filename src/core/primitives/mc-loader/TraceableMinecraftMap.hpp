#ifndef TRACEABLEMAP_HPP_
#define TRACEABLEMAP_HPP_

#include "MultiQuadLight.hpp"
#include "QuadMaterial.hpp"
#include "QuadGeometry.hpp"

#include "primitives/VoxelHierarchy.hpp"
#include "primitives/Primitive.hpp"

#include "textures/BitmapTexture.hpp"

#include "bsdfs/Bsdf.hpp"

#include "bvh/BinaryBvh.hpp"

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

namespace Tungsten {

class Scene;

namespace MinecraftLoader {

class ResourcePackLoader;
struct TexturedQuad;
class ModelRef;

struct BiomeTileTexture
{
    std::unique_ptr<BitmapTexture> foliageTop, foliageBottom;
    std::unique_ptr<BitmapTexture> grassTop, grassBottom;
    std::unique_ptr<float[]> heights;
};

class TraceableMinecraftMap : public Primitive
{
    typedef uint32 ElementType;
    typedef VoxelHierarchy<2, 4, ElementType> HierarchicalGrid;
    template<typename T> using aligned_vector = std::vector<T, AlignedAllocator<T, 4096>>;

    PathPtr _mapPath;
    std::vector<PathPtr> _packPaths;

    std::shared_ptr<Bsdf> _missingBsdf;
    std::vector<QuadMaterial> _materials;
    std::unordered_map<std::string, int> _bsdfCache;
    std::unordered_map<const ModelRef *, int> _modelToPrimitive;
    std::unordered_map<uint32, int> _liquidMap;

    QuadGeometry _geometry;
    QuadGeometry _emitterTemplates;

    Box3f _bounds;
    std::unique_ptr<TriangleMesh> _proxy;
    std::vector<std::unique_ptr<HierarchicalGrid>> _grids;
    std::unordered_map<Vec2i, HierarchicalGrid *> _regions;

    std::vector<std::unique_ptr<BiomeTileTexture>> _biomes;
    std::unordered_map<Vec2i, const BiomeTileTexture *> _biomeMap;
    std::unique_ptr<Bvh::BinaryBvh> _chunkBvh;
    std::shared_ptr<MultiQuadLight> _lights;

    void getTexProperties(const Path &path, int w, int h, int &tileW, int &tileH,
            bool &clamp, bool &linear);

    void loadTexture(ResourcePackLoader &pack, const std::string &name, std::shared_ptr<BitmapTexture> &albedo,
            std::shared_ptr<BitmapTexture> &opacity, Box2f &opaqueBounds, Vec4c tint, const uint8 *mask,
            int maskW, int maskH);
    void loadMaskedBsdf(std::shared_ptr<Bsdf> &bsdf, Box2f &opaqueBounds, std::shared_ptr<BitmapTexture> &emission,
            ResourcePackLoader &pack, const TexturedQuad &quad, Vec4c filter, bool emissive,
            const uint8 *mask, int maskW, int maskH);
    int fetchBsdf(ResourcePackLoader &pack, const TexturedQuad &quad);

    void buildBiomeColors(ResourcePackLoader &pack, int x, int z, uint8 *biomes);

    void convertQuads(ResourcePackLoader &pack, const std::vector<TexturedQuad> &model, const Mat4f &transform);
    void buildModel(ResourcePackLoader &pack, const ModelRef &model);
    void buildModels(ResourcePackLoader &pack);

    int resolveLiquidBlock(ResourcePackLoader &pack, int x, int y, int z);

    void resolveBlocks(ResourcePackLoader &pack);

public:
    TraceableMinecraftMap();

    TraceableMinecraftMap(const TraceableMinecraftMap &o);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable(const TraceableScene &scene, uint32 threadIndex) override;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDirac() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(uint32 threadIndex, const Vec3f &p) const override;

    virtual Box3f bounds() const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;
    virtual void setBsdf(int index, std::shared_ptr<Bsdf> &bsdf) override;

    virtual void prepareForRender() override;
    virtual void teardownAfterRender() override;

    virtual Primitive *clone() override;

    virtual std::vector<std::shared_ptr<Primitive>> createHelperPrimitives() override;

    inline uint32 getBlock(int x, int y, int z) const
    {
        if (y < 0 || y >= 256)
            return 0;

        // Deal with round-to-zero division
        int cx = x < 0 ? -((-x - 1)/256 + 1) : x/256;
        int cz = z < 0 ? -((-z - 1)/256 + 1) : z/256;
        int rx = x < 0 ? ((256 - ((-x) % 256)) % 256) : (x % 256);
        int rz = z < 0 ? ((256 - ((-z) % 256)) % 256) : (z % 256);

        auto iter = _regions.find(Vec2i(cx, cz));
        if (iter == _regions.end())
            return 0;

        ElementType *ref = iter->second->at(rx, y, rz);
        return ref ? *ref : 0;
    }
};

}
}

#endif /* TRACEABLEMAP_HPP_ */

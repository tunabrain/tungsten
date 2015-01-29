#ifndef TRACEABLEMAP_HPP_
#define TRACEABLEMAP_HPP_

#include "VoxelHierarchy.hpp"
#include "Primitive.hpp"

#include "materials/BitmapTexture.hpp"

#include "bsdfs/Bsdf.hpp"

#include "bvh/BinaryBvh.hpp"

#include <unordered_map>
#include <memory>
#include <string>

namespace Tungsten {

class ResourcePackLoader;
class TexturedQuad;
class ModelRef;
class Scene;

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

    std::string _mapPath;
    std::string _packPath;

    std::shared_ptr<Bsdf> _missingBsdf;
    std::unordered_map<std::string, std::shared_ptr<Bsdf>> _bsdfCache;
    std::unordered_map<const ModelRef *, int> _modelToPrimitive;
    std::vector<std::shared_ptr<Primitive>> _models;

    Box3f _bounds;
    std::unique_ptr<TriangleMesh> _proxy;
    std::vector<std::unique_ptr<HierarchicalGrid>> _grids;
    std::unordered_map<Vec2i, HierarchicalGrid *> _regions;

    std::vector<std::unique_ptr<BiomeTileTexture>> _biomes;
    std::unordered_map<Vec2i, const BiomeTileTexture *> _biomeMap;
    std::unique_ptr<Bvh::BinaryBvh> _chunkBvh;

    void getTexProperties(const std::string &path, int w, int h, int &tileW, int &tileH,
            bool &clamp, bool &linear);

    void loadTexture(ResourcePackLoader &pack, const std::string &path,
            std::shared_ptr<BitmapTexture> &albedo, std::shared_ptr<BitmapTexture> &opacity);
    std::shared_ptr<Bsdf> fetchBsdf(ResourcePackLoader &pack, const TexturedQuad &quad);

    void buildBiomeColors(ResourcePackLoader &pack, int x, int z, uint8 *biomes);

    void buildModel(ResourcePackLoader &pack, const ModelRef &model);
    void buildModels(ResourcePackLoader &pack);

public:
    TraceableMinecraftMap();

    TraceableMinecraftMap(const TraceableMinecraftMap &o);

    void saveTestScene();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable() override;

    virtual float inboundPdf(const IntersectionTemporary &data, const IntersectionInfo &info,
            const Vec3f &p, const Vec3f &d) const override;
    virtual bool sampleInboundDirection(LightSample &sample) const override;
    virtual bool sampleOutboundDirection(LightSample &sample) const override;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDelta() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(const Vec3f &p) const override;

    virtual Box3f bounds() const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;

    virtual void prepareForRender() override;
    virtual void cleanupAfterRender() override;

    virtual Primitive *clone() override;

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

#endif /* TRACEABLEMAP_HPP_ */

#ifndef MULTIQUADLIGHT_HPP_
#define MULTIQUADLIGHT_HPP_

#include "QuadMaterial.hpp"
#include "QuadGeometry.hpp"
#include "Primitive.hpp"

#include "bvh/BinaryBvh.hpp"

#include <vector>
#include <memory>

namespace Tungsten {

class BitmapTexture;

struct EmissiveQuad
{
    Vec3f p0, p1, p2, p3;
    Vec3f emission;
};

class MultiQuadLight : public Primitive
{
    struct ThreadlocalSampleInfo
    {
        std::vector<float> sampleWeights;
        Vec3f lastQuery;
    };

    QuadGeometry _geometry;
    const std::vector<QuadMaterial> &_materials;

    Box3f _bounds;
    std::vector<std::unique_ptr<ThreadlocalSampleInfo>> _samplers;
    std::unique_ptr<Bvh::BinaryBvh> _bvh;
    std::unique_ptr<TriangleMesh> _proxy;


public:
    MultiQuadLight(QuadGeometry geometry, const std::vector<QuadMaterial> &materials);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable(uint32 threadIndex) override;

    virtual float inboundPdf(uint32 threadIndex, const IntersectionTemporary &data,
            const IntersectionInfo &info, const Vec3f &p, const Vec3f &d) const override;
    virtual bool sampleInboundDirection(uint32 threadIndex, LightSample &sample) const override;
    virtual bool sampleOutboundDirection(uint32 threadIndex, LightSample &sample) const override;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDelta() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(uint32 threadIndex, const Vec3f &p) const override;

    virtual Box3f bounds() const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;

    virtual void prepareForRender() override;
    virtual void cleanupAfterRender() override;

    virtual Primitive *clone() override;

    virtual bool isEmissive() const override;
    virtual Vec3f emission(const IntersectionTemporary &data, const IntersectionInfo &info) const override;
};

}



#endif /* MULTIQUADLIGHT_HPP_ */

#ifndef INSTANCE_HPP_
#define INSTANCE_HPP_

#include "Primitive.hpp"

#include "math/Quaternion.hpp"

#include "bvh/BinaryBvh.hpp"

namespace Tungsten {

class Instance : public Primitive
{
    std::vector<std::shared_ptr<Primitive>> _master;

    PathPtr _instanceFileA;
    PathPtr _instanceFileB;
    float _ratio;

    uint32 _instanceCount;
    std::unique_ptr<Vec3f[]> _instancePos;
    std::unique_ptr<QuaternionF[]> _instanceRot;
    std::unique_ptr<uint8[]> _instanceId;

    Box3f _bounds;

    std::shared_ptr<TriangleMesh> _proxy;

    std::unique_ptr<Bvh::BinaryBvh> _bvh;

    void buildProxy();

    const Primitive &getMaster(const IntersectionTemporary &data) const;

protected:
    virtual float powerToRadianceFactor() const override;

public:
    Instance();

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;
    virtual void saveResources() override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info, Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable(const TraceableScene &scene, uint32 threadIndex) override;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDirac() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(uint32 threadIndex, const Vec3f &p) const override;
    virtual Box3f bounds() const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual void prepareForRender() override;
    virtual void teardownAfterRender() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;
    virtual void setBsdf(int index, std::shared_ptr<Bsdf> &bsdf) override;

    virtual Primitive *clone() override;
};

}

#endif /* INSTANCE_HPP_ */

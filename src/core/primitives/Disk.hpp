#ifndef DISK_HPP_
#define DISK_HPP_

#include "TriangleMesh.hpp"
#include "Primitive.hpp"

namespace Tungsten {

class Disk : public Primitive
{
    float _coneAngle;

    Vec3f _center;
    float _r;
    Vec3f _n;
    float _area;
    float _invArea;
    TangentFrame _frame;
    float _cosApex;
    Vec3f _coneBase;

    std::shared_ptr<Bsdf> _bsdf;
    std::shared_ptr<TriangleMesh> _proxy;

    void buildProxy();

protected:
    virtual float powerToRadianceFactor() const override;

public:
    Disk();
    Disk(const Vec3f &pos, const Vec3f &n, float r, const std::string &name, std::shared_ptr<Bsdf> bsdf);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable(const TraceableScene &scene, uint32 threadIndex) override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const override;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, DirectionSample &sample) const override;
    virtual bool sampleDirect(uint32 threadIndex, const Vec3f &p, PathSampleGenerator &sampler, LightSample &sample) const override;
    bool invertPosition(WritablePathSampleGenerator &sampler, const PositionSample &point) const;
    bool invertDirection(WritablePathSampleGenerator &sampler, const PositionSample &/*point*/, const DirectionSample &direction) const;
    virtual float positionalPdf(const PositionSample &point) const override;
    virtual float directionalPdf(const PositionSample &point, const DirectionSample &sample) const override;
    virtual float directPdf(uint32 threadIndex, const IntersectionTemporary &data,
            const IntersectionInfo &info, const Vec3f &p) const override;
    virtual Vec3f evalPositionalEmission(const PositionSample &sample) const override;
    virtual Vec3f evalDirectionalEmission(const PositionSample &point, const DirectionSample &sample) const override;
    virtual Vec3f evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const override;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDirac() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(uint32 threadIndex, const Vec3f &p) const override;
    virtual Box3f bounds() const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual void prepareForRender() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;
    virtual void setBsdf(int index, std::shared_ptr<Bsdf> &bsdf) override;

    virtual Primitive *clone() override;
};

}

#endif /* DISK_HPP_ */

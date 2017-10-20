#ifndef TRIANGLEMESH_HPP_
#define TRIANGLEMESH_HPP_

#include "Primitive.hpp"
#include "Triangle.hpp"
#include "Vertex.hpp"

#include "sampling/Distribution1D.hpp"

#include "io/Path.hpp"
#include <memory>
#include <vector>
#include <string>

#include <embree2/rtcore.h>
#include <embree2/rtcore_scene.h>
#include <embree2/rtcore_geometry.h>

namespace Tungsten {

class Scene;

class TriangleMesh : public Primitive
{
    PathPtr _path;
    bool _smoothed;
    bool _backfaceCulling;
    bool _recomputeNormals;

    std::vector<Vertex> _verts;
    std::vector<Vertex> _tfVerts;
    std::vector<TriangleI> _tris;

    std::vector<std::shared_ptr<Bsdf>> _bsdfs;

    std::unique_ptr<Distribution1D> _triSampler;
    float _totalArea;
    float _invArea;

    Box3f _bounds;

    RTCScene _scene;
    unsigned _geomId;

    Vec3f unnormalizedGeometricNormalAt(int triangle) const;
    Vec3f normalAt(int triangle, float u, float v) const;
    Vec2f uvAt(int triangle, float u, float v) const;

protected:
    virtual float powerToRadianceFactor() const override;

public:
    TriangleMesh();
    TriangleMesh(const TriangleMesh &o);
    TriangleMesh(std::vector<Vertex> verts, std::vector<TriangleI> tris,
                 const std::shared_ptr<Bsdf> &bsdf,
                 const std::string &name, bool smoothed, bool backfaceCull);
    TriangleMesh(std::vector<Vertex> verts, std::vector<TriangleI> tris,
                 std::vector<std::shared_ptr<Bsdf>> bsdf,
                 const std::string &name, bool smoothed, bool backfaceCull);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;
    virtual void saveResources() override;
    void saveAs(const Path &path) const;

    void calcSmoothVertexNormals();
    void computeBounds();

    void makeCube();
    void makeSphere(float radius);
    void makeCylinder(float radius, float height);
    void makeCone(float radius, float height);

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &/*info*/,
            Vec3f &T, Vec3f &B) const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable(const TraceableScene &scene, uint32 threadIndex) override;

    virtual bool samplePosition(PathSampleGenerator &sampler, PositionSample &sample) const override final;
    virtual bool sampleDirection(PathSampleGenerator &sampler, const PositionSample &point,
            DirectionSample &sample) const override final;
    virtual bool sampleDirect(uint32 threadIndex, const Vec3f &p, PathSampleGenerator &sampler,
            LightSample &sample) const override;
    virtual float positionalPdf(const PositionSample &point) const override;
    virtual float directionalPdf(const PositionSample &point, const DirectionSample &sample) const override;
    virtual float directPdf(uint32 threadIndex, const IntersectionTemporary &data,
            const IntersectionInfo &info, const Vec3f &p) const override;
    virtual Vec3f evalPositionalEmission(const PositionSample &sample) const override;
    virtual Vec3f evalDirectionalEmission(const PositionSample &point,
            const DirectionSample &sample) const override;
    virtual Vec3f evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const override;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const override;

    virtual bool isDirac() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(uint32 threadIndex, const Vec3f &p) const override;
    virtual Box3f bounds() const override;

    virtual void prepareForRender() override;
    virtual void teardownAfterRender() override;

    virtual int numBsdfs() const override;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) override;
    virtual void setBsdf(int index, std::shared_ptr<Bsdf> &bsdf) override;

    virtual Primitive *clone() override;

    const std::vector<TriangleI>& tris() const
    {
        return _tris;
    }

    const std::vector<Vertex>& verts() const
    {
        return _verts;
    }

    std::vector<TriangleI>& tris()
    {
        return _tris;
    }

    std::vector<Vertex>& verts()
    {
        return _verts;
    }

    bool smoothed() const
    {
        return _smoothed;
    }

    void setSmoothed(bool v)
    {
        _smoothed = v;
    }

    const PathPtr& path() const
    {
        return _path;
    }
};

}

#endif /* TRIANGLEMESH_HPP_ */

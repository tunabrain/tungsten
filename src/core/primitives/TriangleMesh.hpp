#ifndef TRIANGLEMESH_HPP_
#define TRIANGLEMESH_HPP_

#include "Primitive.hpp"
#include "Triangle.hpp"
#include "Vertex.hpp"

#include "sampling/Distribution1D.hpp"

#include <embree/include/embree.h>
#include <memory>
#include <vector>
#include <string>

namespace Tungsten {

class Scene;

class TriangleMesh : public Primitive
{
    std::string _path;
    bool _smoothed;

    std::vector<Vertex> _verts;
    std::vector<Vertex> _tfVerts;
    std::vector<TriangleI> _tris;

    std::unique_ptr<Distribution1D> _triSampler;
    float _totalArea;

    Box3f _bounds;

    embree::RTCGeometry *_geom = nullptr;
    embree::RTCIntersector1 *_intersector = nullptr;

    Vec3f unnormalizedGeometricNormalAt(int triangle) const;
    Vec3f normalAt(int triangle, float u, float v) const;
    Vec2f uvAt(int triangle, float u, float v) const;

public:
    TriangleMesh();
    TriangleMesh(const TriangleMesh &o);
    TriangleMesh(std::vector<Vertex> verts, std::vector<TriangleI> tris,
                 const std::shared_ptr<Bsdf> &bsdf,
                 const std::string &name, bool smoothed);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void saveData() override;
    void saveAsObj(const std::string &path) const;
    void calcSmoothVertexNormals();
    void computeBounds();

    void makeCube();
    void makeSphere(float radius);
    void makeCone(float radius, float height);

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const override;
    virtual bool occluded(const Ray &ray) const override;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const override;
    virtual bool hitBackside(const IntersectionTemporary &data) const override;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &/*info*/,
            Vec3f &T, Vec3f &B) const override;

    virtual const TriangleMesh &asTriangleMesh() override;

    virtual bool isSamplable() const override;
    virtual void makeSamplable() override;

    virtual float inboundPdf(const IntersectionTemporary &data, const Vec3f &p, const Vec3f &d) const override;
    virtual bool sampleInboundDirection(LightSample &sample) const override;
    virtual bool sampleOutboundDirection(LightSample &sample) const override;
    virtual bool invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const override;

    virtual bool isDelta() const override;
    virtual bool isInfinite() const override;

    virtual float approximateRadiance(const Vec3f &/*p*/) const override;
    virtual Box3f bounds() const override;

    virtual void prepareForRender() override;
    virtual void cleanupAfterRender() override;

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

    const std::string& path() const
    {
        return _path;
    }
};

}

#endif /* TRIANGLEMESH_HPP_ */

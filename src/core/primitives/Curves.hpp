#ifndef CURVES_HPP_
#define CURVES_HPP_

#include "Primitive.hpp"

#include "bvh/BinaryBvh.hpp"

#include <memory>
#include <vector>

namespace Tungsten
{

class Curves : public Primitive
{
    struct CurveIntersection
    {
        uint32 curveP0;
        float t;
        Vec2f uv;
        float w;
    };

    std::string _path;

    uint32 _curveCount;
    uint32 _nodeCount;

    std::vector<uint32> _curveEnds;
    std::vector<Vec4f> _nodeData;
    std::vector<Vec3f> _nodeColor;

    std::shared_ptr<TriangleMesh> _proxy;

    Box3f _bounds;

    std::unique_ptr<BinaryBvh> _bvh;

    void loadCurves();
    void computeBounds();

public:
    virtual ~Curves() {}

    Curves() = default;

    Curves(const Curves &o)
    : Primitive(o)
    {
        _path       = o._path;
        _curveCount = o._curveCount;
        _nodeCount  = o._nodeCount;
        _curveEnds  = o._curveEnds;
        _nodeData   = o._nodeData;
        _nodeColor  = o._nodeColor;
        _proxy      = o._proxy;
        _bounds     = o._bounds;
    }

    void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const;

    virtual bool occluded(const Ray &ray) const
    {
        IntersectionTemporary tmp;
        Ray r(ray);
        return intersect(r, tmp);
    }

    virtual bool hitBackside(const IntersectionTemporary &/*data*/) const
    {
        return false;
    }

    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const;

    virtual bool tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/,
            Vec3f &/*T*/, Vec3f &/*B*/) const
    {
        return false;
    }

    virtual bool isSamplable() const
    {
        return false;
    }
    virtual void makeSamplable()
    {
    }

    virtual float inboundPdf(const IntersectionTemporary &/*data*/, const Vec3f &/*p*/, const Vec3f &/*d*/) const
    {
        return 0.0f;
    }
    virtual bool sampleInboundDirection(LightSample &/*sample*/) const
    {
        return false;
    }
    virtual bool sampleOutboundDirection(LightSample &/*sample*/) const
    {
        return false;
    }
    virtual bool invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
    {
        return false;
    }

    virtual bool isDelta() const
    {
        return false;
    }

    virtual bool isInfinite() const
    {
        return false;
    }

    virtual float approximateRadiance(const Vec3f &/*p*/) const
    {
        return -1.0f;
    }

    virtual Box3f bounds() const
    {
        return _bounds;
    }

    void buildProxy();

    virtual const TriangleMesh &asTriangleMesh()
    {
        if (!_proxy)
            buildProxy();
        return *_proxy;
    }

    virtual void prepareForRender();
    virtual void cleanupAfterRender();

    virtual Primitive *clone()
    {
        return new Curves(*this);
    }
};

}

#endif /* CURVES_HPP_ */

#ifndef INFINITESPHERECAP_HPP_
#define INFINITESPHERECAP_HPP_

#include "Primitive.hpp"
#include "Mesh.hpp"

namespace Tungsten {

class InfiniteSphereCap : public Primitive
{
    struct InfiniteSphereCapIntersection
    {
        Vec3f p;
        Vec3f w;
    };

    bool _doSample;
    float _capAngleDeg;

    Vec3f _capDir;
    TangentFrame _capFrame;
    float _capAngleRad;
    float _cosCapAngle;

    std::shared_ptr<TriangleMesh> _proxy;

public:
    InfiniteSphereCap()
    : _doSample(true),
      _capAngleDeg(10.0f)
    {
    }

    void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Primitive::fromJson(v, scene);
        JsonUtils::fromJson(v, "sample", _doSample);
        JsonUtils::fromJson(v, "cap_angle", _capAngleDeg);
    }
    rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Primitive::toJson(allocator);
        v.AddMember("type", "infinite_sphere_cap", allocator);
        v.AddMember("sample", _doSample, allocator);
        v.AddMember("cap_angle", _capAngleDeg, allocator);
        return std::move(v);
    }

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const
    {
        if (ray.dir().dot(_capDir) < _cosCapAngle)
            return false;

        InfiniteSphereCapIntersection *isect = data.as<InfiniteSphereCapIntersection>();
        isect->p = ray.pos();
        isect->w = ray.dir();
        data.primitive = this;
        return true;
    }

    virtual bool occluded(const Ray &ray) const
    {
        return ray.dir().dot(_capDir) >= _cosCapAngle;
    }

    virtual bool hitBackside(const IntersectionTemporary &/*data*/) const
    {
        return false;
    }

    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
    {
        const InfiniteSphereCapIntersection *isect = data.as<InfiniteSphereCapIntersection>();
        info.Ng = info.Ns = -isect->w;
        info.p = isect->p;
        info.uv = Vec2f(0.0f, 0.0f);
        info.primitive = this;
    }

    virtual bool tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/, Vec3f &/*T*/, Vec3f &/*B*/) const
    {
        return false;
    }

    virtual bool isSamplable() const
    {
        return _doSample;
    }

    virtual void makeSamplable()
    {
    }

    virtual float inboundPdf(const IntersectionTemporary &/*data*/, const Vec3f &/*p*/, const Vec3f &/*d*/) const
    {
        return Sample::uniformSphericalCapPdf(_cosCapAngle);
    }

    virtual bool sampleInboundDirection(LightSample &sample) const
    {
        Vec3f dir = Sample::uniformSphericalCap(sample.sampler->next2D(), _cosCapAngle);
        sample.d = _capFrame.toGlobal(dir);
        sample.dist = 1e30f;
        sample.pdf = Sample::uniformSphericalCapPdf(_cosCapAngle);
        return true;
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
        return true;
    }

    virtual float approximateRadiance(const Vec3f &/*p*/) const override final
    {
        if (!isEmissive() || !isSamplable())
            return 0.0f;
        return TWO_PI*(1.0f - _cosCapAngle)*_emission->average().max();
    }

    virtual Box3f bounds() const
    {
        return Box3f(Vec3f(-1e30f), Vec3f(1e30f));
    }

    void buildProxy()
    {
        _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Sphere", false);
        _proxy->makeCone(0.05f, 1.0f);
    }

    virtual const TriangleMesh &asTriangleMesh()
    {
        if (!_proxy)
            buildProxy();
        return *_proxy;
    }

    virtual void prepareForRender()
    {
        _capDir = _transform.transformVector(Vec3f(0.0f, 1.0f, 0.0f)).normalized();
        _capAngleRad = Angle::degToRad(_capAngleDeg);
        _cosCapAngle = std::cos(_capAngleRad);
        _capFrame = TangentFrame(_capDir);
    }

    virtual void cleanupAfterRender()
    {
    }

    virtual Primitive *clone()
    {
        return new InfiniteSphereCap(*this);
    }
};

}


#endif /* INFINITESPHERE_HPP_ */

#ifndef SPOTLIGHT_HPP_
#define SPOTLIGHT_HPP_

#include "Primitive.hpp"
#include "Mesh.hpp"

#include "sampling/Sample.hpp"

namespace Tungsten
{

class Spotlight : public Primitive
{
    struct SpotIntersection
    {
        Vec3f p;
        float rSq;
        bool backSide;
    };

    float _angle;
    bool _disableReflection;

    Vec3f _center;
    float _r;
    Vec3f _n;
    TangentFrame _frame;
    float _cosApex;
    Vec3f _coneBase;

    std::shared_ptr<TriangleMesh> _proxy;

public:
    Spotlight()
    : _angle(45.0f),
      _disableReflection(false)
    {
    }

    void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Primitive::fromJson(v, scene);
        JsonUtils::fromJson(v, "angle", _angle);
        JsonUtils::fromJson(v, "disable_reflection", _disableReflection);
    }

    rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Primitive::toJson(allocator);
        v.AddMember("type", "spot", allocator);
        v.AddMember("angle", _angle, allocator);
        v.AddMember("disable_reflection", _disableReflection, allocator);
        return std::move(v);
    }

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const
    {
        float nDotW = ray.dir().dot(_n);
        if (nDotW >= 0.0f)
            return false;

        float t = _n.dot(_center - ray.pos())/nDotW;
        if (t < ray.nearT() || t > ray.farT())
            return false;

        Vec3f q = ray.pos() + t*ray.dir();
        Vec3f v = q - _center;
        float rSq = v.lengthSq();
        if (rSq > _r*_r)
            return false;

        ray.setFarT(t);
        SpotIntersection *isect = data.as<SpotIntersection>();
        isect->p = q;
        isect->rSq = rSq;
        isect->backSide = -nDotW < _cosApex;
        data.primitive = this;

        return true;
    }

    virtual bool occluded(const Ray &ray) const
    {
        float nDotW = ray.dir().dot(_n);
        if (nDotW >= 0.0f)
            return false;

        float t = _n.dot(_center - ray.pos())/nDotW;
        if (t < ray.nearT() || t > ray.farT())
            return false;

        Vec3f q = ray.pos() + t*ray.dir();
        Vec3f v = q - _center;
        float rSq = v.lengthSq();
        if (rSq > _r*_r)
            return false;

        return true;
    }

    virtual bool hitBackside(const IntersectionTemporary &data) const
    {
        return data.as<SpotIntersection>()->backSide;
    }

    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
    {
        const SpotIntersection *isect = data.as<SpotIntersection>();
        info.Ng = info.Ns = _n;

        info.p = isect->p;
        Vec3f d = isect->p - _center;
        float x = d.dot(_frame.bitangent);
        float y = d.dot(_frame.tangent);
        float v = std::sqrt(isect->rSq)/_r;
        float u = (x == 0.0f && y == 0.0f) ? 0.0f : (std::atan2(y, x)*INV_TWO_PI + 0.5f);
        info.uv = Vec2f(u, v);

        info.primitive = this;
    }

    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &/*info*/, Vec3f &T, Vec3f &B) const
    {
        const SpotIntersection *isect = data.as<SpotIntersection>();
        Vec3f d = isect->p - _center;
        if (d.lengthSq() == 0.0f)
            return false;
        d.normalize();

        T = _n.cross(d);
        B = d;
        return true;
    }

    virtual bool isSamplable() const
    {
        return true;
    }

    virtual void makeSamplable()
    {
    }

    virtual float inboundPdf(const IntersectionTemporary &/*data*/, const Vec3f &p, const Vec3f &d) const
    {
        float cosTheta = std::abs(_n.dot(d));
        float t = _n.dot(_center - p)/_n.dot(d);

        return t*t/(cosTheta*_r*_r*PI);
    }

    virtual bool sampleInboundDirection(LightSample &sample) const
    {
        if (_n.dot(sample.p - _center) < 0.0f)
            return false;

        Vec2f lQ = Sample::uniformDisk(sample.sampler->next2D()).xy()*_r;
        Vec3f q = _center + lQ.x()*_frame.bitangent + lQ.y()*_frame.tangent;
        sample.d = q - sample.p;
        float rSq = sample.d.lengthSq();
        sample.dist = std::sqrt(rSq);
        sample.d /= sample.dist;
        if (-sample.d.dot(_n) < _cosApex)
            return false;
        float cosTheta = -_n.dot(sample.d);
        sample.pdf = rSq/(cosTheta*_r*_r*PI);
        return true;
    }

    virtual bool sampleOutboundDirection(LightSample &sample) const
    {
        Vec2f lQ = Sample::uniformDisk(sample.sampler->next2D()).xy();
        sample.p = _center + lQ.x()*_frame.bitangent + lQ.y()*_frame.tangent;
        sample.d = Sample::cosineHemisphere(sample.sampler->next2D());
        sample.pdf = Sample::cosineHemispherePdf(sample.d)/(_r*_r*PI);
        TangentFrame frame(_n);
        sample.d = frame.toGlobal(sample.d);
        return true;
    }

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const
    {
        float phi = (uv.x() - 0.5f)*TWO_PI;
        float r = uv.y()*_r;
        pos = _center + std::cos(phi)*r*_frame.bitangent + std::sin(phi)*r*_frame.tangent;
        return true;
    }

    virtual bool disableReflectedEmission() const override final
    {
        return _disableReflection;
    }

    virtual bool isDelta() const
    {
        return false;
    }

    virtual bool isInfinite() const
    {
        return false;
    }

    virtual float approximateRadiance(const Vec3f &p) const override final
    {
        if (!isEmissive())
            return 0.0f;
        Vec3f coneD = p - _coneBase;
        if (coneD.dot(_n)/coneD.length() < _cosApex)
            return 0.0f;

        Vec3f d = _center - p;

        Vec3f edge0 = _frame.tangent*_r;
        Vec3f edge1 = _frame.bitangent*_r;

        Vec3f R0 = d - edge0 - edge1;
        Vec3f R1 = R0 + 2.0f*edge0;
        Vec3f R2 = R1 + 2.0f*edge1;
        Vec3f R3 = R0 + 2.0f*edge1;
        Vec3f n0 = R0.cross(R1).normalized();
        Vec3f n1 = R1.cross(R2).normalized();
        Vec3f n2 = R2.cross(R3).normalized();
        Vec3f n3 = R3.cross(R0).normalized();
        float Q =
              std::acos(n0.dot(n1))
            + std::acos(n1.dot(n2))
            + std::acos(n2.dot(n3))
            + std::acos(n3.dot(n0));

        return (TWO_PI - std::abs(Q))*_emission->average().max();
    }

    virtual Box3f bounds() const
    {
        Box3f result;
        result.grow(_center - _frame.tangent*_r - _frame.bitangent*_r);
        result.grow(_center + _frame.tangent*_r - _frame.bitangent*_r);
        result.grow(_center + _frame.tangent*_r + _frame.bitangent*_r);
        result.grow(_center - _frame.tangent*_r + _frame.bitangent*_r);
        return result;
    }

    void buildProxy()
    {
        _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Cone", false);
        _proxy->makeCone(1.0f, 0.01f);
    }

    virtual const TriangleMesh &asTriangleMesh()
    {
        if (!_proxy)
            buildProxy();
        return *_proxy;
    }

    virtual void prepareForRender() final override
    {
        _center = _transform*Vec3f(0.0f);
        _r = (_transform.extractScale()*Vec3f(1.0f, 0.0f, 1.0f)).max();
        _n = _transform.transformVector(Vec3f(0.0f, -1.0f, 0.0f)).normalized();
        _frame = TangentFrame(_n);
        _cosApex = std::cos(Angle::degToRad(_angle));
        _coneBase = _center - _n/std::sin(Angle::degToRad(_angle));
    }

    virtual void cleanupAfterRender()
    {
    }

    float area() const override
    {
        return _r*_r*PI;
    }

    virtual Primitive *clone()
    {
        return new Spotlight(*this);
    }
};

}

#endif /* SPOTLIGHT_HPP_ */

#ifndef SPHERE_HPP_
#define SPHERE_HPP_

#include "Primitive.hpp"
#include "Mesh.hpp"

#include "sampling/Sample.hpp"

namespace Tungsten {

class Sphere : public Primitive
{
    struct SphereIntersection
    {
        bool backSide;
    };

    Mat4f _rot;
    Mat4f _invRot;
    Vec3f _pos;
    float _radius;

    std::shared_ptr<TriangleMesh> _proxy;
public:

    Sphere()
    : _pos(0.0f),
      _radius(1.0f)
    {
    }

    Sphere(const Vec3f &pos, float r, const std::string &name, std::shared_ptr<Bsdf> bsdf)
    : Primitive(name, bsdf),
      _pos(pos),
      _radius(r)
    {
        _transform = Mat4f::translate(_pos)*Mat4f::scale(Vec3f(_radius));
    }

    void fromJson(const rapidjson::Value &v, const Scene &scene) override
    {
        Primitive::fromJson(v, scene);
    }

    rapidjson::Value toJson(Allocator &allocator) const override
    {
        rapidjson::Value v = Primitive::toJson(allocator);
        v.AddMember("type", "sphere", allocator);
        return std::move(v);
    }

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const
    {
        Vec3f p = ray.pos() - _pos;
        float B = p.dot(ray.dir());
        float C = p.lengthSq() - _radius*_radius;
        float detSq = B*B - C;
        if (detSq >= 0.0f) {
            float det = std::sqrt(detSq);
            float t = -B - det;
            if (t < ray.farT() && t > ray.nearT()) {
                ray.setFarT(t);
                data.primitive = this;
                data.as<SphereIntersection>()->backSide = false;
                return true;
            }
            t = -B + det;
            if (t < ray.farT() && t > ray.nearT()) {
                ray.setFarT(t);
                data.primitive = this;
                data.as<SphereIntersection>()->backSide = true;
                return true;
            }
        }

        return false;
    }

    virtual bool occluded(const Ray &ray) const
    {
        Vec3f p = ray.pos() - _pos;
        float B = p.dot(ray.dir());
        float C = p.lengthSq() - _radius*_radius;
        float detSq = B*B - C;
        if (detSq >= 0.0f) {
            float det = std::sqrt(detSq);
            float t = -B - det;
            if (t < ray.farT() && t > ray.nearT())
                return true;
            t = -B + det;
            if (t < ray.farT() && t > ray.nearT())
                return true;
        }

        return false;
    }

    virtual bool hitBackside(const IntersectionTemporary &data) const
    {
        return data.as<SphereIntersection>()->backSide;
    }

    virtual void intersectionInfo(const IntersectionTemporary &/*data*/, IntersectionInfo &info) const
    {
        info.Ns = info.Ng = (info.p - _pos)/_radius;
        Vec3f localN = _invRot.transformVector(info.Ng);
        info.uv = Vec2f(std::atan2(localN.y(), localN.x())*INV_TWO_PI + 0.5f, std::acos(localN.z())*INV_PI);
        info.primitive = this;
    }

    virtual bool tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &info, Vec3f &T, Vec3f &B) const
    {
        Vec3f localN = _invRot.transformVector(info.Ng);
        T = _rot.transformVector(Vec3f(-localN.y(), localN.x(), localN.z()));
        B = info.Ns.cross(T);
        return true;
    }

    virtual bool isSamplable() const
    {
        return true;
    }

    virtual void makeSamplable()
    {
    }

    virtual float inboundPdf(const IntersectionTemporary &/*data*/, const Vec3f &p, const Vec3f &/*d*/) const
    {
        float dist = (_pos - p).length();
        float cosTheta = std::sqrt(max(dist*dist - _radius*_radius, 0.0f))/dist;
        return Sample::uniformSphericalCapPdf(cosTheta);
    }

    virtual bool sampleInboundDirection(LightSample &sample) const
    {
        Vec3f L = _pos - sample.p;
        float d = L.length();
        L.normalize();
        float cosTheta = std::sqrt(max(d*d - _radius*_radius, 0.0f))/d;
        sample.d = Sample::uniformSphericalCap(sample.sampler->next2D(), cosTheta);

        TangentFrame frame(L);
        sample.dist = d;
        sample.d = frame.toGlobal(sample.d);
        sample.pdf = Sample::uniformSphericalCapPdf(cosTheta);

        return true;
    }

    virtual bool sampleOutboundDirection(LightSample &sample) const
    {
        sample.p = Sample::uniformSphere(sample.sampler->next2D());
        sample.d = Sample::cosineHemisphere(sample.sampler->next2D());
        sample.pdf = Sample::cosineHemispherePdf(sample.d)/(FOUR_PI*_radius*_radius);
        TangentFrame frame(sample.p);
        sample.d = frame.toGlobal(sample.d);
        sample.p = sample.p*_radius + _pos;

        return true;
    }

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const
    {
        float phi = uv.x()*TWO_PI - PI;
        float theta = uv.y()*PI;
        Vec3f localPos = Vec3f(std::cos(phi)*std::sin(theta), std::sin(phi)*std::sin(theta), std::cos(theta));
        pos = _rot.transformVector(localPos*_radius) + _pos;
        return true;
    }

    virtual bool isDelta() const
    {
        return false;
    }

    virtual bool isInfinite() const
    {
        return false;
    }

    float solidAngle(const Vec3f &p) const
    {
        Vec3f L = _pos - p;
        float d = L.length();
        float cosTheta = std::sqrt(max(d*d - _radius*_radius, 0.0f))/d;

        return TWO_PI*(1.0f - cosTheta);
    }

    virtual float approximateRadiance(const Vec3f &p) const override final
    {
        if (!isEmissive())
            return 0.0f;
        return solidAngle(p)*_emission->average().max();
    }

    virtual Box3f bounds() const
    {
        return Box3f(_pos - _radius, _pos + _radius);
    }

    void buildProxy()
    {
        _proxy = std::make_shared<TriangleMesh>(std::vector<Vertex>(), std::vector<TriangleI>(), _bsdf, "Sphere", false);
        _proxy->makeSphere(1.0f);
    }

    virtual const TriangleMesh &asTriangleMesh()
    {
        if (!_proxy)
            buildProxy();
        return *_proxy;
    }

    virtual void prepareForRender()
    {
        _pos = _transform*Vec3f(0.0f);
        _radius = (_transform.extractScale()*Vec3f(1.0f)).max();
        _rot = _transform.extractRotation();
        _invRot = _rot.transpose();
    }

    virtual void cleanupAfterRender()
    {
    }

    float area() const override
    {
        return _radius*_radius*FOUR_PI;
    }

    virtual Primitive *clone()
    {
        return new Sphere(*this);
    }

    const Vec3f &pos() const
    {
        return _pos;
    }

    float radius() const
    {
        return _radius;
    }
};

}



#endif /* SPHERE_HPP_ */

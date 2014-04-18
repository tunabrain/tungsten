#ifndef QUADLIGHT_HPP_
#define QUADLIGHT_HPP_

#include "Light.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten
{

class Scene;

class QuadLight : public Light
{
    Vec3f _base;
    Vec3f _edge0, _edge1;
    Vec3f _n;
    float _area;

    Vec3f _emission;

    float intersectRay(const Vec3f &pos, const Vec3f &dir) const
    {
        return _n.dot(_base - pos)/_n.dot(dir);
    }

    bool hitpointInQuad(const Vec3f &p) const
    {
        Vec3f v = p - _base;
        float l0 = v.dot(_edge0);
        float l1 = v.dot(_edge1);

        return l0 >= 0.0f && l1 >= 0.0f && l0 <= _edge0.lengthSq() && l1 <= _edge1.lengthSq();
    }

public:
    QuadLight(const Vec3f &emission)
    : _emission(emission)
    {
    }

    QuadLight(const rapidjson::Value &v, const Scene &/*scene*/)
    : Light(v),
      _emission(JsonUtils::fromJsonMember<float, 3>(v, "emission"))
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Light::toJson(allocator);
        v.AddMember("type", "quad", allocator);
        v.AddMember("emission", JsonUtils::toJsonValue<float, 3>(_emission, allocator), allocator);
        return std::move(v);
    }

    virtual void prepareForRender() final override
    {
        _base = _transform*Vec3f(0.0f);
        _edge0 = _transform.transformVector(Vec3f(1.0f, 0.0f, 0.0f));
        _edge1 = _transform.transformVector(Vec3f(0.0f, 0.0f, 1.0f));
        _base -= _edge0*0.5f;
        _base -= _edge1*0.5f;

        _n = _edge0.cross(_edge1);
        _area = _n.length();
        _n /= _area;
    }

    virtual void buildProxy() final override
    {
        std::vector<Vertex> verts{{
            Vec3f(-0.5f, 0.0f, -0.5f),
            Vec3f( 0.5f, 0.0f, -0.5f),
            Vec3f( 0.5f, 0.0f,  0.5f),
            Vec3f(-0.5f, 0.0f,  0.5f),
        }};
        std::vector<TriangleI> tris{{
            TriangleI{0, 1, 2},
            TriangleI{0, 2, 3}
        }};

        _proxy.reset(new TriangleMesh(std::move(verts), std::move(tris), nullptr, "QuadLight", false));
    }

    std::shared_ptr<Light> clone() const override final
    {
        return std::make_shared<QuadLight>(*this);
    }

    bool isDelta() const override final
    {
        return false;
    }

    bool intersect(const Vec3f &p, const Vec3f &w, float &t, Vec3f &q) const override final
    {
        if (w.dot(_n) >= 0.0f)
            return false;

        t = intersectRay(p, w);
        if (t < 0.0f)
            return false;

        q = p + t*w;
        if (!hitpointInQuad(q))
            return false;

        return true;
    }

    bool sample(LightSample &sample) const override final
    {
        if (_n.dot(sample.p - _base) < 0.0f)
            return false;

        sample.q = _base + sample.xi.x()*_edge0 + sample.xi.y()*_edge1;
        sample.w = sample.q - sample.p;
        float rSq = sample.w.lengthSq();
        sample.r = std::sqrt(rSq);
        sample.w /= sample.r;
        float cosTheta = -_n.dot(sample.w);
        sample.pdf = rSq/(cosTheta*_area);
        sample.L = _emission;
        return true;
    }

    Vec3f eval(const Vec3f &/*w*/) const override final
    {
        return _emission;
    }

    float pdf(const Vec3f &p, const Vec3f &/*n*/, const Vec3f &w) const override final
    {
        float cosTheta = _n.dot(-w);
        float t = intersectRay(p, w);

        return t*t/(cosTheta*_area);
    }

    Vec3f approximateIrradiance(const Vec3f &p, const Vec3f &/*n*/) const override final
    {
        return approximateRadiance(p);
    }

    Vec3f approximateRadiance(const Vec3f &p) const override final
    {
        Vec3f R0 = _base - p;

        if (R0.dot(_n) >= 0.0f)
            return Vec3f(0.0f);

        Vec3f R1 = R0 + _edge0;
        Vec3f R2 = R1 + _edge1;
        Vec3f R3 = R0 + _edge1;
        Vec3f n0 = R0.cross(R1).normalized();
        Vec3f n1 = R1.cross(R2).normalized();
        Vec3f n2 = R2.cross(R3).normalized();
        Vec3f n3 = R3.cross(R0).normalized();
        float Q =
              std::acos(n0.dot(n1))
            + std::acos(n1.dot(n2))
            + std::acos(n2.dot(n3))
            + std::acos(n3.dot(n0));

        return std::abs(Q)*_emission;
    }
};

}



#endif /* QUADLIGHT_HPP_ */

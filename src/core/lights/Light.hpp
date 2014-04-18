#ifndef LIGHT_HPP_
#define LIGHT_HPP_

#include "sampling/LightSample.hpp"

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

#include "Entity.hpp"

#include <memory>

namespace Tungsten
{

class Light : public Entity
{
protected:
    std::unique_ptr<TriangleMesh> _proxy;

    virtual void buildProxy() = 0;

public:
    virtual ~Light()
    {
    }

    Light(const Light &l)
    : Entity(l)
    {
    }

    Light()
    {
    }

    Light(const rapidjson::Value &v)
    : Entity(v)
    {
    }

    rapidjson::Value toJson(Allocator &allocator) const
    {
        return std::move(Entity::toJson(allocator));
    }

    const TriangleMesh &asTriangleMesh()
    {
        if (!_proxy)
            buildProxy();
        return *_proxy;
    }

    virtual std::shared_ptr<Light> clone() const = 0;
    virtual bool isDelta() const = 0;
    virtual bool intersect(const Vec3f &p, const Vec3f &wi, float &t, Vec3f &q) const = 0;
    virtual bool sample(LightSample &sample) const = 0;
    virtual Vec3f eval(const Vec3f &w) const = 0;
    virtual float pdf(const Vec3f &p, const Vec3f &n, const Vec3f &w) const = 0;

    virtual Vec3f approximateIrradiance(const Vec3f &p, const Vec3f &n) const = 0;
    virtual Vec3f approximateRadiance(const Vec3f &p) const = 0;
};

}


#endif /* LIGHT_HPP_ */

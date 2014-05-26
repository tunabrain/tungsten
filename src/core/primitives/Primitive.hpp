#ifndef PRIMITIVE_HPP_
#define PRIMITIVE_HPP_

#include "materials/Texture.hpp"

#include "sampling/LightSample.hpp"

#include "math/Mat4f.hpp"
#include "math/Ray.hpp"
#include "math/Box.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include <memory>

namespace Tungsten {

class TriangleMesh;
class Primitive;
class Bsdf;

struct IntersectionInfo
{
    Vec3f Ng;
    Vec3f Ns;
    Vec3f p;
    Vec3f w;
    Vec2f uv;

    const Primitive *primitive;
};

class Primitive : public JsonSerializable
{
protected:
    std::shared_ptr<Bsdf> _bsdf;
    std::shared_ptr<TextureRgb> _emission;

    Mat4f _transform;

public:
    struct IntersectionTemporary
    {
        const Primitive *primitive;
        uint8 data[64];

        IntersectionTemporary()
        : primitive(nullptr)
        {
        }

        template<typename T>
        T *as()
        {
            return reinterpret_cast<T *>(&data[0]);
        }
        template<typename T>
        const T *as() const
        {
            return reinterpret_cast<const T *>(&data[0]);
        }
    };

    virtual ~Primitive() {}

    Primitive() = default;

    Primitive(const std::string &name, std::shared_ptr<Bsdf> bsdf)
    : JsonSerializable(name), _bsdf(bsdf)
    {
    }

    void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const = 0;
    virtual bool occluded(const Ray &ray) const = 0;
    virtual bool hitBackside(const IntersectionTemporary &data) const = 0;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const = 0;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const = 0;

    virtual bool isSamplable() const = 0;
    virtual void makeSamplable() = 0;

    virtual float inboundPdf(const IntersectionTemporary &data, const Vec3f &p, const Vec3f &d) const = 0;
    virtual bool sampleInboundDirection(LightSample &sample) const = 0;
    virtual bool sampleOutboundDirection(LightSample &sample) const = 0;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const = 0;


    bool isEmissive() const
    {
        return _emission.operator bool();
    }

    Vec3f emission(const IntersectionTemporary &data, const IntersectionInfo &info) const
    {
        if (!_emission)
            return Vec3f(0.0f);
        if (hitBackside(data))
            return Vec3f(0.0f);
        return (*_emission)[info.uv];
    }

    virtual bool disableReflectedEmission() const
    {
        return false;
    }

    void setEmission(const std::shared_ptr<TextureRgb> &emission)
    {
        _emission = emission;
    }

    virtual bool isDelta() const = 0;
    virtual bool isInfinite() const = 0;

    virtual float approximateRadiance(const Vec3f &p) const = 0;

    virtual Box3f bounds() const = 0;

    virtual const TriangleMesh &asTriangleMesh() = 0;

    virtual void prepareForRender() = 0;
    virtual void cleanupAfterRender() = 0;

    virtual float area() const = 0;

    virtual Primitive *clone() = 0;

    virtual void saveData() const {} /* TODO */

    void setTransform(const Mat4f &m)
    {
        _transform = m;
    }

    const Mat4f &transform() const
    {
        return _transform;
    }

    std::shared_ptr<Bsdf> &bsdf()
    {
        return _bsdf;
    }

    const std::shared_ptr<Bsdf> &bsdf() const
    {
        return _bsdf;
    }

    const std::shared_ptr<TextureRgb> &emission() const
    {
        return _emission;
    }
};

}

#endif /* PRIMITIVE_HPP_ */

#ifndef PRIMITIVE_HPP_
#define PRIMITIVE_HPP_

#include "IntersectionTemporary.hpp"
#include "IntersectionInfo.hpp"

#include "samplerecords/LightSample.hpp"

#include "materials/Texture.hpp"

#include "bsdfs/Bsdf.hpp"

#include "math/TangentFrame.hpp"
#include "math/Mat4f.hpp"
#include "math/Ray.hpp"
#include "math/Box.hpp"

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

#include <memory>

namespace Tungsten {

class TriangleMesh;

class Primitive : public JsonSerializable
{
protected:
    std::shared_ptr<Texture> _emission;
    std::shared_ptr<Texture> _bump;
    float _bumpStrength;

    Mat4f _transform;

    bool _needsRayTransform = false;

public:
    virtual ~Primitive() {}

    Primitive();
    Primitive(const std::string &name);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool intersect(Ray &ray, IntersectionTemporary &data) const = 0;
    virtual bool occluded(const Ray &ray) const = 0;
    virtual bool hitBackside(const IntersectionTemporary &data) const = 0;
    virtual void intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const = 0;
    virtual bool tangentSpace(const IntersectionTemporary &data, const IntersectionInfo &info,
            Vec3f &T, Vec3f &B) const = 0;

    virtual bool isSamplable() const = 0;
    virtual void makeSamplable() = 0;

    virtual float inboundPdf(const IntersectionTemporary &data, const IntersectionInfo &info,
            const Vec3f &p, const Vec3f &d) const = 0;
    virtual bool sampleInboundDirection(LightSample &sample) const = 0;
    virtual bool sampleOutboundDirection(LightSample &sample) const = 0;

    virtual bool invertParametrization(Vec2f uv, Vec3f &pos) const = 0;

    virtual bool isDelta() const = 0;
    virtual bool isInfinite() const = 0;

    virtual float approximateRadiance(const Vec3f &p) const = 0;

    virtual Box3f bounds() const = 0;

    virtual const TriangleMesh &asTriangleMesh() = 0;

    virtual void prepareForRender() = 0;
    virtual void cleanupAfterRender() = 0;

    virtual int numBsdfs() const = 0;
    virtual std::shared_ptr<Bsdf> &bsdf(int index) = 0;

    virtual Primitive *clone() = 0;

    virtual void saveData() {}

    void setupTangentFrame(const IntersectionTemporary &data,
            const IntersectionInfo &info, TangentFrame &dst) const;

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

    void setEmission(const std::shared_ptr<Texture> &emission)
    {
        _emission = emission;
    }

    const std::shared_ptr<Texture> &emission() const
    {
        return _emission;
    }

    bool needsRayTransform() const
    {
        return _needsRayTransform;
    }

    void setTransform(const Mat4f &m)
    {
        _transform = m;
    }

    const Mat4f &transform() const
    {
        return _transform;
    }

    void setBump(const std::shared_ptr<Texture> &b)
    {
        _bump = b;
    }

    std::shared_ptr<Texture> &bump()
    {
        return _bump;
    }

    const std::shared_ptr<Texture> &bump() const
    {
        return _bump;
    }
};

}

#endif /* PRIMITIVE_HPP_ */

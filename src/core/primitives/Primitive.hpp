#ifndef PRIMITIVE_HPP_
#define PRIMITIVE_HPP_

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
    std::shared_ptr<Bsdf> _bsdf;
    std::shared_ptr<TextureRgb> _emission;
    std::shared_ptr<TextureA> _bump;
    float _bumpStrength;

    Mat4f _transform;

    bool _needsRayTransform = false;

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
            static_assert(sizeof(T) <= sizeof(data), "Exceeding size of intersection temporary");
            return reinterpret_cast<T *>(&data[0]);
        }
        template<typename T>
        const T *as() const
        {
            static_assert(sizeof(T) <= sizeof(data), "Exceeding size of intersection temporary");
            return reinterpret_cast<const T *>(&data[0]);
        }
    };

    virtual ~Primitive() {}

    Primitive() = default;

    Primitive(const std::string &name, std::shared_ptr<Bsdf> bsdf)
    : JsonSerializable(name), _bsdf(bsdf),
      _bumpStrength(1.0f)
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

    void setupTangentFrame(const Primitive::IntersectionTemporary &data,
            const IntersectionInfo &info, TangentFrame &dst) const
    {
        if ((!_bump || _bump->isConstant()) && !_bsdf->flags().isAnisotropic()) {
            dst = TangentFrame(info.Ns);
            return;
        }
        Vec3f T, B, N(info.Ns);
        if (!tangentSpace(data, info, T, B)) {
            dst = TangentFrame(info.Ns);
            return;
        }
        if (_bump && !_bump->isConstant()) {
            Vec2f dudv;
            _bump->derivatives(info.uv, dudv);

            T += info.Ns*(dudv.x()*_bumpStrength - info.Ns.dot(T));
            B += info.Ns*(dudv.y()*_bumpStrength - info.Ns.dot(B));
            N = T.cross(B);
            if (N == 0.0f) {
                dst = TangentFrame(info.Ns);
                return;
            }
            if (N.dot(info.Ns) < 0.0f)
                N = -N;
            N.normalize();
        }
        T = (T - N.dot(T)*N);
        if (T == 0.0f) {
            dst = TangentFrame(info.Ns);
            return;
        }
        T.normalize();
        B = N.cross(T);

        dst = TangentFrame(N, T, B);
    }

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

    virtual Primitive *clone() = 0;

    virtual void saveData() const {} /* TODO */

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

    void setBump(const std::shared_ptr<TextureA> &b)
    {
        _bump = b;
    }

    std::shared_ptr<TextureA> &bump()
    {
        return _bump;
    }

    const std::shared_ptr<TextureA> &bump() const
    {
        return _bump;
    }
};

}

#endif /* PRIMITIVE_HPP_ */

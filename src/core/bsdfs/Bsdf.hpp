#ifndef BSDF_HPP_
#define BSDF_HPP_

#include <rapidjson/document.h>

#include "sampling/ScatterEvent.hpp"

#include "math/Vec.hpp"

#include "io/JsonSerializable.hpp"

namespace Tungsten
{

class BsdfFlags
{
    uint32 _flags;

public:
    enum
    {
        GlossyFlag       = (1 << 0),
        DiffuseFlag      = (1 << 1),
        SpecularFlag     = (1 << 2),
        TransmissiveFlag = (1 << 3),
    };

    BsdfFlags(uint32 flags)
    : _flags(flags)
    {
    }

    BsdfFlags(const BsdfFlags &a, const BsdfFlags &b)
    : _flags(0)
    {
        if (a.isGlossy() || b.isGlossy())
            _flags |= GlossyFlag;
        if (a.isDiffuse() || b.isDiffuse())
            _flags |= DiffuseFlag;
        if (a.isSpecular() && b.isSpecular())
            _flags |= SpecularFlag;
        if (a.isTransmissive() || b.isTransmissive())
            _flags |= TransmissiveFlag;
    }

    bool isGlossy() const
    {
        return (_flags & GlossyFlag) != 0;
    }

    bool isDiffuse() const
    {
        return (_flags & DiffuseFlag) != 0;
    }

    bool isSpecular() const
    {
        return (_flags & SpecularFlag) != 0;
    }

    bool isTransmissive() const
    {
        return (_flags & TransmissiveFlag) != 0;
    }
};

class Bsdf : public JsonSerializable
{
protected:
    BsdfFlags _flags;

public:
    virtual ~Bsdf()
    {
    }

    Bsdf()
    : _flags(0)
    {
    }

    Bsdf(const rapidjson::Value &v)
    : JsonSerializable(v),
      _flags(0)
    {
    }

    rapidjson::Value toJson(Allocator &allocator) const
    {
        return std::move(JsonSerializable::toJson(allocator));
    }

    virtual bool sample         (ScatterEvent &event) const = 0;
    virtual bool sampleFromLight(ScatterEvent &event) const = 0;

    virtual Vec3f eval(const Vec3f &wo, const Vec3f &wi) const = 0;

    virtual float pdf         (const Vec3f &wo, const Vec3f &wi) const = 0;
    virtual float pdfFromLight(const Vec3f &wo, const Vec3f &wi) const = 0;

    const BsdfFlags &flags() const
    {
        return _flags;
    }
};

}

#endif /* BSDF_HPP_ */

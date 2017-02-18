#include "Primitive.hpp"

#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/Bsdf.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

namespace Tungsten {

std::shared_ptr<Bsdf> Primitive::_defaultBsdf = std::make_shared<LambertBsdf>();

Primitive::Primitive()
{
}

Primitive::Primitive(const std::string &name)
: JsonSerializable(name)
{
}

void Primitive::fromJson(JsonPtr value, const Scene &scene)
{
    JsonSerializable::fromJson(value, scene);
    value.getField("transform", _transform);

    if (auto emission = value["emission"]) _emission = scene.fetchTexture(emission, TexelConversion::REQUEST_RGB);
    if (auto power    = value["power"   ]) _power    = scene.fetchTexture(power,    TexelConversion::REQUEST_RGB);

    if (auto intMedium  = value["int_medium"]) _intMedium = scene.fetchMedium(intMedium);
    if (auto extMedium  = value["ext_medium"]) _extMedium = scene.fetchMedium(extMedium);
}

rapidjson::Value Primitive::toJson(Allocator &allocator) const
{
    JsonObject result{JsonSerializable::toJson(allocator), allocator,
        "transform", _transform
    };
    if (_power)
        result.add("power", *_power);
    else if (_emission)
        result.add("emission", *_emission);
    if (_intMedium)
        result.add("int_medium", *_intMedium);
    if (_extMedium)
        result.add("ext_medium", *_extMedium);

    return result;
}

bool Primitive::samplePosition(PathSampleGenerator &/*sampler*/, PositionSample &/*sample*/) const
{
    return false;
}

bool Primitive::sampleDirection(PathSampleGenerator &/*sampler*/, const PositionSample &/*point*/, DirectionSample &/*sample*/) const
{
    return false;
}

bool Primitive::sampleDirect(uint32 /*threadIndex*/, const Vec3f &/*p*/, PathSampleGenerator &/*sampler*/, LightSample &/*sample*/) const
{
    return false;
}

bool Primitive::invertPosition(WritablePathSampleGenerator &/*sampler*/, const PositionSample &/*point*/) const
{
    FAIL("Primitive::invertPosition not implemented!");
}
bool Primitive::invertDirection(WritablePathSampleGenerator &/*sampler*/, const PositionSample &/*point*/,
        const DirectionSample &/*direction*/) const
{
    FAIL("Primitive::invertDirection not implemented!");
}

float Primitive::positionalPdf(const PositionSample &/*point*/) const
{
    return 0.0f;
}

float Primitive::directionalPdf(const PositionSample &/*point*/, const DirectionSample &/*sample*/) const
{
    return 0.0f;
}

float Primitive::directPdf(uint32 /*threadIndex*/, const IntersectionTemporary &/*data*/,
        const IntersectionInfo &/*info*/, const Vec3f &/*p*/) const
{
    return 0.0f;
}

Vec3f Primitive::evalPositionalEmission(const PositionSample &/*sample*/) const
{
    return Vec3f(0.0f);
}

Vec3f Primitive::evalDirectionalEmission(const PositionSample &/*point*/, const DirectionSample &/*sample*/) const
{
    return Vec3f(0.0f);
}

Vec3f Primitive::evalDirect(const IntersectionTemporary &data, const IntersectionInfo &info) const
{
    if (!_emission)
        return Vec3f(0.0f);
    if (hitBackside(data))
        return Vec3f(0.0f);
    return (*_emission)[info];
}

void Primitive::prepareForRender()
{
    if (_power) {
        _emission = std::shared_ptr<Texture>(_power->clone());
        _emission->scaleValues(powerToRadianceFactor());
    }
}

void Primitive::teardownAfterRender()
{
    if (_power)
        _emission.reset();
}

void Primitive::setupTangentFrame(const IntersectionTemporary &data,
        const IntersectionInfo &info, TangentFrame &dst) const
{
    const Texture *bump = info.bsdf ? info.bsdf->bump().get() : nullptr;

    if ((!bump || bump->isConstant()) && !info.bsdf->lobes().isAnisotropic()) {
        dst = TangentFrame(info.Ns);
        return;
    }
    Vec3f T, B, N(info.Ns);
    if (!tangentSpace(data, info, T, B)) {
        dst = TangentFrame(info.Ns);
        return;
    }
    if (bump && !bump->isConstant()) {
        Vec2f dudv;
        bump->derivatives(info.uv, dudv);

        T += info.Ns*(dudv.x() - info.Ns.dot(T));
        B += info.Ns*(dudv.y() - info.Ns.dot(B));
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

}

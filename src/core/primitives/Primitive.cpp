#include "Primitive.hpp"

#include "bsdfs/Bsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

Primitive::Primitive()
: _bumpStrength(1.0f)
{
}

Primitive::Primitive(const std::string &name)
: JsonSerializable(name),
  _bumpStrength(1.0f)
{
}

void Primitive::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);
    JsonUtils::fromJson(v, "transform", _transform);
    JsonUtils::fromJson(v, "bump_strength", _bumpStrength);

    scene.textureFromJsonMember(v, "emission", TexelConversion::REQUEST_RGB, _emission);
    scene.textureFromJsonMember(v, "power", TexelConversion::REQUEST_RGB, _power);
    scene.textureFromJsonMember(v, "bump", TexelConversion::REQUEST_AVERAGE, _bump);
}

rapidjson::Value Primitive::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("transform", JsonUtils::toJsonValue(_transform, allocator), allocator);
    v.AddMember("bump_strength", JsonUtils::toJsonValue(_bumpStrength, allocator), allocator);
    if (_power)
        JsonUtils::addObjectMember(v, "power", *_power, allocator);
    else if (_emission)
        JsonUtils::addObjectMember(v, "emission", *_emission, allocator);
    if (_bump)
        JsonUtils::addObjectMember(v, "bump", *_bump,  allocator);
    return std::move(v);
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
    if ((!_bump || _bump->isConstant()) && !info.bsdf->lobes().isAnisotropic()) {
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

}

#include "Primitive.hpp"

#include "bsdfs/Bsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

Primitive::Primitive()
: _bumpStrength(1.0f)
{
}

Primitive::Primitive(const std::string &name, std::shared_ptr<Bsdf> bsdf)
: JsonSerializable(name), _bsdf(bsdf),
  _bumpStrength(1.0f)
{
}

void Primitive::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);
    JsonUtils::fromJson(v, "transform", _transform);
    JsonUtils::fromJson(v, "bump_strength", _bumpStrength);
    _bsdf = scene.fetchBsdf(JsonUtils::fetchMember(v, "bsdf"));

    scene.textureFromJsonMember(v, "emission", TexelConversion::REQUEST_RGB, _emission);
    scene.textureFromJsonMember(v, "bump", TexelConversion::REQUEST_AVERAGE, _bump);
}

rapidjson::Value Primitive::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    v.AddMember("transform", JsonUtils::toJsonValue(_transform, allocator), allocator);
    v.AddMember("bump_strength", JsonUtils::toJsonValue(_bumpStrength, allocator), allocator);
    JsonUtils::addObjectMember(v, "bsdf", *_bsdf, allocator);
    if (_emission)
        JsonUtils::addObjectMember(v, "emission", *_emission, allocator);
    if (_bump)
        JsonUtils::addObjectMember(v, "bump", *_bump,  allocator);
    return std::move(v);
}

void Primitive::setupTangentFrame(const IntersectionTemporary &data,
        const IntersectionInfo &info, TangentFrame &dst) const
{
    if ((!_bump || _bump->isConstant()) && !_bsdf->lobes().isAnisotropic()) {
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

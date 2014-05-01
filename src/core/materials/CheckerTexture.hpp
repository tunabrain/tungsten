#ifndef CHECKERTEXTURE_HPP_
#define CHECKERTEXTURE_HPP_

#include "Texture.hpp"

#include "math/MathUtil.hpp"

namespace Tungsten
{

template<bool Scalar>
class CheckerTexture : public Texture<Scalar, 2>
{
    typedef JsonSerializable::Allocator Allocator;
    typedef typename Texture<Scalar, 2>::Value Value;

    Value _onColor, _offColor;
    int _resU, _resV;

public:
    CheckerTexture()
    : _onColor(0.8f), _offColor(0.2f),
      _resU(20), _resV(20)
    {
    }

    void fromJson(const rapidjson::Value &v, const Scene &/*scene*/) override final
    {
        JsonUtils::fromJson(v, "onColor", _onColor);
        JsonUtils::fromJson(v, "offColor", _offColor);
        JsonUtils::fromJson(v, "resU", _resU);
        JsonUtils::fromJson(v, "resV", _resV);
    }

    rapidjson::Value toJson(Allocator &allocator) const override final
    {
        rapidjson::Value v = Texture<Scalar, 2>::toJson(allocator);
        v.AddMember("type", "checker", allocator);
        v.AddMember("onColor",  JsonUtils::toJsonValue(_onColor,  allocator), allocator);
        v.AddMember("offColor", JsonUtils::toJsonValue(_offColor, allocator), allocator);
        v.AddMember("resU", _resU, allocator);
        v.AddMember("resV", _resV, allocator);
        return std::move(v);
    }

    void derivatives(const Vec<float, 2> &/*uv*/, Vec<Value, 2> &derivs) const override final
    {
        derivs = Vec<Value, 2>(Value(0.0f));
    }

    bool isConstant() const override final
    {
        return false;
    }

    Value average() const override final
    {
        return (_onColor + _offColor)*0.5f;
    }

    Value minimum() const override final
    {
        return min(_onColor, _offColor);
    }

    Value maximum() const override final
    {
        return max(_onColor, _offColor);
    }

    Value operator[](const Vec<float, 2> &uv) const override final
    {
        Vec2i uvI(uv*Vec2f(float(_resU), float(_resV)));
        bool on = (uvI.x() ^ uvI.y()) & 1;
        return on ? _onColor : _offColor;
    }
};

}

#endif /* CHECKERTEXTURE_HPP_ */

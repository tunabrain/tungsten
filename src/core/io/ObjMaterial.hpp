#ifndef OBJMATERIAL_HPP_
#define OBJMATERIAL_HPP_

#include <string>

#include "math/Vec.hpp"

namespace Tungsten
{

struct ObjMaterial
{
    std::string name;

    Vec3f diffuse;
    Vec3f specular;
    Vec3f emission;
    Vec3f opacity;
    float hardness;
    float ior;
    std::string diffuseMap;
    std::string alphaMap;
    std::string bumpMap;

    ObjMaterial()
    : opacity(1.0f), hardness(0.0f), ior(1.0f)
    {
    }

    ObjMaterial(const std::string &name_)
    : name(name_), opacity(1.0f), hardness(0.0f), ior(1.0f)
    {
    }

    bool isTransmissive() const
    {
        return opacity.max() < 1.0f;
    }

    bool isSpecular() const
    {
        return specular.max() > 0.0f;
    }

    bool isEmissive() const
    {
        return emission.max() > 0.0f;
    }

    bool hasDiffuseMap() const
    {
        return !diffuseMap.empty();
    }

    bool hasAlphaMap() const
    {
        return !alphaMap.empty();
    }

    bool hasBumpMap() const
    {
        return !bumpMap.empty();
    }
};

}


#endif /* OBJMATERIAL_HPP_ */

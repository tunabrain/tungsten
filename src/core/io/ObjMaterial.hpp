#ifndef OBJMATERIAL_HPP_
#define OBJMATERIAL_HPP_

#include "math/Vec.hpp"

#include <string>

namespace Tungsten {

struct ObjMaterial
{
    std::string name;

    Vec3f diffuse = Vec3f(0.0f);
    Vec3f specular = Vec3f(0.0f);
    Vec3f emission = Vec3f(0.0f);
    Vec3f opacity = Vec3f(1.0f);
    float hardness = 0.0f;
    float ior = 1.0f;
    std::string diffuseMap;
    std::string alphaMap;
    std::string bumpMap;

    ObjMaterial(const std::string &name_)
    : name(name_)
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

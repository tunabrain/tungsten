#ifndef TONEMAP_HPP_
#define TONEMAP_HPP_

#include "math/MathUtil.hpp"
#include "math/Vec.hpp"

#include "StringableEnum.hpp"

namespace Tungsten {

class Tonemap
{
    enum TypeEnum {
        LinearOnly,
        GammaOnly,
        Reinhard,
        Filmic,
        Pbrt
    };

public:
    typedef StringableEnum<TypeEnum> Type;
    friend Type;

    static inline Vec3f tonemap(TypeEnum type, const Vec3f &c)
    {
        switch (type) {
        case LinearOnly:
            return c;
        case GammaOnly:
            return std::pow(c, 1.0f/2.2f);
        case Reinhard:
            return std::pow(c/(c + 1.0f), 1.0f/2.2f);
        case Filmic: {
            Vec3f x = max(Vec3f(0.0f), c - 0.004f);
            return (x*(6.2f*x + 0.5f))/(x*(6.2f*x + 1.7f) + 0.06f);
        } case Pbrt: {
            Vec3f result;
            for (int i = 0; i < 3; ++i) {
                if (c[i] < 0.0031308f)
                    result[i] = 12.92f*c[i];
                else
                    result[i] = 1.055f*std::pow(c[i], 1.0f/2.4f) - 0.055f;
            }
            return result;
        }}
        return c;
    }
};

}


#endif /* TONEMAP_HPP_ */

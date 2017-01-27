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
        Filmic
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
        }}
        return c;
    }
};

}


#endif /* TONEMAP_HPP_ */

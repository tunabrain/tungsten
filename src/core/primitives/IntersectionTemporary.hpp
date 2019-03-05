#ifndef INTERSECTIONTEMPORARY_HPP_
#define INTERSECTIONTEMPORARY_HPP_

#include "IntTypes.hpp"

namespace Tungsten {

class Primitive;

struct IntersectionTemporary
{
    const Primitive *primitive;
    uint32 flags;
    uint8 data[64];

    IntersectionTemporary() = default;

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

}

#endif /* INTERSECTIONTEMPORARY_HPP_ */

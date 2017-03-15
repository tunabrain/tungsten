
#ifndef MATH_BOX_HPP_
#define MATH_BOX_HPP_

#include "IntTypes.hpp"
#include "MathUtil.hpp"
#include "Range.hpp"

#include "sse/SimdUtils.hpp"

#include <limits>

namespace Tungsten {

template<typename TVec, typename ElementType, unsigned Size>
class Box {
    TVec _min;
    TVec _max;

public:
    Box()
    : _min(std::numeric_limits<ElementType>::max()),
      _max(std::numeric_limits<ElementType>::lowest())
    {
    }

    Box(const TVec &p)
    : _min(p), _max(p)
    {
    }

    Box(const TVec &min, const TVec &max)
    : _min(min), _max(max)
    {
    }

    inline const TVec &min() const
    {
        return _min;
    }

    inline const TVec &max() const
    {
        return _max;
    }

    inline TVec &min()
    {
        return _min;
    }

    inline TVec &max()
    {
        return _max;
    }

    inline TVec center() const
    { 
        return (_min + _max)/ElementType(2);
    }

    inline TVec diagonal() const
    {
        return Tungsten::max(_max - _min, TVec(ElementType(0)));
    }

    inline ElementType area() const
    {
        TVec d(diagonal());
        if (Size == 2)
            return (d[0] + d[1])*ElementType(2);
        else if (Size == 3)
            return (d[0]*d[1] + d[1]*d[2] + d[2]*d[0])*ElementType(2);
        return ElementType(0);
    }

    inline bool empty() const
    {
        for (unsigned i = 0; i < Size; ++i)
            if (_max[i] <= _min[i])
                return true;
        return false;
    }

    void grow(ElementType t)
    {
        _min -= TVec(t);
        _max += TVec(t);
    }

    void grow(const TVec &p)
    {
        _min = Tungsten::min(_min, p);
        _max = Tungsten::max(_max, p);
    }

    void grow(const Box &box)
    {
        _min = Tungsten::min(_min, box._min);
        _max = Tungsten::max(_max, box._max);
    }

    inline bool contains(const TVec &p) const
    {
        for (unsigned i = 0; i < Size; i++)
            if (p[i] < _min[i] || p[i] > _max[i])
                return false;
        return true;
    }

    inline bool contains(const Box &box) const
    {
        for (unsigned i = 0; i < Size; i++)
            if (box._max[i] < _min[i] || box._min[i] > _max[i])
                return false;
        return true;
    }

    void intersect(const Box &box)
    {
        _min = Tungsten::max(_min, box._min);
        _max = Tungsten::min(_max, box._max);
    }

    Range<ElementType> range(int dimension) const
    {
        return Range<ElementType>(_min[dimension], _max[dimension]);
    }

    friend std::ostream &operator<< (std::ostream &stream, const Box &box) {
        stream << '(' << box.min() << " - " << box.max() << ')';
        return stream;
    }
};

typedef Box<Vec4f, float, 4> Box4f;
typedef Box<Vec3f, float, 3> Box3f;
typedef Box<Vec2f, float, 2> Box2f;

typedef Box<Vec4u, uint32, 4> Box4u;
typedef Box<Vec3u, uint32, 3> Box3u;
typedef Box<Vec2u, uint32, 2> Box2u;

typedef Box<Vec4i, int32, 4> Box4i;
typedef Box<Vec3i, int32, 3> Box3i;
typedef Box<Vec2i, int32, 2> Box2i;

typedef Box<Vec4c, uint8, 4> Box4c;
typedef Box<Vec3c, uint8, 3> Box3c;
typedef Box<Vec2c, uint8, 2> Box2c;

typedef Box<Vec4fp, float, 4> Box4fp;
typedef Box<Vec3fp, float, 3> Box3fp;
typedef Box<Vec2fp, float, 2> Box2fp;

inline Box3fp expand(const Box3f &b)
{
    return Box3fp(expand(b.min()), expand(b.max()));
}

inline Box3f narrow(const Box3fp &b)
{
    return Box3f(narrow(b.min()), narrow(b.max()));
}

}

#endif // MATH_BOX_HPP_

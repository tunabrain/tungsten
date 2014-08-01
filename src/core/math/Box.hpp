
#ifndef MATH_BOX_HPP_
#define MATH_BOX_HPP_

#include "IntTypes.hpp"
#include "MathUtil.hpp"

#include <limits>

namespace Tungsten {

template<typename ElementType, unsigned Size>
class Box {
    typedef Vec<ElementType, Size> TVec;

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

    const TVec &min() const
    {
        return _min;
    }

    const TVec &max() const
    {
        return _max;
    }

    TVec &min()
    {
        return _min;
    }

    TVec &max()
    {
        return _max;
    }

    TVec center() const
    { 
        return (_min + _max)/ElementType(2);
    }

    TVec diagonal() const
    {
        return _max - _min;
    }

    float area() const
    {
        TVec d(diagonal());
        return Tungsten::max(d.x()*d.y() + d.y()*d.z() + d.z()*d.x(), ElementType(0))*ElementType(2);
    }

    bool empty() const
    {
        return diagonal().min() <= ElementType(0);
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

    bool contains(const TVec &p) const
    {
        for (int i = 0; i < TVec::size; i++)
            if (p[i] < _min[i] || p[i] > _max[i])
                return false;
        return true;
    }

    bool contains(const Box &box) const
    {
        for (int i = 0; i < TVec::size; i++)
            if (box._max[i] < _min[i] || box._min[i] > _max[i])
                return false;
        return true;
    }

    void intersect(const Box &box)
    {
        _min = Tungsten::max(_min, box._min);
        _max = Tungsten::min(_max, box._max);
    }

    friend std::ostream &operator<< (std::ostream &stream, const Box &box) {
        stream << '(' << box.min() << " - " << box.max() << ')';
        return stream;
    }
};

typedef Box<float, 4> Box4f;
typedef Box<float, 3> Box3f;
typedef Box<float, 2> Box2f;

typedef Box<uint32, 4> Box4u;
typedef Box<uint32, 3> Box3u;
typedef Box<uint32, 2> Box2u;

typedef Box<int32, 4> Box4i;
typedef Box<int32, 3> Box3i;
typedef Box<int32, 2> Box2i;

typedef Box<uint8, 4> Box4c;
typedef Box<uint8, 3> Box3c;
typedef Box<uint8, 2> Box2c;

}

#endif // MATH_BOX_HPP_

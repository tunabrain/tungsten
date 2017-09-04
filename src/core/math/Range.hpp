#ifndef RANGE_HPP_
#define RANGE_HPP_

namespace Tungsten {

template<typename T> class RangeIterator;

template<typename T>
class Range
{
    T _start, _end, _step;

public:
    Range(T start, T end, T step = T(1))
    : _start(start), _end(end), _step(step)
    {
    }

    RangeIterator<T> begin() const;
    RangeIterator<T> end() const;
};

template<typename T>
Range<T> range(T end)
{
    return Range<T>(T(0), end, T(1));
}

template<typename T>
Range<T> range(T start, T end, T step = T(1))
{
    return Range<T>(start, end, step);
}

template<typename T>
class RangeIterator
{
    T _pos, _step;

public:
    RangeIterator(T pos, T step) : _pos(pos), _step(step) {}

    bool operator!=(const RangeIterator &o) const { return _pos < o._pos; }
    RangeIterator &operator++() { _pos += _step; return *this; }
    RangeIterator operator++(int) { RangeIterator copy(*this); operator++(); return copy; }
    T operator*() const { return _pos; }
};

template<typename T>
RangeIterator<T> Range<T>::begin() const { return RangeIterator<T>(_start, _step); }

template<typename T>
RangeIterator<T> Range<T>::end() const { return RangeIterator<T>(_end, _step); }

}

#endif /* RANGE_HPP_ */
